//
// Created by Mike Smith on 2021/2/2.
//

#pragma once

#include <core/stl/memory.h>
#include <core/stl/string.h>
#include <core/stl/hash.h>

namespace luisa {
class DynamicModule;
}

namespace luisa::compute {

class Device;
class BinaryIO;
class ContextPaths;

struct DeviceConfig {
    Hash128 hash;
    size_t device_index{0ull};
    bool inqueue_buffer_limit{true};
    bool headless{false};
};

class LC_RUNTIME_API Context {

    friend class ContextPaths;

private:
    struct Impl;
    luisa::shared_ptr<Impl> _impl;
    explicit Context(luisa::shared_ptr<Impl> impl) noexcept;

public:
    explicit Context(string_view program_path) noexcept;
    explicit Context(const char *program_path) noexcept
        : Context{string_view{program_path}} {}
    ~Context() noexcept;
    Context(Context &&) noexcept = default;
    Context(const Context &) noexcept = default;
    Context &operator=(Context &&) noexcept = default;
    Context &operator=(const Context &) noexcept = default;
    [[nodiscard]] ContextPaths paths() const noexcept;
    [[nodiscard]] Device create_device(luisa::string_view backend_name,
                                       DeviceConfig const *settings = nullptr) noexcept;
    [[nodiscard]] luisa::span<const luisa::string> installed_backends() const noexcept;
    [[nodiscard]] luisa::span<const DynamicModule> loaded_modules() const noexcept;
    [[nodiscard]] Device create_default_device() noexcept;
    [[nodiscard]] BinaryIO *file_io() const noexcept;
    void set_file_io(BinaryIO *file_io) noexcept;
};

}// namespace luisa::compute
