#include <core/mathematics.h>
#include <runtime/command_reorder_visitor.h>
#include <runtime/stream.h>
namespace luisa::compute {
CommandReorderVisitor::ResourceHandle *CommandReorderVisitor::GetHandle(
    uint64_t tarGetHandle,
    ResourceType target_type) {
    auto func = [&](auto &&map) {
        auto tryResult = map.TryEmplace(
            tarGetHandle);
        auto &&value = tryResult.first.Value();
        if (tryResult.second) {
            value = handlePool.New();
            value->handle = tarGetHandle;
            value->type = target_type;
        }
        return value;
    };
    auto accelFunc = [&] {
        auto tryResult = accelMap.TryEmplace(
            tarGetHandle);
        auto &&value = tryResult.first.Value();
        if (tryResult.second) {
            value = handlePool.New();
            value->handle = tarGetHandle;
            value->type = target_type;
            for (auto &&i : meshMap) {
                if (device->is_mesh_in_accel(tarGetHandle, i.first)) {
                    i.second->belonedAccel.emplace_back(value);
                    value->writeLayer = std::max<int64_t>(value->writeLayer, i.second->writeLayer);
                }
            }
        }
        return value;
    };
    auto meshFunc = [&] {
        auto tryResult = meshMap.TryEmplace(
            tarGetHandle);
        auto &&value = tryResult.first.Value();
        if (tryResult.second) {
            auto newMeshValue = meshHandlePool.New();
            value = newMeshValue;
            value->handle = tarGetHandle;
            value->type = target_type;
            for (auto &&i : accelMap) {
                if (device->is_mesh_in_accel(i.first, tarGetHandle)) {
                    newMeshValue->belonedAccel.emplace_back(i.second);
                    newMeshValue->writeLayer = std::max<int64_t>(newMeshValue->writeLayer, i.second->writeLayer);
                }
            }
        }
        return value;
    };
    switch (target_type) {
        case ResourceType::Mesh:
            return meshFunc();
        case ResourceType::Bindless:
            return func(bindlessMap);
        case ResourceType::Accel:
            return accelFunc();
        default:
            return func(resMap);
    }
}
size_t CommandReorderVisitor::GetLastLayerWrite(ResourceHandle *handle) const {
    size_t layer = std::max<int64_t>(handle->readLayer + 1, handle->writeLayer + 1);
    switch (handle->type) {
        case ResourceType::Buffer:
            if (bindlessMaxLayer >= layer) {
                for (auto &&i : bindlessMap) {
                    if (device->is_buffer_in_bindless_array(i.first, handle->handle)) {
                        layer = std::max<int64_t>(layer, i.second->readLayer + 1);
                    }
                }
            }
            if (accelMaxLayer >= layer) {
                for (auto &&i : accelMap) {
                    if (device->is_buffer_in_accel(i.first, handle->handle)) {
                        layer = std::max<int64_t>(layer, i.second->readLayer + 1);
                    }
                }
            }
            break;
        case ResourceType::Texture:
            for (auto &&i : bindlessMap) {
                if (device->is_texture_in_bindless_array(i.first, handle->handle)) {
                    layer = std::max<int64_t>(layer, i.second->readLayer + 1);
                }
            }
            break;
        case ResourceType::Mesh:
            if (bindlessMaxLayer >= layer) {
                if (accelMaxLayer >= layer) {
                    for (auto &&i : accelMap) {
                        if (device->is_mesh_in_accel(i.first, handle->handle)) {
                            layer = std::max<int64_t>(layer, i.second->readLayer + 1);
                        }
                    }
                }
            }
            break;
        default: break;
    }
    return layer;
}
size_t CommandReorderVisitor::GetLastLayerRead(ResourceHandle *handle) const {
    size_t layer = handle->writeLayer + 1;
    return layer;
}
CommandReorderVisitor::CommandReorderVisitor(Device::Interface *device)
    : device(device),
      handlePool(256, true),
      meshHandlePool(256, true),
      resMap(256),
      bindlessMap(256),
      accelMap(256),
      meshMap(256) {}

CommandReorderVisitor::~CommandReorderVisitor() {
}
size_t CommandReorderVisitor::SetRead(
    uint64_t handle,
    ResourceType type) {
    auto srcHandle = GetHandle(
        handle,
        type);
    auto layer = GetLastLayerRead(srcHandle);
    srcHandle->readLayer = layer;
    return layer;
}
size_t CommandReorderVisitor::SetWrite(
    uint64_t handle,
    ResourceType type) {
    auto dstHandle = GetHandle(
        handle,
        type);
    auto layer = GetLastLayerWrite(dstHandle);
    dstHandle->writeLayer = layer;
    return layer;
}
size_t CommandReorderVisitor::SetRW(
    uint64_t read_handle,
    ResourceType read_type,
    uint64_t write_handle,
    ResourceType write_type) {
    auto srcHandle = GetHandle(
        read_handle,
        read_type);
    auto layer = GetLastLayerRead(srcHandle);
    auto dstHandle = GetHandle(
        write_handle,
        write_type);
    layer = std::max<int64_t>(layer, GetLastLayerWrite(dstHandle));
    srcHandle->readLayer = layer;
    dstHandle->writeLayer = layer;
    return layer;
}
void CommandReorderVisitor::visit(const BufferUploadCommand *command) noexcept {
    AddCommand(command, SetWrite(command->handle(), ResourceType::Buffer));
}
void CommandReorderVisitor::visit(const BufferDownloadCommand *command) noexcept {
    AddCommand(command, SetRead(command->handle(), ResourceType::Buffer));
}
void CommandReorderVisitor::visit(const BufferCopyCommand *command) noexcept {
    AddCommand(command, SetRW(command->src_handle(), ResourceType::Buffer, command->dst_handle(), ResourceType::Buffer));
}
void CommandReorderVisitor::visit(const BufferToTextureCopyCommand *command) noexcept {
    AddCommand(command, SetRW(command->buffer(), ResourceType::Buffer, command->texture(), ResourceType::Texture));
}

// Shader : function, read/write multi resources
void CommandReorderVisitor::visit(const ShaderDispatchCommand *command) noexcept {
    dispatchReadHandle.clear();
    dispatchWriteHandle.clear();
    useAccelInPass = false;
    useBindlessInPass = false;
    f = command->kernel();
    arg = command->kernel().arguments().data();
    dispatchLayer = 0;
    command->decode(*this);
    for (auto &&i : dispatchReadHandle) {
        i->readLayer = dispatchLayer;
    }
    for (auto &&i : dispatchWriteHandle) {
        i->writeLayer = dispatchLayer;
    }
    AddCommand(command, dispatchLayer);
    if (useAccelInPass) {
        accelMaxLayer = std::max<int64_t>(accelMaxLayer, dispatchLayer);
    }
    if (useBindlessInPass) {
        bindlessMaxLayer = std::max<int64_t>(bindlessMaxLayer, dispatchLayer);
    }
}
// Texture : resource
void CommandReorderVisitor::visit(const TextureUploadCommand *command) noexcept {
    AddCommand(command, SetWrite(command->handle(), ResourceType::Texture));
}
void CommandReorderVisitor::visit(const TextureDownloadCommand *command) noexcept {
    AddCommand(command, SetRead(command->handle(), ResourceType::Texture));
}
void CommandReorderVisitor::visit(const TextureCopyCommand *command) noexcept {
    AddCommand(command, SetRW(command->src_handle(), ResourceType::Texture, command->dst_handle(), ResourceType::Texture));
}
void CommandReorderVisitor::visit(const TextureToBufferCopyCommand *command) noexcept {
    AddCommand(command, SetRW(command->texture(), ResourceType::Texture, command->buffer(), ResourceType::Buffer));
}

// BindlessArray : read multi resources
void CommandReorderVisitor::visit(const BindlessArrayUpdateCommand *command) noexcept {
    AddCommand(command, SetWrite(command->handle(), ResourceType::Bindless));
}

// Accel : conclude meshes and their buffer
void CommandReorderVisitor::visit(const AccelUpdateCommand *command) noexcept {
    AddCommand(command, SetWrite(command->handle(), ResourceType::Accel));
}
void CommandReorderVisitor::visit(const AccelBuildCommand *command) noexcept {
    AddCommand(command, SetWrite(command->handle(), ResourceType::Accel));
}

// Mesh : conclude vertex and triangle buffers
void CommandReorderVisitor::visit(const MeshUpdateCommand *command) noexcept {
    AddCommand(
        command,
        SetMesh(command->handle(),
                device->get_vertex_buffer_from_mesh(command->handle()),
                device->get_triangle_buffer_from_mesh(command->handle())));
}
size_t CommandReorderVisitor::SetMesh(
    uint64_t handle,
    uint64_t vb,
    uint64_t ib) {
    auto vbHandle = GetHandle(
        vb,
        ResourceType::Buffer);
    auto meshHandle = GetHandle(
        handle,
        ResourceType::Mesh);
    auto layer = GetLastLayerRead(vbHandle);
    layer = std::max<int64_t>(layer, GetLastLayerWrite(meshHandle));
    if (ib != vb) {
        auto ibHandle = GetHandle(
            ib,
            ResourceType::Buffer);
        layer = std::max<int64_t>(layer, GetLastLayerRead(ibHandle));
        ibHandle->readLayer = layer;
    }
    vbHandle->readLayer = layer;
    meshHandle->writeLayer = layer;
    for (auto &&i : static_cast<ResourceMeshHandle *>(meshHandle)->belonedAccel) {
        i->writeLayer = layer;
    }
    return layer;
}
void CommandReorderVisitor::visit(const MeshBuildCommand *command) noexcept {
    AddCommand(
        command,
        SetMesh(command->handle(),
                device->get_vertex_buffer_from_mesh(command->handle()),
                device->get_triangle_buffer_from_mesh(command->handle())));
}
void CommandReorderVisitor::clear() {
    for (auto &&i : resMap) {
        handlePool.Delete(i.second);
    }
    for (auto &&i : bindlessMap) {
        handlePool.Delete(i.second);
    }
    for (auto &&i : accelMap) {
        handlePool.Delete(i.second);
    }
    for (auto &&i : meshMap) {
        meshHandlePool.Delete(i.second);
    }
    resMap.Clear();
    bindlessMap.Clear();
    accelMap.Clear();
    meshMap.Clear();
    accelMaxLayer = -1;
    bindlessMaxLayer = -1;
    eastl::span<CommandList> sp(commandLists.data(), layerCount);
    for (auto &&i : sp) {
        i.clear();
    }
    layerCount = 0;
}

void CommandReorderVisitor::AddCommand(Command const *cmd, size_t layer) {
    if (commandLists.size() <= layer) {
        commandLists.resize(layer + 1);
    }
    layerCount = std::max(layerCount, layer + 1);
    commandLists[layer].append(const_cast<Command *>(cmd));
}

void CommandReorderVisitor::AddDispatchHandle(
    uint64_t handle,
    ResourceType type,
    bool isWrite) {
    if (isWrite) {
        auto h = GetHandle(
            handle,
            type);
        dispatchLayer = std::max<int64_t>(dispatchLayer, GetLastLayerWrite(h));
        dispatchWriteHandle.emplace_back(h);
    } else {
        auto h = GetHandle(
            handle,
            type);
        dispatchLayer = std::max<int64_t>(dispatchLayer, GetLastLayerRead(h));
        dispatchReadHandle.emplace_back(h);
    }
}
void CommandReorderVisitor::operator()(uint uid, ShaderDispatchCommand::TextureArgument const &bf) {
    AddDispatchHandle(
        bf.handle,
        ResourceType::Texture,
        ((uint)f.variable_usage(arg->uid()) & (uint)Usage::WRITE) != 0);
    arg++;
}
void CommandReorderVisitor::operator()(uint uid, ShaderDispatchCommand::BufferArgument const &bf) {
    AddDispatchHandle(
        bf.handle,
        ResourceType::Buffer,
        ((uint)f.variable_usage(arg->uid()) & (uint)Usage::WRITE) != 0);
    arg++;
}
void CommandReorderVisitor::operator()(uint uid, vstd::span<std::byte const> bf) {
    arg++;
}
void CommandReorderVisitor::operator()(uint uid, ShaderDispatchCommand::BindlessArrayArgument const &bf) {
    useBindlessInPass = true;
    AddDispatchHandle(
        bf.handle,
        ResourceType::Bindless,
        false);
    arg++;
}
void CommandReorderVisitor::operator()(uint uid, ShaderDispatchCommand::AccelArgument const &bf) {
    useAccelInPass = true;
    AddDispatchHandle(
        bf.handle,
        ResourceType::Accel,
        false);
    arg++;
}

}// namespace luisa::compute