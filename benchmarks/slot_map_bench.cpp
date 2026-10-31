#ifdef SM_TRACK_MEMORY
#include "allocation_tracker.h"
#endif

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "m_slot_map.h"
#include "slot_map.h"
#include "mesh_element.h"

namespace
{
    using Clock = std::chrono::steady_clock;
    using U64 = std::uint64_t;

    void barrier() { asm volatile("" ::: "memory"); }
    void consume(U64 value) { asm volatile("" : : "g"(value) : "memory"); }
    void require(bool condition, const char *message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    // 使用项目中的真实类型；邻接数组填入占位 handle，只测试存储而不构造网格拓扑。
    template <class T> struct Payload;
    template <> struct Payload<MyMesh::Vertex>
    {
        static constexpr const char *name = "Vertex";
        static MyMesh::Vertex make(std::size_t i)
        {
            MyMesh::Vertex v;
            v.pos = Eigen::Vector3d(double(i + 1), double(i) / 4, -double(i));
            v.outgoing.resize(6 + i % 3);
            return v;
        }
        static U64 score(const MyMesh::Vertex &v) { return U64(v.pos.x()) + v.outgoing.size(); }
        static U64 expected(std::size_t i) { return i + 1 + 6 + i % 3; }
    };
    template <> struct Payload<MyMesh::Dedge>
    {
        static constexpr const char *name = "Dedge";
        static MyMesh::Dedge make(std::size_t i)
        {
            MyMesh::Dedge e;
            e.incident_halffaces.resize(4 + i % 3);
            return e;
        }
        static U64 score(const MyMesh::Dedge &v) { return v.incident_halffaces.size(); }
        static U64 expected(std::size_t i) { return 4 + i % 3; }
    };
    template <> struct Payload<MyMesh::Halfface>
    {
        static constexpr const char *name = "Halfface";
        static MyMesh::Halfface make(std::size_t i)
        {
            MyMesh::Halfface f;
            f.type = i % 2 ? MyMesh::FaceType::QUADRILATERAL : MyMesh::FaceType::TRIANGLE;
            return f;
        }
        static U64 score(const MyMesh::Halfface &v) { return U64(v.type); }
        static U64 expected(std::size_t i) { return 1 + i % 2; }
    };
    template <> struct Payload<MyMesh::Cell>
    {
        static constexpr const char *name = "Cell";
        static MyMesh::Cell make(std::size_t i)
        {
            MyMesh::Cell c;
            c.type = static_cast<MyMesh::CellType>(1 + i % 3);
            return c;
        }
        static U64 score(const MyMesh::Cell &v) { return U64(v.type); }
        static U64 expected(std::size_t i) { return 1 + i % 3; }
    };

    template <class T, bool Custom> struct Backend;
    template <class T> struct Backend<T, true>
    {
        using Container = slotmap::SlotMap<T>;
        using Key = typename Container::KeyType;
        static constexpr const char *name = "custom";
        static U64 scan_keys(Container &storage)
        {
            U64 sum = 0;
            for (auto key : storage.keys())
                sum += Payload<T>::score(*storage.get(key));
            return sum;
        }
        static U64 scan_native(Container &storage) { return scan_keys(storage); }
    };
    template <class T> struct Backend<T, false>
    {
        using Container = dod::slot_map64<T>; // 原始默认参数：4096 槽/页，64 个空槽延迟复用。
        using Key = typename Container::key;
        static constexpr const char *name = "dod";
        static U64 scan_keys(Container &storage)
        {
            U64 sum = 0;
            // 使用非 const items()，避开原实现 const 版本修改 const 子对象的路径。
            // 此循环只读 key/value，不修改容器或元素。
            for (const auto &kv : storage.items())
                sum += Payload<T>::score(*storage.get(kv.first));
            return sum;
        }
        static U64 scan_native(Container &storage)
        {
            U64 sum = 0;
            for (const auto &value : std::as_const(storage))
                sum += Payload<T>::score(value);
            return sum;
        }
    };

    struct Timing
    {
        const char *type;
        const char *backend;
        std::size_t n;
        int trial; // -1 是预热，不输出。

        template <class F>
        void run(const char *operation, U64 work_items, U64 expected, F action) const
        {
            barrier();
            const auto begin = Clock::now();
            const U64 result = action();
            consume(result);
            const auto end = Clock::now();
            require(result == expected, operation);
            const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
            if (trial >= 0)
                std::printf("%s,%s,%zu,%d,%s,%llu,%lld,%.6f,ok\n", type, backend, n, trial, operation,
                            static_cast<unsigned long long>(work_items), static_cast<long long>(ns), double(ns) / work_items);
        }

        void skip(const char *operation) const
        {
            if (trial >= 0)
                std::printf("%s,%s,%zu,%d,%s,0,0,0,skipped_no_correct_dod_comparison\n", type, backend, n, trial, operation);
        }
    };

    template <class T, class Storage, class Keys>
    U64 lookup_sum(const Storage &storage, const Keys &keys)
    {
        U64 sum = 0;
        for (auto key : keys)
            if (const auto *value = storage.get(key))
                sum += Payload<T>::score(*value);
        return sum;
    }

    template <class T, bool Custom>
    void timed_case(std::size_t n, int trial, const std::vector<std::size_t> &order,
                    const std::vector<std::size_t> &query_order,
                    const std::vector<std::size_t> &edits, std::size_t query_budget)
    {
        using B = Backend<T, Custom>;
        using Storage = typename B::Container;
        using Key = typename B::Key;
        Timing timing{Payload<T>::name, B::name, n, trial};
        std::vector<Key> keys(n), random_keys(n);
        std::vector<std::size_t> ids(n);
        std::iota(ids.begin(), ids.end(), 0);
        std::optional<Storage> storage(std::in_place);
        U64 sum = 0;
        for (std::size_t i = 0; i < n; ++i)
            sum += Payload<T>::expected(i);

        timing.run("append", n, n, [&] {
            for (std::size_t i = 0; i < n; ++i)
                keys[i] = storage->emplace(Payload<T>::make(i));
            return storage->size();
        });
        require(lookup_sum<T>(*storage, keys) == sum, "initial content");
        for (std::size_t i = 0; i < n; ++i)
            random_keys[i] = keys[query_order[i]];
        const std::size_t passes = std::max<std::size_t>(1, query_budget / n);
        auto lookup = [&](const auto &queries, bool checked) {
            U64 result = 0;
            for (std::size_t p = 0; p < passes; ++p)
            {
                barrier();
                for (auto key : queries)
                {
                    if (checked && !storage->contains(key))
                        continue;
                    if (const T *value = storage->get(key))
                        result += Payload<T>::score(*value);
                }
            }
            return result;
        };
        timing.run("get_sequential_live", n * passes, sum * passes, [&] { return lookup(keys, false); });
        timing.run("get_random_live", n * passes, sum * passes, [&] { return lookup(random_keys, false); });
        timing.run("contains_then_get_random_live", n * passes, sum * passes, [&] { return lookup(random_keys, true); });

        auto scans = [&](const char *key_name, const char *native_name) {
            const auto repeats = std::max<std::size_t>(1, query_budget / storage->size());
            timing.run(key_name, repeats * storage->size(), repeats * sum, [&] {
                U64 result = 0;
                for (std::size_t p = 0; p < repeats; ++p) { barrier(); result += B::scan_keys(*storage); }
                return result;
            });
            timing.run(native_name, repeats * storage->size(), repeats * sum, [&] {
                U64 result = 0;
                for (std::size_t p = 0; p < repeats; ++p) { barrier(); result += B::scan_native(*storage); }
                return result;
            });
        };
        scans("iterate_keys_dense", "iterate_native_dense");

        std::optional<Storage> copy;
        timing.run("copy_dense", n, n, [&] { copy.emplace(*storage); return copy->size(); });
        require(lookup_sum<T>(*copy, keys) == sum, "dense copy content");
        timing.run("destroy_dense_copy", n, 0, [&] { copy.reset(); return 0; });

        const std::size_t half = n / 2;
        timing.run("erase_random_half", half, n - half, [&] {
            for (std::size_t i = 0; i < half; ++i)
                storage->erase(keys[order[i]]);
            return storage->size();
        });
        for (std::size_t i = 0; i < half; ++i)
            sum -= Payload<T>::expected(ids[order[i]]);
        timing.run("get_random_half_stale", n * passes, sum * passes, [&] { return lookup(random_keys, false); });
        timing.run("contains_random_half_stale", n * passes, (n - half) * passes, [&] {
            U64 found = 0;
            for (std::size_t p = 0; p < passes; ++p)
            {
                barrier();
                for (auto key : random_keys)
                    found += storage->contains(key);
            }
            return found;
        });
        scans("iterate_keys_50pct_live", "iterate_native_50pct_live");
        // dod 的该场景有已确认的复制缺陷，因此双方均不计时；也不在单侧
        // 额外执行一次复制，以免改变后续回填的缓存/分配器状态。
        timing.skip("copy_50pct_live");

        timing.run("refill_deleted_half", half, n, [&] {
            for (std::size_t i = 0; i < half; ++i)
                keys[order[i]] = storage->emplace(Payload<T>::make(n + i));
            return storage->size();
        });
        for (std::size_t i = 0; i < half; ++i)
            ids[order[i]] = n + i;
        timing.run("erase_insert_pair", edits.size(), n, [&] {
            for (std::size_t i = 0; i < edits.size(); ++i)
            {
                const auto index = edits[i];
                storage->erase(keys[index]);
                keys[index] = storage->emplace(Payload<T>::make(2 * n + i));
            }
            return storage->size();
        });
        for (std::size_t i = 0; i < edits.size(); ++i)
            ids[edits[i]] = 2 * n + i;
        sum = 0;
        for (auto id : ids)
            sum += Payload<T>::expected(id);
        require(lookup_sum<T>(*storage, keys) == sum, "churn content");

        const auto removed = n - n / 10;
        for (std::size_t i = 0; i < removed; ++i)
        {
            storage->erase(keys[order[i]]);
            sum -= Payload<T>::expected(ids[order[i]]);
        }
        require(storage->size() == n / 10, "sparse size");
        scans("iterate_keys_10pct_live", "iterate_native_10pct_live");
        timing.run("destroy_10pct_live", storage->size(), 0, [&] { storage.reset(); return 0; });
    }

    template <class T, bool Custom>
    void growth_case(std::size_t n, int trial)
    {
        using B = Backend<T, Custom>;
        typename B::Container storage;
        for (std::size_t i = 0; i < n; ++i)
            storage.emplace(Payload<T>::make(i));
        Timing timing{Payload<T>::name, B::name, n, trial};
        timing.run("append_at_capacity_boundary", 1, Payload<T>::expected(n), [&] {
            const auto key = storage.emplace(Payload<T>::make(n));
            return Payload<T>::score(*storage.get(key));
        });
    }

    template <class T>
    void timed_type(const std::vector<std::size_t> &sizes, int repeats, std::size_t query_budget, std::size_t growth_n)
    {
        for (auto n : sizes)
        {
            for (int trial = -1; trial < repeats; ++trial)
            {
                std::mt19937_64 rng(0x7baf219ULL + n + (trial + 1) * 31);
                std::vector<std::size_t> order(n), query_order(n), edits(std::max<std::size_t>(n, query_budget / 4));
                std::iota(order.begin(), order.end(), 0);
                std::iota(query_order.begin(), query_order.end(), 0);
                std::shuffle(order.begin(), order.end(), rng);
                std::shuffle(query_order.begin(), query_order.end(), rng);
                // 查询次序独立于删除次序，保证失效 key 分散在整个查询序列中。
                std::uniform_int_distribution<std::size_t> distribution(0, n - 1);
                for (auto &index : edits)
                    index = distribution(rng);
                // 同轮输入完全一致，交替容器先后顺序，减少固定顺序偏差。
                if (trial % 2 == 0)
                {
                    timed_case<T, true>(n, trial, order, query_order, edits, query_budget);
                    timed_case<T, false>(n, trial, order, query_order, edits, query_budget);
                }
                else
                {
                    timed_case<T, false>(n, trial, order, query_order, edits, query_budget);
                    timed_case<T, true>(n, trial, order, query_order, edits, query_budget);
                }
                std::fflush(stdout);
                std::fprintf(stderr, "%s n=%zu trial=%d done\n", Payload<T>::name, n, trial);
            }
        }
        for (int trial = -1; trial < repeats; ++trial)
        {
            if (trial % 2 == 0) { growth_case<T, true>(growth_n, trial); growth_case<T, false>(growth_n, trial); }
            else { growth_case<T, false>(growth_n, trial); growth_case<T, true>(growth_n, trial); }
        }
    }

#ifdef SM_TRACK_MEMORY
    template <class T, bool Custom>
    void memory_case(std::size_t n)
    {
        using B = Backend<T, Custom>;
        using Storage = typename B::Container;
        // 测试驱动的 keys/order 在统计前分配，不计入容器占用。
        std::vector<typename B::Key> keys(n);
        std::vector<std::size_t> order(n);
        std::iota(order.begin(), order.end(), 0);
        std::mt19937_64 rng(0x7baf219ULL + n);
        std::shuffle(order.begin(), order.end(), rng);
        require(allocation_tracker::live == 0, "previous memory case leaked");
        auto record = [&](const char *stage, std::size_t count) {
            allocation_tracker::enabled = false;
            std::printf("%s,%s,%zu,%s,%zu,%zu,%zu,%zu\n", Payload<T>::name, B::name, n, stage, count,
                        allocation_tracker::live, allocation_tracker::peak, allocation_tracker::calls);
        };
        allocation_tracker::start_phase();
        {
            Storage storage;
            record("empty", storage.size());
            allocation_tracker::start_phase();
            for (std::size_t i = 0; i < n; ++i)
                keys[i] = storage.emplace(Payload<T>::make(i));
            record("dense", storage.size());

            allocation_tracker::start_phase();
            { Storage copy(storage); record("copy_dense_two_containers", storage.size() + copy.size()); }
            allocation_tracker::start_phase();
            for (std::size_t i = 0; i < n / 2; ++i)
                storage.erase(keys[order[i]]);
            record("50pct_live", storage.size());
            allocation_tracker::start_phase();
            for (std::size_t i = n / 2; i < n - n / 10; ++i)
                storage.erase(keys[order[i]]);
            record("10pct_live", storage.size());
            allocation_tracker::start_phase();
            for (std::size_t i = 0; i < n - n / 10; ++i)
                keys[order[i]] = storage.emplace(Payload<T>::make(n + i));
            record("refilled", storage.size());
            allocation_tracker::start_phase();
            for (auto key : keys)
                storage.erase(key);
            record("all_erased", storage.size());
        }
        require(allocation_tracker::live == 0, "container leaked tracked allocations");
    }

    template <class T>
    void memory_type(const std::vector<std::size_t> &sizes)
    {
        for (auto n : sizes) { memory_case<T, true>(n); memory_case<T, false>(n); }
    }
#endif

    template <class T> void metadata()
    {
        std::printf("%s,%zu,%zu,%d,%d,%zu,%zu,%zu,%zu\n", Payload<T>::name, sizeof(T), alignof(T),
                    int(std::is_nothrow_move_constructible_v<T>), int(std::is_trivially_copyable_v<T>),
                    sizeof(slotmap::SlotMap<T>), sizeof(dod::slot_map64<T>),
                    sizeof(typename slotmap::SlotMap<T>::KeyType), sizeof(typename dod::slot_map64<T>::key));
    }
}

int main(int argc, char **argv)
{
    try
    {
        const std::string mode = argc > 1 ? argv[1] : "help";
        if (mode == "metadata")
        {
            std::puts("type,sizeof_T,alignof_T,nothrow_move,trivially_copyable,sizeof_custom,sizeof_dod,sizeof_custom_key,sizeof_dod_key");
            metadata<MyMesh::Vertex>(); metadata<MyMesh::Dedge>(); metadata<MyMesh::Halfface>(); metadata<MyMesh::Cell>();
            return 0;
        }
#ifdef SM_TRACK_MEMORY
        if (mode == "memory")
        {
            std::vector<std::size_t> sizes;
            for (int i = 2; i < argc; ++i) sizes.push_back(std::stoull(argv[i]));
            if (sizes.empty()) sizes = {10000, 100000, 1000000};
            for (auto n : sizes) require(n >= 10 && n < 100000000, "invalid size");
            std::puts("type,backend,n,stage,live_elements,live_requested_bytes,phase_peak_requested_bytes,phase_allocations");
            memory_type<MyMesh::Vertex>(sizes); memory_type<MyMesh::Dedge>(sizes);
            memory_type<MyMesh::Halfface>(sizes); memory_type<MyMesh::Cell>(sizes);
            return 0;
        }
#else
        if (mode == "timing" || mode == "smoke")
        {
            const bool smoke = mode == "smoke";
            const int repeats = smoke ? 1 : (argc > 2 ? std::stoi(argv[2]) : 7);
            require(repeats > 0, "invalid repeat count");
            std::vector<std::size_t> sizes;
            for (int i = 3; i < argc; ++i) sizes.push_back(std::stoull(argv[i]));
            if (smoke) sizes = {256};
            else if (sizes.empty()) sizes = {10000, 100000, 1000000};
            for (auto n : sizes) require(n >= 10 && n < 100000000, "invalid size");
            std::puts("type,backend,n,trial,operation,work_items,total_ns,ns_per_item,status");
            const auto queries = smoke ? 2048 : 2000000;
            const auto growth_n = smoke ? 512 : 65536;
            timed_type<MyMesh::Vertex>(sizes, repeats, queries, growth_n);
            timed_type<MyMesh::Dedge>(sizes, repeats, queries, growth_n);
            timed_type<MyMesh::Halfface>(sizes, repeats, queries, growth_n);
            timed_type<MyMesh::Cell>(sizes, repeats, queries, growth_n);
            return 0;
        }
#endif
        std::fprintf(stderr, "Usage: slot_map_bench timing [repeats] [sizes...] | smoke | metadata\n"
                             "       slot_map_memory memory [sizes...]\n");
        return 2;
    }
    catch (const std::exception &e)
    {
        std::fprintf(stderr, "BENCHMARK FAILED: %s\n", e.what());
        return 1;
    }
}
