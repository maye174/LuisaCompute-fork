
#include <DXApi/LCEvent.h>
#include <DXRuntime/CommandQueue.h>
#include <DXRuntime/DStorageCommandQueue.h>
namespace lc::dx {
LCEvent::LCEvent(Device *device)
    : Resource(device) {
    ThrowIfFailed(device->device->CreateFence(
        0,
        D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&fence)));
}
LCEvent::~LCEvent() {
}

void LCEvent::Sync() const {
    std::unique_lock lck(eventMtx);
    while (finishedEvent < lastFence) {
        cv.wait(lck);
    }
}
void LCEvent::Signal(CommandQueue *queue, uint64 fenceIdx) const {
    std::lock_guard lck(eventMtx);
    queue->Queue()->Signal(fence.Get(), fenceIdx);
    lastFence = std::max(lastFence, fenceIdx);
    queue->AddEvent(this, fenceIdx);
}
void LCEvent::Signal(DStorageCommandQueue *queue, uint64 fenceIdx) const {
    std::lock_guard lck(eventMtx);
    queue->Signal(fence.Get(), fenceIdx);
    lastFence = std::max(lastFence, fenceIdx);
    queue->AddEvent(this, fenceIdx);
}
void LCEvent::Wait(CommandQueue *queue, uint64 fenceIdx) const {
    std::lock_guard lck(eventMtx);
    queue->Queue()->Wait(fence.Get(), std::min(fenceIdx, lastFence));
}
bool LCEvent::IsComplete(uint64 fenceIdx) const {
    std::lock_guard lck(eventMtx);
    return finishedEvent >= std::min(fenceIdx, lastFence);
}
}// namespace lc::dx
