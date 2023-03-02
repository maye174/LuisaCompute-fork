//
// Created by Mike Smith on 2021/4/18.
//

#pragma once

#include <runtime/pixel.h>
#include <runtime/resource.h>
#include <runtime/mipmap.h>
#include <runtime/sampler.h>
#include <runtime/device_interface.h>

namespace luisa::compute {

namespace detail {

template<typename VolumeOrView>
struct VolumeExprProxy;

LC_RUNTIME_API void error_volume_invalid_mip_levels(size_t level, size_t mip) noexcept;

}// namespace detail

template<typename T>
class VolumeView;

// Volumes are 3D textures without sampling, i.e., 3D surfaces.
template<typename T>
class Volume final : public Resource {

    static_assert(std::disjunction_v<
                  std::is_same<T, int>,
                  std::is_same<T, uint>,
                  std::is_same<T, float>>);

private:
    PixelStorage _storage{};
    uint _mip_levels{};
    uint3 _size{};

private:
    friend class Device;
    friend class ResourceGenerator;
    Volume(DeviceInterface *device, const ResourceCreationInfo &create_info, PixelStorage storage, uint3 size, uint mip_levels) noexcept
        : Resource{
              device, Tag::TEXTURE,
              create_info},
          _storage{storage}, _mip_levels{detail::max_mip_levels(size, mip_levels)}, _size{size} {
    }
    Volume(DeviceInterface *device, PixelStorage storage, uint3 size, uint mip_levels = 1u) noexcept
        : Resource{
              device, Tag::TEXTURE,
              device->create_texture(
                  pixel_storage_to_format<T>(storage), 3u,
                  size.x, size.y, size.z,
                  detail::max_mip_levels(size, mip_levels))},
          _storage{storage}, _mip_levels{detail::max_mip_levels(size, mip_levels)}, _size{size} {}

public:
    Volume() noexcept = default;
    using Resource::operator bool;
    Volume(Volume &&) noexcept = default;
    Volume(Volume const &) noexcept = delete;
    Volume &operator=(Volume &&) noexcept = default;
    Volume &operator=(Volume const &) noexcept = delete;
    // properties
    [[nodiscard]] auto mip_levels() const noexcept { return _mip_levels; }
    [[nodiscard]] auto size() const noexcept { return _size; }
    [[nodiscard]] auto size_bytes() const noexcept {
        size_t byte_size = 0;
        auto size = _size;
        for (size_t i = 0; i < _mip_levels; ++i) {
            byte_size += pixel_storage_size(_storage, size.x, size.y, size.z);
            size >>= uint3(1);
        }
        return byte_size;
    }
    [[nodiscard]] auto storage() const noexcept { return _storage; }
    [[nodiscard]] auto format() const noexcept { return pixel_storage_to_format<T>(_storage); }
    [[nodiscard]] auto view(uint32_t level) const noexcept {
        if (level >= _mip_levels) [[unlikely]] {
            detail::error_volume_invalid_mip_levels(level, _mip_levels);
        }
        auto mip_size = luisa::max(_size >> level, 1u);
        return VolumeView<T>{handle(), _storage, level, mip_size};
    }
    [[nodiscard]] auto view() const noexcept { return view(0u); }

    // commands
    // copy image's data to pointer or another image
    template<typename U>
    [[nodiscard]] auto copy_to(U &&dst) const noexcept {
        return this->view(0).copy_to(std::forward<U>(dst));
    }
    // copy pointer or another image's data to image
    template<typename U>
    [[nodiscard]] auto copy_from(U &&dst) const noexcept {
        return this->view(0).copy_from(std::forward<U>(dst));
    }
    // DSL interface
    [[nodiscard]] auto operator->() const noexcept {
        return reinterpret_cast<const detail::VolumeExprProxy<Volume<T>> *>(this);
    }
};

template<typename T>
class VolumeView {

private:
    uint64_t _handle;
    PixelStorage _storage;
    uint _level;
    uint3 _size;

private:
    friend class Volume<T>;
    friend class detail::MipmapView;
    friend class ResourceGenerator;

    constexpr explicit VolumeView(
        uint64_t handle, PixelStorage storage, uint level, uint3 size) noexcept
        : _handle{handle}, _storage{storage}, _level{level}, _size{size} {}

    [[nodiscard]] auto _as_mipmap() const noexcept {
        return detail::MipmapView{
            _handle, _size, _level, _storage};
    }

public:
    VolumeView(const Volume<T> &volume) noexcept : VolumeView{volume.view(0u)} {}
    // properties
    [[nodiscard]] auto handle() const noexcept { return _handle; }
    [[nodiscard]] auto size() const noexcept { return _size; }
    [[nodiscard]] auto size_bytes() const noexcept {
        return pixel_storage_size(_storage, _size.x, _size.y, _size.z);
    }
    [[nodiscard]] auto storage() const noexcept { return _storage; }
    [[nodiscard]] auto level() const noexcept { return _level; }
    [[nodiscard]] auto format() const noexcept { return pixel_storage_to_format<T>(_storage); }
    // commands
    // copy image's data to pointer or another image
    template<typename U>
    [[nodiscard]] auto copy_to(U &&dst) const noexcept { return _as_mipmap().copy_to(std::forward<U>(dst)); }
    // copy pointer or another image's data to image
    template<typename U>
    [[nodiscard]] auto copy_from(U &&src) const noexcept { return _as_mipmap().copy_from(std::forward<U>(src)); }
    // DSL interface
    [[nodiscard]] auto operator->() const noexcept {
        return reinterpret_cast<const detail::VolumeExprProxy<VolumeView<T>> *>(this);
    }
};

template<typename T>
VolumeView(const Volume<T> &) -> VolumeView<T>;

template<typename T>
VolumeView(VolumeView<T>) -> VolumeView<T>;

template<typename T>
struct is_volume : std::false_type {};

template<typename T>
struct is_volume<Volume<T>> : std::true_type {};

template<typename T>
struct is_volume_view : std::false_type {};

template<typename T>
struct is_volume_view<VolumeView<T>> : std::true_type {};

template<typename T>
using is_volume_or_view = std::disjunction<is_volume<T>, is_volume_view<T>>;

template<typename T>
constexpr auto is_volume_v = is_volume<T>::value;

template<typename T>
constexpr auto is_volume_view_v = is_volume_view<T>::value;

template<typename T>
constexpr auto is_volume_or_view_v = is_volume_or_view<T>::value;

namespace detail {

template<typename VolumeOrView>
struct volume_element_impl {
    static_assert(always_false_v<VolumeOrView>);
};

template<typename T>
struct volume_element_impl<Volume<T>> {
    using type = T;
};

template<typename T>
struct volume_element_impl<VolumeView<T>> {
    using type = T;
};

}// namespace detail

template<typename VolumeOrView>
using volume_element = detail::volume_element_impl<std::remove_cvref_t<VolumeOrView>>;

template<typename VolumeOrView>
using volume_element_t = typename volume_element<VolumeOrView>::type;

}// namespace luisa::compute
