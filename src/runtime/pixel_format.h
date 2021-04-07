//
// Created by Mike Smith on 2021/3/29.
//

#pragma once

#include <cstdint>

namespace luisa::compute {

enum struct PixelFormat : uint32_t {
    R8U,
    R8U_SRGB,
    RG8U,
    RG8U_SRGB,
    RGBA8U,
    RGBA8U_SRGB,
    R32F,
    RG32F,
    RGBA32F,
};

/*
enum struct PixelFormat : uint32_t {
    RGBA8U_SRGB,

    R32F,
    RG32F,
    RGBA32F,

    R32UInt,
    RG32UInt,
    RGBA32UInt,

    R32Int,
    RG32Int,
    RGBA32Int,

    R16F,
    RG16F,
    RGBA16F,

    R16UInt,
    RG16UInt,
    RGBA16UInt,

    R16UNorm,
    RG16UNorm,
    RGBA16UNorm,

    R16SNorm,
    RG16SNorm,
    RGBA16SNorm,
    R16Int,
    RG16Int,
    RGBA16Int,

    R8UInt,
    RG8UInt,
    RGBA8UInt,

    R8SNorm,
    RG8SNorm,
    RGBA8SNorm,

    R8Int,
    RG8Int,
    RGBA8Int,
};
*/

[[nodiscard]] constexpr auto pixel_size_bytes(PixelFormat format) noexcept {
    switch (format) {
        case PixelFormat::R8U:
        case PixelFormat::R8U_SRGB:
            return sizeof(uint8_t);
        case PixelFormat::RG8U:
        case PixelFormat::RG8U_SRGB:
            return sizeof(uint8_t) * 2u;
        case PixelFormat::RGBA8U:
        case PixelFormat::RGBA8U_SRGB:
            return sizeof(uint8_t) * 4u;
        case PixelFormat::R32F:
            return sizeof(float);
        case PixelFormat::RG32F:
            return sizeof(float) * 2u;
        case PixelFormat::RGBA32F:
            return sizeof(float) * 4u;
    }
    return static_cast<size_t>(0u);
}

}