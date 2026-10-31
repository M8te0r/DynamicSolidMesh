#pragma once

// 仅用于独立的内存统计可执行文件；计时版本不包含本文件。
// 统计请求的堆字节数，不含分配器取整、下列记录头或进程 RSS。
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <new>

namespace allocation_tracker
{
    inline bool enabled = false;
    inline std::size_t live = 0, peak = 0, calls = 0;
    struct Header
    {
        void *base;
        std::size_t bytes;
        bool counted;
    };

    void *allocate(std::size_t bytes, std::size_t alignment)
    {
        alignment = std::max(alignment, alignof(std::max_align_t));
        if (bytes > std::numeric_limits<std::size_t>::max() - sizeof(Header) - alignment)
            throw std::bad_alloc();
        void *base = std::malloc(bytes + sizeof(Header) + alignment);
        if (!base)
            throw std::bad_alloc();
        auto address = (reinterpret_cast<std::uintptr_t>(base) + sizeof(Header) + alignment - 1) &
                       ~(static_cast<std::uintptr_t>(alignment) - 1);
        auto *header = reinterpret_cast<Header *>(address - sizeof(Header));
        new (header) Header{base, bytes, enabled};
        if (enabled)
        {
            live += bytes;
            peak = std::max(peak, live);
            ++calls;
        }
        return reinterpret_cast<void *>(address);
    }

    void release(void *pointer) noexcept
    {
        if (!pointer)
            return;
        auto *header = reinterpret_cast<Header *>(reinterpret_cast<std::uintptr_t>(pointer) - sizeof(Header));
        if (header->counted)
            live -= header->bytes;
        std::free(header->base);
    }

    void start_phase() noexcept
    {
        peak = live;
        calls = 0;
        enabled = true;
    }
}

void *operator new(std::size_t n) { return allocation_tracker::allocate(n, alignof(std::max_align_t)); }
void *operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void *p) noexcept { allocation_tracker::release(p); }
void operator delete[](void *p) noexcept { allocation_tracker::release(p); }
void operator delete(void *p, std::size_t) noexcept { allocation_tracker::release(p); }
void operator delete[](void *p, std::size_t) noexcept { allocation_tracker::release(p); }
void *operator new(std::size_t n, std::align_val_t a) { return allocation_tracker::allocate(n, static_cast<std::size_t>(a)); }
void *operator new[](std::size_t n, std::align_val_t a) { return ::operator new(n, a); }
void operator delete(void *p, std::align_val_t) noexcept { allocation_tracker::release(p); }
void operator delete[](void *p, std::align_val_t) noexcept { allocation_tracker::release(p); }
void operator delete(void *p, std::size_t, std::align_val_t) noexcept { allocation_tracker::release(p); }
void operator delete[](void *p, std::size_t, std::align_val_t) noexcept { allocation_tracker::release(p); }
void *operator new(std::size_t n, const std::nothrow_t &) noexcept
{
    try { return ::operator new(n); } catch (...) { return nullptr; }
}
void *operator new[](std::size_t n, const std::nothrow_t &tag) noexcept { return ::operator new(n, tag); }
void operator delete(void *p, const std::nothrow_t &) noexcept { allocation_tracker::release(p); }
void operator delete[](void *p, const std::nothrow_t &) noexcept { allocation_tracker::release(p); }
void *operator new(std::size_t n, std::align_val_t a, const std::nothrow_t &) noexcept
{
    try { return ::operator new(n, a); } catch (...) { return nullptr; }
}
void *operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t &tag) noexcept { return ::operator new(n, a, tag); }
void operator delete(void *p, std::align_val_t, const std::nothrow_t &) noexcept { allocation_tracker::release(p); }
void operator delete[](void *p, std::align_val_t, const std::nothrow_t &) noexcept { allocation_tracker::release(p); }

#define SLOT_MAP_ALLOC(bytes, alignment) allocation_tracker::allocate(bytes, alignment)
#define SLOT_MAP_FREE(pointer) allocation_tracker::release(pointer)
