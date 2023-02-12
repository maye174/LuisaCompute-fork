#pragma once
#ifndef LC_DISABLE_DSL
#include <dsl/struct.h>
#include <dsl/builtin.h>
#include <dsl/var.h>
#include <rtx/hit.h>
namespace luisa::compute {
[[nodiscard]] LC_RUNTIME_API Var<float> interpolate(Expr<Hit> hit, Expr<float> a, Expr<float> b, Expr<float> c) noexcept;
[[nodiscard]] LC_RUNTIME_API Var<float2> interpolate(Expr<Hit> hit, Expr<float2> a, Expr<float2> b, Expr<float2> c) noexcept;
[[nodiscard]] LC_RUNTIME_API Var<float3> interpolate(Expr<Hit> hit, Expr<float3> a, Expr<float3> b, Expr<float3> c) noexcept;
[[nodiscard]] LC_RUNTIME_API Var<float4> interpolate(Expr<Hit> hit, Expr<float4> a, Expr<float4> b, Expr<float4> c) noexcept;
}// namespace luisa::compute
// clang-format off
LUISA_STRUCT_EXT(luisa::compute::Hit) {
    [[nodiscard]] auto miss() const noexcept {
        return hit_type == static_cast<uint32_t>(luisa::compute::HitType::Miss);
    }
    [[nodiscard]] auto hit_triangle() const noexcept {
        return hit_type == static_cast<uint32_t>(luisa::compute::HitType::Triangle);
    }
    [[nodiscard]] auto hit_procedural() const noexcept {
        return hit_type == static_cast<uint32_t>(luisa::compute::HitType::Procedural);
    }
    template<typename A, typename B, typename C>
    [[nodiscard]] auto interpolate(A &&a, B &&b, C &&c) const noexcept {
        return luisa::compute::interpolate(
            *this,
            std::forward<A>(a),
            std::forward<B>(b),
            std::forward<C>(c));
    }
};
// clang-format on
#endif