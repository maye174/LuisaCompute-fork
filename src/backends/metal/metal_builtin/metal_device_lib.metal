#pragma clang diagnostic ignored "-Wc++17-extensions"
#pragma clang diagnostic ignored "-Wunused-variable"

#include <metal_stdlib>

using namespace metal;

#define lc_assume(...) __builtin_assume(__VA_ARGS__)

template<typename T = void>
[[noreturn, gnu::always_inline]] inline T lc_unreachable() {
    __builtin_unreachable();
}

template<typename... T>
[[nodiscard, gnu::always_inline]] inline auto make_float2x2(T... args) {
    return float2x2(args...);
}

[[nodiscard, gnu::always_inline]] inline auto make_float2x2(float3x3 m) {
    return float2x2(m[0].xy, m[1].xy);
}

[[nodiscard, gnu::always_inline]] inline auto make_float2x2(float4x4 m) {
    return float2x2(m[0].xy, m[1].xy);
}

template<typename... T>
[[nodiscard, gnu::always_inline]] inline auto make_float3x3(T... args) {
    return float3x3(args...);
}

[[nodiscard, gnu::always_inline]] inline auto make_float3x3(float2x2 m) {
    return float3x3(
        float3(m[0], 0.0f),
        float3(m[1], 0.0f),
        float3(0.0f, 0.0f, 1.0f));
}

[[nodiscard, gnu::always_inline]] inline auto make_float3x3(float4x4 m) {
    return float3x3(m[0].xyz, m[1].xyz, m[2].xyz);
}

template<typename... T>
[[nodiscard, gnu::always_inline]] inline auto make_float4x4(T... args) {
    return float4x4(args...);
}

[[nodiscard, gnu::always_inline]] inline auto make_float4x4(float2x2 m) {
    return float4x4(
        float4(m[0], 0.0f, 0.0f),
        float4(m[1], 0.0f, 0.0f),
        float4(0.0f, 0.0f, 1.0f, 0.0f),
        float4(0.0f, 0.0f, 0.0f, 1.0f));
}

[[nodiscard, gnu::always_inline]] inline auto make_float4x4(float3x3 m) {
    return float4x4(
        float4(m[0], 0.0f),
        float4(m[1], 0.0f),
        float4(m[2], 0.0f),
        float4(0.0f, 0.0f, 0.0f, 1.0f));
}

template<typename T>
struct LCBuffer {
    device T *data;
    ulong size;
};

template<typename T>
struct LCBuffer<const T> {
    const device T *data;
    ulong size;

    LCBuffer(LCBuffer<T> buffer)
        : data{buffer.data}, size{buffer.size} {}
};

template<typename T, typename I>
[[nodiscard, gnu::always_inline]] inline auto buffer_read(LCBuffer<T> buffer, I index) {
    return buffer.data[index];
}

template<typename T, typename I>
[[gnu::always_inline]] inline void buffer_write(LCBuffer<T> buffer, I index, T value) {
    buffer.data[index] = value;
}

template<typename T>
[[gnu::always_inline]] inline auto buffer_size(LCBuffer<T> buffer) {
    return buffer.size;
}

template<typename T>
[[nodiscard, gnu::always_inline]] inline auto address_of(thread T &x) { return &x; }

template<typename T>
[[nodiscard, gnu::always_inline]] inline auto address_of(threadgroup T &x) { return &x; }

template<typename T>
[[nodiscard, gnu::always_inline]] inline auto address_of(device T &x) { return &x; }

namespace detail {
template<typename T>
inline auto vector_element_impl(T v) { return v.x; }
}// namespace detail

template<typename T>
struct vector_element {
    using type = decltype(detail::vector_element_impl(T{}));
};

template<typename T>
using vector_element_t = typename vector_element<T>::type;

#define LC_VECTOR_REF_IMPL(T, N, addr_space)                                                                       \
    [[nodiscard, gnu::always_inline]] inline addr_space auto &vector_element_ref(addr_space T##N &v, uint index) { \
        return *(reinterpret_cast<addr_space vector_element_t<T##N> *>(&v) + index);                               \
    }                                                                                                              \
    [[nodiscard, gnu::always_inline]] inline auto vector_element_ref(addr_space const T##N &v, uint index) { return v[index]; }

#define LC_VECTOR_REF(T, N)               \
    LC_VECTOR_REF_IMPL(T, N, thread)      \
    LC_VECTOR_REF_IMPL(T, N, threadgroup) \
    LC_VECTOR_REF_IMPL(T, N, device)

#define LC_VECTOR_REF_ALL(T) \
    LC_VECTOR_REF(T, 2)      \
    LC_VECTOR_REF(T, 3)      \
    LC_VECTOR_REF(T, 4)

LC_VECTOR_REF_ALL(half)
LC_VECTOR_REF_ALL(float)
LC_VECTOR_REF_ALL(int)
LC_VECTOR_REF_ALL(uint)
LC_VECTOR_REF_ALL(bool)
LC_VECTOR_REF_ALL(char)
LC_VECTOR_REF_ALL(uchar)
LC_VECTOR_REF_ALL(short)
LC_VECTOR_REF_ALL(ushort)
LC_VECTOR_REF_ALL(long)
LC_VECTOR_REF_ALL(ulong)

// sinkholes for non-vectors
template<typename T>
[[nodiscard, gnu::always_inline]] inline thread auto &vector_element_ref(thread T &v) { return v; }

template<typename T>
[[nodiscard, gnu::always_inline]] inline threadgroup auto &vector_element_ref(threadgroup T &v) { return v; }

template<typename T>
[[nodiscard, gnu::always_inline]] inline device auto &vector_element_ref(device T &v) { return v; }

template<typename T, access a>
[[nodiscard, gnu::always_inline]] inline auto texture_read(texture2d<T, a> t, uint2 uv) {
    return t.read(uv);
}

template<typename T, access a>
[[nodiscard, gnu::always_inline]] inline auto texture_read(texture3d<T, a> t, uint3 uvw) {
    return t.read(uvw);
}

template<typename T, access a, typename Value>
[[gnu::always_inline]] inline void texture_write(texture2d<T, a> t, uint2 uv, Value value) {
    t.write(value, uv);
    if constexpr (a == access::read_write) { t.fence(); }
}

template<typename T, access a, typename Value>
[[gnu::always_inline]] inline void texture_write(texture3d<T, a> t, uint3 uvw, Value value) {
    t.write(value, uvw);
    if constexpr (a == access::read_write) { t.fence(); }
}

[[nodiscard]] inline auto inverse(float2x2 m) {
    const auto one_over_determinant = 1.0f / (m[0][0] * m[1][1] - m[1][0] * m[0][1]);
    return float2x2(m[1][1] * one_over_determinant,
                    -m[0][1] * one_over_determinant,
                    -m[1][0] * one_over_determinant,
                    +m[0][0] * one_over_determinant);
}

[[nodiscard]] inline auto inverse(float3x3 m) {
    const auto one_over_determinant = 1.0f / (m[0].x * (m[1].y * m[2].z - m[2].y * m[1].z) - m[1].x * (m[0].y * m[2].z - m[2].y * m[0].z) + m[2].x * (m[0].y * m[1].z - m[1].y * m[0].z));
    return float3x3(
        (m[1].y * m[2].z - m[2].y * m[1].z) * one_over_determinant,
        (m[2].y * m[0].z - m[0].y * m[2].z) * one_over_determinant,
        (m[0].y * m[1].z - m[1].y * m[0].z) * one_over_determinant,
        (m[2].x * m[1].z - m[1].x * m[2].z) * one_over_determinant,
        (m[0].x * m[2].z - m[2].x * m[0].z) * one_over_determinant,
        (m[1].x * m[0].z - m[0].x * m[1].z) * one_over_determinant,
        (m[1].x * m[2].y - m[2].x * m[1].y) * one_over_determinant,
        (m[2].x * m[0].y - m[0].x * m[2].y) * one_over_determinant,
        (m[0].x * m[1].y - m[1].x * m[0].y) * one_over_determinant);
}

[[nodiscard]] inline auto inverse(float4x4 m) {
    const auto coef00 = m[2].z * m[3].w - m[3].z * m[2].w;
    const auto coef02 = m[1].z * m[3].w - m[3].z * m[1].w;
    const auto coef03 = m[1].z * m[2].w - m[2].z * m[1].w;
    const auto coef04 = m[2].y * m[3].w - m[3].y * m[2].w;
    const auto coef06 = m[1].y * m[3].w - m[3].y * m[1].w;
    const auto coef07 = m[1].y * m[2].w - m[2].y * m[1].w;
    const auto coef08 = m[2].y * m[3].z - m[3].y * m[2].z;
    const auto coef10 = m[1].y * m[3].z - m[3].y * m[1].z;
    const auto coef11 = m[1].y * m[2].z - m[2].y * m[1].z;
    const auto coef12 = m[2].x * m[3].w - m[3].x * m[2].w;
    const auto coef14 = m[1].x * m[3].w - m[3].x * m[1].w;
    const auto coef15 = m[1].x * m[2].w - m[2].x * m[1].w;
    const auto coef16 = m[2].x * m[3].z - m[3].x * m[2].z;
    const auto coef18 = m[1].x * m[3].z - m[3].x * m[1].z;
    const auto coef19 = m[1].x * m[2].z - m[2].x * m[1].z;
    const auto coef20 = m[2].x * m[3].y - m[3].x * m[2].y;
    const auto coef22 = m[1].x * m[3].y - m[3].x * m[1].y;
    const auto coef23 = m[1].x * m[2].y - m[2].x * m[1].y;
    const auto fac0 = float4{coef00, coef00, coef02, coef03};
    const auto fac1 = float4{coef04, coef04, coef06, coef07};
    const auto fac2 = float4{coef08, coef08, coef10, coef11};
    const auto fac3 = float4{coef12, coef12, coef14, coef15};
    const auto fac4 = float4{coef16, coef16, coef18, coef19};
    const auto fac5 = float4{coef20, coef20, coef22, coef23};
    const auto Vec0 = float4{m[1].x, m[0].x, m[0].x, m[0].x};
    const auto Vec1 = float4{m[1].y, m[0].y, m[0].y, m[0].y};
    const auto Vec2 = float4{m[1].z, m[0].z, m[0].z, m[0].z};
    const auto Vec3 = float4{m[1].w, m[0].w, m[0].w, m[0].w};
    const auto inv0 = Vec1 * fac0 - Vec2 * fac1 + Vec3 * fac2;
    const auto inv1 = Vec0 * fac0 - Vec2 * fac3 + Vec3 * fac4;
    const auto inv2 = Vec0 * fac1 - Vec1 * fac3 + Vec3 * fac5;
    const auto inv3 = Vec0 * fac2 - Vec1 * fac4 + Vec2 * fac5;
    auto sign_a = float4{+1.0f, -1.0f, +1.0f, -1.0f};
    auto sign_b = float4{-1.0f, +1.0f, -1.0f, +1.0f};
    const auto inv_0 = inv0 * sign_a;
    const auto inv_1 = inv1 * sign_b;
    const auto inv_2 = inv2 * sign_a;
    const auto inv_3 = inv3 * sign_b;
    const auto dot0 = m[0] * float4{inv_0.x, inv_1.x, inv_2.x, inv_3.x};
    const auto dot1 = dot0.x + dot0.y + dot0.z + dot0.w;
    const auto one_over_determinant = 1.0f / dot1;
    return float4x4(inv_0 * one_over_determinant,
                    inv_1 * one_over_determinant,
                    inv_2 * one_over_determinant,
                    inv_3 * one_over_determinant);
}

[[gnu::always_inline]] inline void block_barrier() {
    threadgroup_barrier(mem_flags::mem_threadgroup);
}

#define LC_AS_ATOMIC(addr_space, type)                                            \
    [[gnu::always_inline, nodiscard]] inline auto as_atomic(addr_space type &a) { \
        return reinterpret_cast<addr_space atomic_##type *>(&a);                  \
    }
LC_AS_ATOMIC(device, int)
LC_AS_ATOMIC(device, uint)
LC_AS_ATOMIC(device, float)
LC_AS_ATOMIC(threadgroup, int)
LC_AS_ATOMIC(threadgroup, uint)
LC_AS_ATOMIC(threadgroup, float)
#undef LC_AS_ATOMIC

template<typename A, typename T>
[[gnu::always_inline]] inline auto atomic_compare_exchange_explicit(A a, T expected, T desired, memory_order) {
    atomic_compare_exchange_weak_explicit(a, &expected, desired, memory_order_relaxed, memory_order_relaxed);
    return expected;
}

[[gnu::always_inline]] inline auto atomic_compare_exchange_explicit(threadgroup atomic_float *a, float expected, float desired, memory_order) {
    auto expected_i = as_type<int>(expected);
    atomic_compare_exchange_weak_explicit(reinterpret_cast<threadgroup atomic_int *>(a), &expected_i, as_type<int>(desired), memory_order_relaxed, memory_order_relaxed);
    return as_type<float>(expected_i);
}

[[gnu::always_inline]] inline auto atomic_exchange_explicit(threadgroup atomic_float *a, float x, memory_order) {
    return as_type<float>(atomic_exchange_explicit(reinterpret_cast<threadgroup atomic_int *>(a), as_type<int>(x), memory_order_relaxed));
}

[[gnu::always_inline]] inline auto atomic_fetch_min_explicit(device atomic_float *a, float val, memory_order) {
    for (;;) {
        if (auto old = atomic_load_explicit(static_cast<device volatile atomic_float *>(a), memory_order_relaxed);
            old <= val || atomic_compare_exchange_explicit(a, old, val, memory_order_relaxed)) {
            return old;
        }
    }
}

[[gnu::always_inline]] inline auto atomic_fetch_min_explicit(threadgroup atomic_float *a, float val, memory_order) {
    for (;;) {
        if (auto old = *reinterpret_cast<threadgroup volatile float *>(a);
            old <= val || atomic_compare_exchange_explicit(a, old, val, memory_order_relaxed) == old) {
            return old;
        }
    }
}

[[gnu::always_inline]] inline auto atomic_fetch_max_explicit(device atomic_float *a, float val, memory_order) {
    for (;;) {
        if (auto old = atomic_load_explicit(static_cast<device volatile atomic_float *>(a), memory_order_relaxed);
            old >= val || atomic_compare_exchange_explicit(a, old, val, memory_order_relaxed)) {
            return old;
        }
    }
}

[[gnu::always_inline]] inline auto atomic_fetch_max_explicit(threadgroup atomic_float *a, float val, memory_order) {
    for (;;) {
        if (auto old = *reinterpret_cast<threadgroup volatile float *>(a);
            old >= val || atomic_compare_exchange_explicit(a, old, val, memory_order_relaxed) == old) {
            return old;
        }
    }
}

[[gnu::always_inline]] inline auto atomic_fetch_add_explicit(threadgroup atomic_float *a, float val, memory_order) {
    for (;;) {
        if (auto old = *reinterpret_cast<threadgroup volatile float *>(a);
            atomic_compare_exchange_explicit(a, old, old + val, memory_order_relaxed) == old) {
            return old;
        }
    }
}

[[gnu::always_inline]] inline auto atomic_fetch_sub_explicit(threadgroup atomic_float *object, float operand, memory_order) {
    return atomic_fetch_add_explicit(object, -operand, memory_order_relaxed);
}

#define LC_ATOMIC_OP2(op, addr_space, type)                                         \
    [[gnu::always_inline]] inline auto lc_atomic_##op(addr_space type &a, type x) { \
        return atomic_##op##_explicit(as_atomic(a), x, memory_order_relaxed);       \
    }
#define LC_ATOMIC_OP3(op, addr_space, type)                                                 \
    [[gnu::always_inline]] inline auto lc_atomic_##op(addr_space type &a, type x, type y) { \
        return atomic_##op##_explicit(as_atomic(a), x, y, memory_order_relaxed);            \
    }

#define LC_ATOMIC_OP2_INT(op)           \
    LC_ATOMIC_OP2(op, device, int)      \
    LC_ATOMIC_OP2(op, device, uint)     \
    LC_ATOMIC_OP2(op, threadgroup, int) \
    LC_ATOMIC_OP2(op, threadgroup, uint)

#define LC_ATOMIC_OP2_FLOAT(op)      \
    LC_ATOMIC_OP2(op, device, float) \
    LC_ATOMIC_OP2(op, threadgroup, float)

#define LC_ATOMIC_OP2_ALL(op) \
    LC_ATOMIC_OP2_INT(op)     \
    LC_ATOMIC_OP2_FLOAT(op)

#define LC_ATOMIC_OP3_INT(op)           \
    LC_ATOMIC_OP3(op, device, int)      \
    LC_ATOMIC_OP3(op, device, uint)     \
    LC_ATOMIC_OP3(op, threadgroup, int) \
    LC_ATOMIC_OP3(op, threadgroup, uint)

#define LC_ATOMIC_OP3_FLOAT(op)      \
    LC_ATOMIC_OP3(op, device, float) \
    LC_ATOMIC_OP3(op, threadgroup, float)

#define LC_ATOMIC_OP3_ALL(op) \
    LC_ATOMIC_OP3_INT(op)     \
    LC_ATOMIC_OP3_FLOAT(op)

LC_ATOMIC_OP2_ALL(exchange)
LC_ATOMIC_OP3_ALL(compare_exchange)
LC_ATOMIC_OP2_ALL(fetch_add)
LC_ATOMIC_OP2_ALL(fetch_sub)
LC_ATOMIC_OP2_INT(fetch_and)
LC_ATOMIC_OP2_INT(fetch_or)
LC_ATOMIC_OP2_INT(fetch_xor)
LC_ATOMIC_OP2_ALL(fetch_min)
LC_ATOMIC_OP2_ALL(fetch_max)

[[gnu::always_inline, nodiscard]] inline auto lc_isnan(float x) {
    auto u = as_type<uint>(x);
    return (u & 0x7F800000u) == 0x7F800000u && (u & 0x7FFFFFu);
}

[[gnu::always_inline, nodiscard]] inline auto lc_isnan(float2 v) {
    return bool2(lc_isnan(v.x), lc_isnan(v.y));
}

[[gnu::always_inline, nodiscard]] inline auto lc_isnan(float3 v) {
    return bool3(lc_isnan(v.x), lc_isnan(v.y), lc_isnan(v.z));
}

[[gnu::always_inline, nodiscard]] inline auto lc_isnan(float4 v) {
    return bool4(lc_isnan(v.x), lc_isnan(v.y), lc_isnan(v.z), lc_isnan(v.w));
}

[[gnu::always_inline, nodiscard]] inline auto lc_isinf(float x) {
    auto u = as_type<uint>(x);
    return (u & 0x7F800000u) == 0x7F800000u && !(u & 0x7FFFFFu);
}

[[gnu::always_inline, nodiscard]] inline auto lc_isinf(float2 v) {
    return bool2(lc_isinf(v.x), lc_isinf(v.y));
}

[[gnu::always_inline, nodiscard]] inline auto lc_isinf(float3 v) {
    return bool3(lc_isinf(v.x), lc_isinf(v.y), lc_isinf(v.z));
}

[[gnu::always_inline, nodiscard]] inline auto lc_isinf(float4 v) {
    return bool4(lc_isinf(v.x), lc_isinf(v.y), lc_isinf(v.z), lc_isinf(v.w));
}

template<typename T>
[[gnu::always_inline, nodiscard]] inline auto lc_select(T f, T t, bool b) {
    return b ? t : f;
}

template<typename T>
[[gnu::always_inline, nodiscard]] inline auto lc_select(T f, T t, bool2 b) {
    return T{b.x ? t.x : f.x, b.y ? t.y : f.y};
}

template<typename T>
[[gnu::always_inline, nodiscard]] inline auto lc_select(T f, T t, bool3 b) {
    return T{b.x ? t.x : f.x, b.y ? t.y : f.y, b.z ? t.z : f.z};
}

template<typename T>
[[gnu::always_inline, nodiscard]] inline auto lc_select(T f, T t, bool4 b) {
    return T{b.x ? t.x : f.x, b.y ? t.y : f.y, b.z ? t.z : f.z, b.w ? t.w : f.w};
}

struct alignas(16) LCBindlessItem {
    device const void *buffer;
    ulong buffer_size : 48;
    uint sampler2d : 8;
    uint sampler3d : 8;
    metal::texture2d<float> tex2d;
    metal::texture3d<float> tex3d;
};

struct LCBindlessArray {
    device const LCBindlessItem *items;
};

[[nodiscard, gnu::always_inline]] sampler get_sampler(uint code) {
    const array<sampler, 16u> samplers{
        sampler(coord::normalized, address::clamp_to_edge, filter::nearest, mip_filter::none),
        sampler(coord::normalized, address::repeat, filter::nearest, mip_filter::none),
        sampler(coord::normalized, address::mirrored_repeat, filter::nearest, mip_filter::none),
        sampler(coord::normalized, address::clamp_to_zero, filter::nearest, mip_filter::none),
        sampler(coord::normalized, address::clamp_to_edge, filter::linear, mip_filter::none),
        sampler(coord::normalized, address::repeat, filter::linear, mip_filter::none),
        sampler(coord::normalized, address::mirrored_repeat, filter::linear, mip_filter::none),
        sampler(coord::normalized, address::clamp_to_zero, filter::linear, mip_filter::none),
        sampler(coord::normalized, address::clamp_to_edge, filter::linear, mip_filter::linear, max_anisotropy(1)),
        sampler(coord::normalized, address::repeat, filter::linear, mip_filter::linear, max_anisotropy(1)),
        sampler(coord::normalized, address::mirrored_repeat, filter::linear, mip_filter::linear, max_anisotropy(1)),
        sampler(coord::normalized, address::clamp_to_zero, filter::linear, mip_filter::linear, max_anisotropy(1)),
        sampler(coord::normalized, address::clamp_to_edge, filter::linear, mip_filter::linear, max_anisotropy(16)),
        sampler(coord::normalized, address::repeat, filter::linear, mip_filter::linear, max_anisotropy(16)),
        sampler(coord::normalized, address::mirrored_repeat, filter::linear, mip_filter::linear, max_anisotropy(16)),
        sampler(coord::normalized, address::clamp_to_zero, filter::linear, mip_filter::linear, max_anisotropy(16))};
    __builtin_assume(code < 16u);
    return samplers[code];
}

[[nodiscard, gnu::always_inline]] inline auto bindless_texture_sample2d(LCBindlessArray array, uint index, float2 uv) {
    device const auto &t = array.items[index];
    return t.tex2d.sample(get_sampler(t.sampler2d), uv);
}

[[nodiscard, gnu::always_inline]] inline auto bindless_texture_sample3d(LCBindlessArray array, uint index, float3 uvw) {
    device const auto &t = array.items[index];
    return t.tex3d.sample(get_sampler(t.sampler3d), uvw);
}

[[nodiscard, gnu::always_inline]] inline auto bindless_texture_sample2d_level(LCBindlessArray array, uint index, float2 uv, float lod) {
    device const auto &t = array.items[index];
    return t.tex2d.sample(get_sampler(t.sampler2d), uv, level(lod));
}

[[nodiscard, gnu::always_inline]] inline auto bindless_texture_sample3d_level(LCBindlessArray array, uint index, float3 uvw, float lod) {
    device const auto &t = array.items[index];
    return t.tex3d.sample(get_sampler(t.sampler3d), uvw, level(lod));
}

[[nodiscard, gnu::always_inline]] inline auto bindless_texture_sample2d_grad(LCBindlessArray array, uint index, float2 uv, float2 dpdx, float2 dpdy) {
    device const auto &t = array.items[index];
    return t.tex2d.sample(get_sampler(t.sampler2d), uv, gradient2d(dpdx, dpdy));
}

[[nodiscard, gnu::always_inline]] inline auto bindless_texture_sample3d_grad(LCBindlessArray array, uint index, float3 uvw, float3 dpdx, float3 dpdy) {
    device const auto &t = array.items[index];
    return t.tex3d.sample(get_sampler(t.sampler3d), uvw, gradient3d(dpdx, dpdy));
}

[[nodiscard, gnu::always_inline]] inline auto bindless_texture_size2d(LCBindlessArray array, uint i) {
    return uint2(array.items[i].tex2d.get_width(), array.items[i].tex2d.get_height());
}

[[nodiscard, gnu::always_inline]] inline auto bindless_texture_size3d(LCBindlessArray array, uint i) {
    return uint3(array.items[i].tex3d.get_width(), array.items[i].tex3d.get_height(), array.items[i].tex3d.get_depth());
}

[[nodiscard, gnu::always_inline]] inline auto bindless_texture_size2d_level(LCBindlessArray array, uint i, uint lv) {
    return uint2(array.items[i].tex2d.get_width(lv), array.items[i].tex2d.get_height(lv));
}

[[nodiscard, gnu::always_inline]] inline auto bindless_texture_size3d_level(LCBindlessArray array, uint i, uint lv) {
    return uint3(array.items[i].tex3d.get_width(lv), array.items[i].tex3d.get_height(lv), array.items[i].tex3d.get_depth(lv));
}

[[nodiscard, gnu::always_inline]] inline auto bindless_texture_read2d(LCBindlessArray array, uint i, uint2 uv) {
    return array.items[i].tex2d.read(uv);
}

[[nodiscard, gnu::always_inline]] inline auto bindless_texture_read3d(LCBindlessArray array, uint i, uint3 uvw) {
    return array.items[i].tex3d.read(uvw);
}

[[nodiscard, gnu::always_inline]] inline auto bindless_texture_read2d_level(LCBindlessArray array, uint i, uint2 uv, uint lv) {
    return array.items[i].tex2d.read(uv, lv);
}

[[nodiscard, gnu::always_inline]] inline auto bindless_texture_read3d_level(LCBindlessArray array, uint i, uint3 uvw, uint lv) {
    return array.items[i].tex3d.read(uvw, lv);
}

template<typename T>
[[nodiscard, gnu::always_inline]] inline auto bindless_buffer_size(LCBindlessArray array, uint buffer_index) {
    return array.items[buffer_index].buffer_size / sizeof(T);
}

template<typename T>
[[nodiscard, gnu::always_inline]] inline auto bindless_buffer_read(LCBindlessArray array, uint buffer_index, uint i) {
    return static_cast<device const T *>(array.items[buffer_index].buffer)[i];
}

using namespace metal::raytracing;

struct alignas(16) LCRay {
    array<float, 3> m0;// origin
    float m1;          // min distance
    array<float, 3> m2;// direction
    float m3;          // max distance
};

struct LCTriangleHit {
    uint m0;  // instance index
    uint m1;  // triangle index
    float2 m2;// barycentric coordinates
    float m3; // distance
};

struct LCCommittedHit {
    uint m0;  // instance index
    uint m1;  // primitive index
    float2 m2;// barycentric coordinates
    uint m3;  // hit kind
    float m4; // distance
};

struct LCProceduralHit {
    uint m0;// instance index
    uint m1;// primitive index
};

struct alignas(16) LCInstance {
    array<float, 12> transform;
    uint options;
    uint mask;
    uint intersection_function_offset;
    uint mesh_index;
};

static_assert(sizeof(LCInstance) == 64u, "");

struct LCAccel {
    instance_acceleration_structure handle;
    device LCInstance *__restrict__ instances;
};

[[nodiscard, gnu::always_inline]] auto intersector_closest() {
    intersector<triangle_data, instancing> i;
    i.assume_geometry_type(geometry_type::triangle);
    i.force_opacity(forced_opacity::opaque);
    i.accept_any_intersection(false);
    return i;
}

[[nodiscard, gnu::always_inline]] auto intersector_any() {
    intersector<triangle_data, instancing> i;
    i.assume_geometry_type(geometry_type::triangle);
    i.force_opacity(forced_opacity::opaque);
    i.accept_any_intersection(true);
    return i;
}

[[nodiscard, gnu::always_inline]] inline auto make_ray(LCRay r_in) {
    auto o = float3(r_in.m0[0], r_in.m0[1], r_in.m0[2]);
    auto d = float3(r_in.m2[0], r_in.m2[1], r_in.m2[2]);
    return ray{o, d, r_in.m1, r_in.m3};
}

[[nodiscard, gnu::always_inline]] inline auto accel_trace_closest(LCAccel accel, LCRay r, uint mask) {
    auto isect = intersector_closest().intersect(make_ray(r), accel.handle, mask);
    return isect.type == intersection_type::none ?
               LCTriangleHit{0xffffffffu, 0xffffffffu, float2(0.f), 0.f} :
               LCTriangleHit{isect.instance_id,
                             isect.primitive_id,
                             isect.triangle_barycentric_coord,
                             isect.distance};
}

[[nodiscard, gnu::always_inline]] inline auto accel_trace_any(LCAccel accel, LCRay r, uint mask) {
    auto isect = intersector_any().intersect(make_ray(r), accel.handle, mask);
    return isect.type != intersection_type::none;
}

struct LCRayQuery {
    instance_acceleration_structure accel;
    ray ray;
    uint mask;
    bool terminate_on_first_hit;
    thread intersection_query<triangle_data, instancing> *i;
};

[[nodiscard, gnu::always_inline]] inline auto accel_query_all(LCAccel accel, LCRay ray, uint mask) {
    return LCRayQuery{accel.handle, make_ray(ray), mask, false, nullptr};
}

[[nodiscard, gnu::always_inline]] inline auto accel_query_any(LCAccel accel, LCRay ray, uint mask) {
    return LCRayQuery{accel.handle, make_ray(ray), mask, true, nullptr};
}

void ray_query_init(thread LCRayQuery &q, thread intersection_query<triangle_data, instancing> &i, bool has_procedural_branch) {
    intersection_params params;
    params.accept_any_intersection(q.terminate_on_first_hit);
    params.assume_geometry_type(has_procedural_branch ?
                                    geometry_type::triangle | geometry_type::bounding_box :
                                    geometry_type::triangle);
    i.reset(q.ray, q.accel, q.mask, params);
    q.i = &i;
}

#define LC_RAY_QUERY_SHADOW_VARIABLE(q) \
    intersection_query<triangle_data, instancing> q##_i

#define LC_RAY_QUERY_INIT(q) ray_query_init(q, q##_i, true)
#define LC_RAY_QUERY_INIT_NO_PROCEDURAL(q) ray_query_init(q, q##_i, false)

[[nodiscard]] inline auto ray_query_is_triangle_candidate(LCRayQuery q) {
    return q.i->get_candidate_intersection_type() == intersection_type::triangle;
}

[[nodiscard]] inline auto ray_query_next(LCRayQuery q) {
    return q.i->next();
}

[[nodiscard]] inline auto ray_query_world_ray(LCRayQuery q) {
    auto o = q.i->get_world_space_ray_origin();
    auto d = q.i->get_world_space_ray_direction();
    auto t_min = q.i->get_ray_min_distance();
    auto t_max = q.i->get_committed_distance();
    return LCRay{.m0 = {o.x, o.y, o.z},
                 .m1 = t_min,
                 .m2 = {d.x, d.y, d.z},
                 .m3 = t_max};
}

[[nodiscard]] inline auto ray_query_procedural_candidate(LCRayQuery q) {
    auto inst = q.i->get_candidate_instance_id();
    auto prim = q.i->get_candidate_primitive_id();
    return LCProceduralHit{inst, prim};
}

[[nodiscard]] inline auto ray_query_triangle_candidate(LCRayQuery q) {
    auto inst = q.i->get_candidate_instance_id();
    auto prim = q.i->get_candidate_primitive_id();
    auto bary = q.i->get_candidate_triangle_barycentric_coord();
    auto t = q.i->get_candidate_triangle_distance();
    return LCTriangleHit{inst, prim, bary, t};
}

[[nodiscard]] inline auto ray_query_committed_hit(LCRayQuery q) {
    auto type = q.i->get_committed_intersection_type();
    auto kind = type == intersection_type::none     ? 0u :
                type == intersection_type::triangle ? 1u :
                                                      2u;
    auto inst = kind ? q.i->get_committed_instance_id() : ~0u;
    auto prim = q.i->get_committed_primitive_id();
    auto bary = q.i->get_committed_triangle_barycentric_coord();
    auto t = q.i->get_committed_distance();
    return LCCommittedHit{inst, prim, bary, kind, t};
}

inline void ray_query_commit_triangle(LCRayQuery q) {
    q.i->commit_triangle_intersection();
}

inline void ray_query_commit_procedural(LCRayQuery q, float t) {
    q.i->commit_bounding_box_intersection(t);
}

inline void ray_query_terminate(LCRayQuery q) {
    q.i->abort();
}

[[nodiscard, gnu::always_inline]] inline auto
accel_instance_transform(LCAccel accel, uint i) {
    auto m = accel.instances[i].transform;
    return float4x4(
        m[0], m[1], m[2], 0.0f,
        m[3], m[4], m[5], 0.0f,
        m[6], m[7], m[8], 0.0f,
        m[9], m[10], m[11], 1.0f);
}

[[gnu::always_inline]] inline void accel_set_instance_transform(LCAccel accel, uint i, float4x4 m) {
    auto p = accel.instances[i].transform.data();
    p[0] = m[0][0];
    p[1] = m[0][1];
    p[2] = m[0][2];
    p[3] = m[1][0];
    p[4] = m[1][1];
    p[5] = m[1][2];
    p[6] = m[2][0];
    p[7] = m[2][1];
    p[8] = m[2][2];
    p[9] = m[3][0];
    p[10] = m[3][1];
    p[11] = m[3][2];
}

[[gnu::always_inline]] inline void accel_set_instance_visibility(LCAccel accel, uint i, uint mask) {
    accel.instances[i].mask = mask;
}

[[gnu::always_inline]] inline void accel_set_instance_opacity(LCAccel accel, uint i, bool opaque) {
    auto instance_option_opaque = 4u;
    auto instance_option_non_opaque = 8u;
    auto options = accel.instances[i].options;
    options &= ~(instance_option_opaque | instance_option_non_opaque);
    options |= opaque ? instance_option_opaque : instance_option_non_opaque;
    accel.instances[i].options = options;
}

template<typename T>
[[gnu::always_inline, nodiscard]] inline auto lc_one();

template<typename T>
struct One {
    [[gnu::always_inline, nodiscard]] inline static auto make() { return T{1}; }
};

template<typename T, size_t N>
struct One<array<T, N>> {
    [[gnu::always_inline, nodiscard]] inline static auto make() {
        array<T, N> a;
#pragma unroll
        for (auto i = 0u; i < N; i++) { a[i] = lc_one<T>(); }
        return a;
    }
};

template<typename T>
[[gnu::always_inline, nodiscard]] inline auto lc_zero() { return T{}; }

template<typename T>
[[gnu::always_inline, nodiscard]] inline auto lc_one() { return One<T>::make(); }

template<>
[[gnu::always_inline, nodiscard]] inline auto lc_one<float2x2>() { return float2x2{lc_one<float2>(), lc_one<float2>()}; }

template<>
[[gnu::always_inline, nodiscard]] inline auto lc_one<float3x3>() { return float3x3{lc_one<float3>(), lc_one<float3>(), lc_one<float3>()}; }

template<>
[[gnu::always_inline, nodiscard]] inline auto lc_one<float4x4>() { return float4x4{lc_one<float4>(), lc_one<float4>(), lc_one<float4>(), lc_one<float4>()}; }

[[nodiscard]] inline auto lc_mat_comp_mul(float2x2 lhs, float2x2 rhs) {
    return float2x2(lhs[0] * rhs[0],
                    lhs[1] * rhs[1]);
}

[[nodiscard]] inline auto lc_mat_comp_mul(float3x3 lhs, float3x3 rhs) {
    return float3x3(lhs[0] * rhs[0],
                    lhs[1] * rhs[1],
                    lhs[2] * rhs[2]);
}

[[nodiscard]] inline auto lc_mat_comp_mul(float4x4 lhs, float4x4 rhs) {
    return float4x4(lhs[0] * rhs[0],
                    lhs[1] * rhs[1],
                    lhs[2] * rhs[2],
                    lhs[3] * rhs[3]);
}

[[nodiscard]] inline auto lc_remove_nan(float v) {
    return lc_select(v, lc_zero<float>(), lc_isnan(v) | lc_isinf(v));
}

[[nodiscard]] inline auto lc_remove_nan(float2 v) {
    return lc_select(v, lc_zero<float2>(), lc_isnan(v) | lc_isinf(v));
}

[[nodiscard]] inline auto lc_remove_nan(float3 v) {
    return lc_select(v, lc_zero<float3>(), lc_isnan(v) | lc_isinf(v));
}

[[nodiscard]] inline auto lc_remove_nan(float4 v) {
    return lc_select(v, lc_zero<float4>(), lc_isnan(v) | lc_isinf(v));
}

[[nodiscard]] inline auto lc_remove_nan(float2x2 v) {
    return float2x2(lc_remove_nan(v[0]), lc_remove_nan(v[1]));
}

[[nodiscard]] inline auto lc_remove_nan(float3x3 v) {
    return float3x3(lc_remove_nan(v[0]), lc_remove_nan(v[1]), lc_remove_nan(v[2]));
}

[[nodiscard]] inline auto lc_remove_nan(float4x4 v) {
    return float4x4(lc_remove_nan(v[0]), lc_remove_nan(v[1]), lc_remove_nan(v[2]), lc_remove_nan(v[3]));
}

template<typename T>
[[gnu::always_inline]] inline void lc_accumulate_grad(thread T *dst, T grad) {}

[[gnu::always_inline]] inline void lc_accumulate_grad(thread float *dst, float grad) { *dst += lc_remove_nan(grad); }
[[gnu::always_inline]] inline void lc_accumulate_grad(thread float2 *dst, float2 grad) { *dst += lc_remove_nan(grad); }
[[gnu::always_inline]] inline void lc_accumulate_grad(thread float3 *dst, float3 grad) { *dst += lc_remove_nan(grad); }
[[gnu::always_inline]] inline void lc_accumulate_grad(thread float4 *dst, float4 grad) { *dst += lc_remove_nan(grad); }
[[gnu::always_inline]] inline void lc_accumulate_grad(thread float2x2 *dst, float2x2 grad) { *dst += lc_remove_nan(grad); }
[[gnu::always_inline]] inline void lc_accumulate_grad(thread float3x3 *dst, float3x3 grad) { *dst += lc_remove_nan(grad); }
[[gnu::always_inline]] inline void lc_accumulate_grad(thread float4x4 *dst, float4x4 grad) { *dst += lc_remove_nan(grad); }

#define LC_GRAD_SHADOW_VARIABLE(x) auto x##_grad = lc_zero<decltype(x)>()
#define LC_MARK_GRAD(x, dx) x##_grad = dx
#define LC_GRAD(x) (x##_grad)
#define LC_ACCUM_GRAD(x_grad, dx) lc_accumulate_grad(&(x_grad), (dx))
#define LC_REQUIRES_GRAD(x) x##_grad = lc_zero<decltype(x##_grad)>()

template<typename T, size_t N>
inline void lc_accumulate_grad(thread array<T, N> *dst, array<T, N> grad) {
#pragma unroll
    for (auto i = 0u; i < N; i++) { lc_accumulate_grad(&(*dst)[i], grad[i]); }
}

[[nodiscard]] inline auto lc_reduce_sum(short2 v) { return short(v.x + v.y); }
[[nodiscard]] inline auto lc_reduce_prod(short2 v) { return short(v.x * v.y); }
[[nodiscard]] inline auto lc_reduce_min(short2 v) { return short(min(v.x, v.y)); }
[[nodiscard]] inline auto lc_reduce_max(short2 v) { return short(max(v.x, v.y)); }
[[nodiscard]] inline auto lc_reduce_sum(short3 v) { return short(v.x + v.y + v.z); }
[[nodiscard]] inline auto lc_reduce_prod(short3 v) { return short(v.x * v.y * v.z); }
[[nodiscard]] inline auto lc_reduce_min(short3 v) { return short(min(v.x, min(v.y, v.z))); }
[[nodiscard]] inline auto lc_reduce_max(short3 v) { return short(max(v.x, max(v.y, v.z))); }
[[nodiscard]] inline auto lc_reduce_sum(short4 v) { return short(v.x + v.y + v.z + v.w); }
[[nodiscard]] inline auto lc_reduce_prod(short4 v) { return short(v.x * v.y * v.z * v.w); }
[[nodiscard]] inline auto lc_reduce_min(short4 v) { return short(min(v.x, min(v.y, min(v.z, v.w)))); }
[[nodiscard]] inline auto lc_reduce_max(short4 v) { return short(max(v.x, max(v.y, max(v.z, v.w)))); }
[[nodiscard]] inline auto lc_reduce_sum(ushort2 v) { return ushort(v.x + v.y); }
[[nodiscard]] inline auto lc_reduce_prod(ushort2 v) { return ushort(v.x * v.y); }
[[nodiscard]] inline auto lc_reduce_min(ushort2 v) { return ushort(min(v.x, v.y)); }
[[nodiscard]] inline auto lc_reduce_max(ushort2 v) { return ushort(max(v.x, v.y)); }
[[nodiscard]] inline auto lc_reduce_sum(ushort3 v) { return ushort(v.x + v.y + v.z); }
[[nodiscard]] inline auto lc_reduce_prod(ushort3 v) { return ushort(v.x * v.y * v.z); }
[[nodiscard]] inline auto lc_reduce_min(ushort3 v) { return ushort(min(v.x, min(v.y, v.z))); }
[[nodiscard]] inline auto lc_reduce_max(ushort3 v) { return ushort(max(v.x, max(v.y, v.z))); }
[[nodiscard]] inline auto lc_reduce_sum(ushort4 v) { return ushort(v.x + v.y + v.z + v.w); }
[[nodiscard]] inline auto lc_reduce_prod(ushort4 v) { return ushort(v.x * v.y * v.z * v.w); }
[[nodiscard]] inline auto lc_reduce_min(ushort4 v) { return ushort(min(v.x, min(v.y, min(v.z, v.w)))); }
[[nodiscard]] inline auto lc_reduce_max(ushort4 v) { return ushort(max(v.x, max(v.y, max(v.z, v.w)))); }
[[nodiscard]] inline auto lc_reduce_sum(int2 v) { return int(v.x + v.y); }
[[nodiscard]] inline auto lc_reduce_prod(int2 v) { return int(v.x * v.y); }
[[nodiscard]] inline auto lc_reduce_min(int2 v) { return int(min(v.x, v.y)); }
[[nodiscard]] inline auto lc_reduce_max(int2 v) { return int(max(v.x, v.y)); }
[[nodiscard]] inline auto lc_reduce_sum(int3 v) { return int(v.x + v.y + v.z); }
[[nodiscard]] inline auto lc_reduce_prod(int3 v) { return int(v.x * v.y * v.z); }
[[nodiscard]] inline auto lc_reduce_min(int3 v) { return int(min(v.x, min(v.y, v.z))); }
[[nodiscard]] inline auto lc_reduce_max(int3 v) { return int(max(v.x, max(v.y, v.z))); }
[[nodiscard]] inline auto lc_reduce_sum(int4 v) { return int(v.x + v.y + v.z + v.w); }
[[nodiscard]] inline auto lc_reduce_prod(int4 v) { return int(v.x * v.y * v.z * v.w); }
[[nodiscard]] inline auto lc_reduce_min(int4 v) { return int(min(v.x, min(v.y, min(v.z, v.w)))); }
[[nodiscard]] inline auto lc_reduce_max(int4 v) { return int(max(v.x, max(v.y, max(v.z, v.w)))); }
[[nodiscard]] inline auto lc_reduce_sum(uint2 v) { return uint(v.x + v.y); }
[[nodiscard]] inline auto lc_reduce_prod(uint2 v) { return uint(v.x * v.y); }
[[nodiscard]] inline auto lc_reduce_min(uint2 v) { return uint(min(v.x, v.y)); }
[[nodiscard]] inline auto lc_reduce_max(uint2 v) { return uint(max(v.x, v.y)); }
[[nodiscard]] inline auto lc_reduce_sum(uint3 v) { return uint(v.x + v.y + v.z); }
[[nodiscard]] inline auto lc_reduce_prod(uint3 v) { return uint(v.x * v.y * v.z); }
[[nodiscard]] inline auto lc_reduce_min(uint3 v) { return uint(min(v.x, min(v.y, v.z))); }
[[nodiscard]] inline auto lc_reduce_max(uint3 v) { return uint(max(v.x, max(v.y, v.z))); }
[[nodiscard]] inline auto lc_reduce_sum(uint4 v) { return uint(v.x + v.y + v.z + v.w); }
[[nodiscard]] inline auto lc_reduce_prod(uint4 v) { return uint(v.x * v.y * v.z * v.w); }
[[nodiscard]] inline auto lc_reduce_min(uint4 v) { return uint(min(v.x, min(v.y, min(v.z, v.w)))); }
[[nodiscard]] inline auto lc_reduce_max(uint4 v) { return uint(max(v.x, max(v.y, max(v.z, v.w)))); }
[[nodiscard]] inline auto lc_reduce_sum(float2 v) { return float(v.x + v.y); }
[[nodiscard]] inline auto lc_reduce_prod(float2 v) { return float(v.x * v.y); }
[[nodiscard]] inline auto lc_reduce_min(float2 v) { return float(min(v.x, v.y)); }
[[nodiscard]] inline auto lc_reduce_max(float2 v) { return float(max(v.x, v.y)); }
[[nodiscard]] inline auto lc_reduce_sum(float3 v) { return float(v.x + v.y + v.z); }
[[nodiscard]] inline auto lc_reduce_prod(float3 v) { return float(v.x * v.y * v.z); }
[[nodiscard]] inline auto lc_reduce_min(float3 v) { return float(min(v.x, min(v.y, v.z))); }
[[nodiscard]] inline auto lc_reduce_max(float3 v) { return float(max(v.x, max(v.y, v.z))); }
[[nodiscard]] inline auto lc_reduce_sum(float4 v) { return float(v.x + v.y + v.z + v.w); }
[[nodiscard]] inline auto lc_reduce_prod(float4 v) { return float(v.x * v.y * v.z * v.w); }
[[nodiscard]] inline auto lc_reduce_min(float4 v) { return float(min(v.x, min(v.y, min(v.z, v.w)))); }
[[nodiscard]] inline auto lc_reduce_max(float4 v) { return float(max(v.x, max(v.y, max(v.z, v.w)))); }
[[nodiscard]] inline auto lc_reduce_sum(long2 v) { return long(v.x + v.y); }
[[nodiscard]] inline auto lc_reduce_prod(long2 v) { return long(v.x * v.y); }
[[nodiscard]] inline auto lc_reduce_min(long2 v) { return long(min(v.x, v.y)); }
[[nodiscard]] inline auto lc_reduce_max(long2 v) { return long(max(v.x, v.y)); }
[[nodiscard]] inline auto lc_reduce_sum(long3 v) { return long(v.x + v.y + v.z); }
[[nodiscard]] inline auto lc_reduce_prod(long3 v) { return long(v.x * v.y * v.z); }
[[nodiscard]] inline auto lc_reduce_min(long3 v) { return long(min(v.x, min(v.y, v.z))); }
[[nodiscard]] inline auto lc_reduce_max(long3 v) { return long(max(v.x, max(v.y, v.z))); }
[[nodiscard]] inline auto lc_reduce_sum(long4 v) { return long(v.x + v.y + v.z + v.w); }
[[nodiscard]] inline auto lc_reduce_prod(long4 v) { return long(v.x * v.y * v.z * v.w); }
[[nodiscard]] inline auto lc_reduce_min(long4 v) { return long(min(v.x, min(v.y, min(v.z, v.w)))); }
[[nodiscard]] inline auto lc_reduce_max(long4 v) { return long(max(v.x, max(v.y, max(v.z, v.w)))); }
[[nodiscard]] inline auto lc_reduce_sum(ulong2 v) { return ulong(v.x + v.y); }
[[nodiscard]] inline auto lc_reduce_prod(ulong2 v) { return ulong(v.x * v.y); }
[[nodiscard]] inline auto lc_reduce_min(ulong2 v) { return ulong(min(v.x, v.y)); }
[[nodiscard]] inline auto lc_reduce_max(ulong2 v) { return ulong(max(v.x, v.y)); }
[[nodiscard]] inline auto lc_reduce_sum(ulong3 v) { return ulong(v.x + v.y + v.z); }
[[nodiscard]] inline auto lc_reduce_prod(ulong3 v) { return ulong(v.x * v.y * v.z); }
[[nodiscard]] inline auto lc_reduce_min(ulong3 v) { return ulong(min(v.x, min(v.y, v.z))); }
[[nodiscard]] inline auto lc_reduce_max(ulong3 v) { return ulong(max(v.x, max(v.y, v.z))); }
[[nodiscard]] inline auto lc_reduce_sum(ulong4 v) { return ulong(v.x + v.y + v.z + v.w); }
[[nodiscard]] inline auto lc_reduce_prod(ulong4 v) { return ulong(v.x * v.y * v.z * v.w); }
[[nodiscard]] inline auto lc_reduce_min(ulong4 v) { return ulong(min(v.x, min(v.y, min(v.z, v.w)))); }
[[nodiscard]] inline auto lc_reduce_max(ulong4 v) { return ulong(max(v.x, max(v.y, max(v.z, v.w)))); }

[[nodiscard]] inline auto lc_outer_product(float2 a, float2 b) { return float2x2(a * b.x, a * b.y); }
[[nodiscard]] inline auto lc_outer_product(float3 a, float3 b) { return float3x3(a * b.x, a * b.y, a * b.z); }
[[nodiscard]] inline auto lc_outer_product(float4 a, float4 b) { return float4x4(a * b.x, a * b.y, a * b.z, a * b.w); }

template<typename T>
[[nodiscard]] inline thread T &as_ref(const thread T &x) { return const_cast<thread T &>(x); }

template<typename T>
[[nodiscard]] inline threadgroup T &as_ref(const threadgroup T &x) { return const_cast<threadgroup T &>(x); }

template<typename T>
[[nodiscard]] inline device T &as_ref(const device T &x) { return const_cast<device T &>(x); }

template<typename U, typename T>
[[nodiscard]] inline auto bitcast(T x) {
    static_assert(sizeof(U) == sizeof(T), "bitcast requires types of same size");
    return *reinterpret_cast<const thread U *>(&x);
}
