#pragma once
#include <cstdint>

namespace MyMesh
{
    template <class Tag, typename HandleSize>
    class Handle
    {
    public:
        // 默认构造得到空句柄。
        constexpr Handle() noexcept = default;

        // 只判断是否为空，不判断对应元素是否仍然存在。
        constexpr bool empty() const noexcept
        {
            return raw_ == 0;
        }

        friend constexpr bool operator==(Handle a, Handle b) noexcept
        {
            return a.raw_ == b.raw_;
        }

        friend constexpr bool operator!=(Handle a, Handle b) noexcept
        {
            return !(a == b);
        }

    private:
        explicit constexpr Handle(HandleSize raw) noexcept
            : raw_(raw) {}

        HandleSize raw_ = 0;

        friend class Mesh;

        template <class Storage, class H>
        friend class HandleRange;
    };

    template <class Storage, class H>
    class HandleRange
    {
    public:
        explicit HandleRange(const Storage &storage) noexcept
            : storage_(&storage) {}

        struct Iterator
        {
            typename Storage::const_kv_iterator it;

            H operator*() const noexcept
            {
                return H{it->first.raw};
            }

            Iterator &operator++() noexcept
            {
                ++it;
                return *this;
            }

            bool operator!=(const Iterator &other) const noexcept
            {
                return it != other.it;
            }
        };

        Iterator begin() const noexcept
        {
            return {storage_->items().begin()};
        }

        Iterator end() const noexcept
        {
            return {storage_->items().end()};
        }

    private:
        const Storage *storage_;
    };
}
