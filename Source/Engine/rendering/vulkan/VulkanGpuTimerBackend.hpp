/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/GpuTimerBackend.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Core/Constants.hpp>
#include <Core/containers/FixedArray.hpp>

#include <vulkan/vulkan.h>

namespace Hyperion {

class VulkanDevice;
class EngineStatGpuTimer;

class VulkanGpuTimerBackend final : public GpuTimerBackend
{
public:
    VulkanGpuTimerBackend();
    ~VulkanGpuTimerBackend() override;

    bool Initialize(DeviceBase* device) override;
    void Destroy() override;

    bool IsSupported() const override;
    double GetTimestampPeriod() const override;

    void WriteTimestamp(CommandBufferBase* cmd, uint32 frameIndex, EngineStatGpuTimer* timer, bool isStart) override;

    void ResolveFrameResults(uint32 completedFrameIndex) override;

private:
    double ComputeDeltaMs(uint64 start, uint64 end) const;

    uint32 GetOrCreateQuerySlot(EngineStatGpuTimer* timer);

    struct PerFrameState
    {
        VkQueryPool queryPool = VK_NULL_HANDLE;
        bool resultsPending = false;
    };

    static constexpr uint32 MaxGpuTimers = 32;
    static constexpr uint32 MaxGpuQueriesPerFrame = MaxGpuTimers * 2;

    VulkanDevice* m_device = nullptr;
    FixedArray<PerFrameState, NumFramesInFlight> m_frames;
    FixedArray<EngineStatGpuTimer*, MaxGpuTimers> m_timerSlots;
    uint32 m_numTimerSlots = 0;
    double m_timestampPeriod = 0.0;
    bool m_isSupported = false;
};

} // namespace Hyperion
