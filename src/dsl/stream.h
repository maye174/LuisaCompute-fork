//
// Created by Mike Smith on 2021/2/15.
//

#pragma once

#include <utility>

#include <core/spin_mutex.h>
#include <runtime/device.h>
#include <runtime/command_buffer.h>

namespace luisa::compute::dsl {

class Stream : public concepts::Noncopyable {

public:
    struct SynchronizeToken {};
    
    class Delegate {
    
    private:
        Stream *_stream;
        CommandBuffer _command_buffer;
        std::function<void()> _callback;
        
    private:
        void _commit() noexcept;
    
    public:
        explicit Delegate(Stream *s) noexcept;
        Delegate(Delegate &&) noexcept = default;
        Delegate &operator=(Delegate &&) noexcept = default;
        ~Delegate() noexcept;
        Delegate &operator<<(CommandHandle cmd) noexcept;
        Delegate &operator<<(std::function<void()> f) noexcept;
        void operator<<(SynchronizeToken) noexcept;
    };

private:
    Device *_device;
    uint64_t _handle;

private:
    friend class Device;
    void _dispatch(CommandBuffer command_buffer, std::function<void()> callback) noexcept;

public:
    explicit Stream(Device &device) noexcept
        : _device{&device}, _handle{device.create_stream()} {}
    Stream(Stream &&s) noexcept;
    ~Stream() noexcept;
    Stream &operator=(Stream &&rhs) noexcept;
    Delegate operator<<(CommandHandle cmd) noexcept;
    Delegate operator<<(std::function<void()> f) noexcept;
    void operator<<(SynchronizeToken);
};

[[nodiscard]] constexpr auto synchronize() noexcept { return Stream::SynchronizeToken{}; }

}// namespace luisa::compute