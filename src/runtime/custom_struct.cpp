#include <runtime/device.h>
#include <runtime/buffer.h>
#include <runtime/custom_struct.h>

namespace luisa::compute {

//template<typename T>
//Buffer<T> Device::create_indirect_dispatch_buffer(size_t capacity) noexcept {
//    Buffer<T> v;
//    // Resource
//    v._device = _impl;
//    auto ptr = _impl.get();
//    auto buffer = ptr->create_indirect_dispatch_buffer(capacity);
//    v._handle = buffer.handle;
//    v._tag = Resource::Tag::BUFFER;
//    // Buffer
//    v._size = buffer.size / Type::custom_struct_size;
//    return v;
//}
//
//Buffer<DispatchArgs> Device::create_1d_dispatch_buffer(size_t capacity) noexcept {
//    return create_dispatch_buffer<1, DispatchArgs>(capacity);
//}
//
//Buffer<DispatchArgs> Device::create_2d_dispatch_buffer(size_t capacity) noexcept {
//    return create_dispatch_buffer<2, DispatchArgs>(capacity);
//}
//
//Buffer<DispatchArgs> Device::create_3d_dispatch_buffer(size_t capacity) noexcept {
//    return create_dispatch_buffer<3, DispatchArgs>(capacity);
//}
//
//Buffer<AABB> Device::create_aabb_buffer(size_t capacity) noexcept {
//    Buffer<AABB> v;
//    v._device = _impl;
//    auto ptr = _impl.get();
//    auto buffer = ptr->create_aabb_buffer(capacity);
//    v._handle = buffer.handle;
//    v._tag = Resource::Tag::BUFFER;
//    // Buffer
//    v._size = buffer.size / Type::custom_struct_size;
//    return v;
//}

}// namespace luisa::compute
