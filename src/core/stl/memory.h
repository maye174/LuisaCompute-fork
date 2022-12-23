//
// Created by Mike on 2022/9/30.
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <memory>

#include <EASTL/bit.h>
#include <EASTL/memory.h>
#include <EASTL/shared_array.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/shared_ptr.h>
#include <EASTL/span.h>

#include <core/dll_export.h>
#include <core/stl/hash_fwd.h>

namespace luisa {

namespace detail {
LUISA_EXPORT_API void *allocator_allocate(size_t size, size_t alignment) noexcept;
LUISA_EXPORT_API void allocator_deallocate(void *p, size_t alignment) noexcept;
LUISA_EXPORT_API void *allocator_reallocate(void *p, size_t size, size_t alignment) noexcept;
}// namespace detail

[[nodiscard]] inline auto align(size_t s, size_t a) noexcept {
    return (s + a - 1u) / a * a;
}

template<typename T = std::byte>
struct allocator {
    using value_type = T;
    constexpr allocator() noexcept = default;
    explicit constexpr allocator(const char *) noexcept {}
    template<typename U>
    constexpr allocator(allocator<U>) noexcept {}
    [[nodiscard]] auto allocate(std::size_t n) const noexcept {
        return static_cast<T *>(detail::allocator_allocate(sizeof(T) * n, alignof(T)));
    }
    [[nodiscard]] auto allocate(std::size_t n, size_t alignment, size_t) const noexcept {
        assert(alignment >= alignof(T));
        return static_cast<T *>(detail::allocator_allocate(sizeof(T) * n, alignment));
    }
    void deallocate(T *p, size_t) const noexcept {
        detail::allocator_deallocate(p, alignof(T));
    }
    void deallocate(void *p, size_t) const noexcept {
        detail::allocator_deallocate(p, alignof(T));
    }
    template<typename R>
    [[nodiscard]] constexpr auto operator==(allocator<R>) const noexcept -> bool {
        return std::is_same_v<T, R>;
    }
};

template<typename T>
[[nodiscard]] inline auto allocate_with_allocator(size_t n = 1u) noexcept {
    return allocator<T>{}.allocate(n);
}

template<typename T>
inline void deallocate_with_allocator(T *p) noexcept {
    allocator<T>{}.deallocate(p, 0u);
}

template<typename T, typename... Args>
[[nodiscard]] inline auto new_with_allocator(Args &&...args) noexcept {
    return std::construct_at(allocate_with_allocator<T>(), std::forward<Args>(args)...);
}

template<typename T>
inline void delete_with_allocator(T *p) noexcept {
    if (p != nullptr) {
        std::destroy_at(p);
        deallocate_with_allocator(p);
    }
}

using eastl::bit_cast;
using eastl::span;

struct default_sentinel_t {};
inline constexpr default_sentinel_t default_sentinel{};

// smart pointers
using eastl::const_pointer_cast;
using eastl::enable_shared_from_this;
using eastl::make_shared;
using eastl::make_unique;
using eastl::reinterpret_pointer_cast;
using eastl::shared_array;
using eastl::shared_ptr;
using eastl::static_pointer_cast;
using eastl::unique_ptr;
using eastl::weak_ptr;

// hash functions
template<typename T>
struct pointer_hash {
    using is_transparent = void;
    using is_avalanching = void;
    [[nodiscard]] uint64_t operator()(const T *p) const noexcept {
        auto x = reinterpret_cast<uint64_t>(p);
        return hash64(&x, sizeof(x), hash64_default_seed);
    }
    [[nodiscard]] uint64_t operator()(const volatile T *p) const noexcept {
        auto x = reinterpret_cast<uint64_t>(p);
        return hash64(&x, sizeof(x), hash64_default_seed);
    }
    [[nodiscard]] uint64_t operator()(const shared_ptr<T> &ptr) const noexcept {
        return (*this)(ptr.get());
    }
    template<typename Deleter>
    [[nodiscard]] uint64_t operator()(const unique_ptr<T, Deleter> &ptr) const noexcept {
        return (*this)(ptr.get());
    }
    [[nodiscard]] uint64_t operator()(const std::shared_ptr<T> &ptr) const noexcept {
        return (*this)(ptr.get());
    }
    template<typename Deleter>
    [[nodiscard]] uint64_t operator()(const std::unique_ptr<T, Deleter> &ptr) const noexcept {
        return (*this)(ptr.get());
    }
};

template<>
struct pointer_hash<void> {
    using is_transparent = void;
    using is_avalanching = void;
    [[nodiscard]] uint64_t operator()(const void *p) const noexcept {
        auto x = reinterpret_cast<uint64_t>(p);
        return hash64(&x, sizeof(x), hash64_default_seed);
    }
    [[nodiscard]] uint64_t operator()(const volatile void *p) const noexcept {
        auto x = reinterpret_cast<uint64_t>(p);
        return hash64(&x, sizeof(x), hash64_default_seed);
    }
};

}// namespace luisa
