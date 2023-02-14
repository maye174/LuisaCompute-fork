
#include <DXRuntime/CommandQueue.h>
#include <DXRuntime/CommandBuffer.h>
#include <DXRuntime/CommandAllocator.h>
#include <Resource/GpuAllocator.h>
#include <DXApi/LCEvent.h>
namespace toolhub::directx {
CommandQueue::CommandQueue(
    Device *device,
    GpuAllocator *resourceAllocator,
    D3D12_COMMAND_LIST_TYPE type)
    : device(device),
      resourceAllocator(resourceAllocator),
      type(type),
      thd([this] { ExecuteThread(); }) {
    auto CreateQueue = [&] {
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = type;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT;
        ThrowIfFailed(device->device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(queueComPtr.GetAddressOf())));
        queue = queueComPtr.Get();
    };
    if (device->deviceSettings) {
        queue = device->deviceSettings->CreateQueue(type);
        if (!queue) [[unlikely]] {
            CreateQueue();
        }
    } else {
        CreateQueue();
    }
    ThrowIfFailed(device->device->CreateFence(
        0,
        D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&cmdFence)));
}
CommandQueue::AllocatorPtr CommandQueue::CreateAllocator(size_t maxAllocCount) {
    if (maxAllocCount != std::numeric_limits<size_t>::max()) {
        std::unique_lock lck(mtx);
        while (lastFrame - executedFrame > maxAllocCount) {
            mainCv.wait(lck);
        }
    }
    auto newPtr = allocatorPool.Pop();
    if (newPtr) {
        return std::move(*newPtr);
    }
    return AllocatorPtr(new CommandAllocator(device, resourceAllocator, type));
}

void CommandQueue::AddEvent(LCEvent const *evt) {
    executedAllocators.Push(evt, evt->fenceIndex);
    mtx.lock();
    mtx.unlock();
    waitCv.notify_one();
}
void CommandQueue::Callback(vstd::function<void()> &&f) {
    auto curFrame = ++lastFrame;
    executedAllocators.Push(std::move(f), curFrame);
    mtx.lock();
    mtx.unlock();
    waitCv.notify_one();
}
void CommandQueue::ExecuteThread() {
    while (enabled) {
        auto ExecuteAllocator = [&](auto &b) {
            b.first->Complete(this, cmdFence.Get(), b.second);
            b.first->Reset(this);
            allocatorPool.Push(std::move(b.first));
            {
                std::lock_guard lck(mtx);
                executedFrame = b.second;
            }
            mainCv.notify_all();
        };
        auto ExecuteCallback = [&](auto &b) {
            b.first();
            {
                std::lock_guard lck(mtx);
                executedFrame = b.second;
            }
            mainCv.notify_all();
        };
        auto ExecuteCallbacks = [&](auto &vec) {
            for (auto &&i : vec.first) {
                i();
            }
            {
                std::lock_guard lck(mtx);
                executedFrame = vec.second;
            }
            mainCv.notify_all();
        };

        auto ExecuteEvent = [&](auto &pair) {
            auto evt = pair.first;
            auto tarFrame = pair.second;
            device->WaitFence(evt->fence.Get(), tarFrame);
            {
                std::lock_guard lck(evt->eventMtx);
                evt->finishedEvent++;
            }
            evt->cv.notify_all();
        };
        while (auto b = executedAllocators.Pop()) {
            b->multi_visit(
                ExecuteAllocator,
                ExecuteCallback,
                ExecuteCallbacks,
                ExecuteEvent);
        }
        std::unique_lock lck(mtx);
        while (enabled && executedAllocators.Length() == 0) {
            waitCv.wait(lck);
        }
    }
}
void CommandQueue::ForceSync(
    AllocatorPtr &alloc,
    CommandBuffer &cb) {
    cb.Close();
    Complete();
    auto curFrame = ++lastFrame;
    alloc->Execute(this, cmdFence.Get(), curFrame);
    alloc->Complete(this, cmdFence.Get(), curFrame);
    alloc->Reset(this);
    {
        std::lock_guard lck(mtx);
        executedFrame = curFrame;
    }
    cb.Reset();
}
CommandQueue::~CommandQueue() {
    {
        std::lock_guard lck(mtx);
        enabled = false;
    }
    waitCv.notify_one();
    thd.join();
}
void CommandQueue::WaitFrame(uint64 lastFrame) {
    if (lastFrame > 0)
        queue->Wait(cmdFence.Get(), lastFrame);
}

template<typename Func>
uint64 CommandQueue::_Execute(AllocatorPtr &&alloc, Func &&callback) {
    auto curFrame = ++lastFrame;
    alloc->Execute(this, cmdFence.Get(), curFrame);
    executedAllocators.Push(std::move(alloc), curFrame);
    curFrame = ++lastFrame;
    executedAllocators.Push(std::move(callback), curFrame);
    mtx.lock();
    mtx.unlock();
    waitCv.notify_one();
    return curFrame;
}

uint64 CommandQueue::Execute(AllocatorPtr &&alloc) {
    auto curFrame = ++lastFrame;
    alloc->Execute(this, cmdFence.Get(), curFrame);
    executedAllocators.Push(std::move(alloc), curFrame);
    mtx.lock();
    mtx.unlock();
    waitCv.notify_one();
    return curFrame;
}
uint64 CommandQueue::ExecuteCallback(AllocatorPtr &&alloc, vstd::function<void()> &&callback) {
    return _Execute(std::move(alloc), std::move(callback));
}
uint64 CommandQueue::ExecuteCallbacks(AllocatorPtr &&alloc, vstd::fixed_vector<vstd::function<void()>, 1> &&callbacks) {
    return _Execute(std::move(alloc), std::move(callbacks));
}
void CommandQueue::ExecuteEmpty(AllocatorPtr &&alloc) {
    alloc->Reset(this);
    allocatorPool.Push(std::move(alloc));
}

void CommandQueue::ExecuteEmptyCallbacks(AllocatorPtr &&alloc, vstd::fixed_vector<vstd::function<void()>, 1> &&callbacks) {
    alloc->Reset(this);
    allocatorPool.Push(std::move(alloc));
    auto curFrame = ++lastFrame;
    executedAllocators.Push(std::move(callbacks), curFrame);
    mtx.lock();
    mtx.unlock();
    waitCv.notify_one();
}

uint64 CommandQueue::ExecuteAndPresent(AllocatorPtr &&alloc, IDXGISwapChain3 *swapChain, bool vsync) {
    auto curFrame = ++lastFrame;
    alloc->ExecuteAndPresent(this, cmdFence.Get(), curFrame, swapChain, vsync);
    executedAllocators.Push(std::move(alloc), curFrame);
    mtx.lock();
    mtx.unlock();
    waitCv.notify_one();
    return curFrame;
}

void CommandQueue::Complete(uint64 fence) {
    std::unique_lock lck(mtx);
    while (executedFrame < fence) {
        mainCv.wait(lck);
    }
}
void CommandQueue::Complete() {
    std::unique_lock lck(mtx);
    while (executedAllocators.Length() > 0) {
        mainCv.wait(lck);
    }
}

}// namespace toolhub::directx