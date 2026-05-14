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

class VulkanGpuTimerBackend final : public GpuTimerBackend
{
public:
    VulkanGpuTimerBackend();
    ~VulkanGpuTimerBackend() override;

    bool Initialize(DeviceBase* device) override;
    void Destroy() override;

    bool IsSupported() const override;
    double GetTimestampPeriod() const override;

    void RecordFrameStart(CommandBufferBase* cmd, uint32 frameIndex) override;
    void WriteTimestamp(CommandBufferBase* cmd, uint32 frameIndex, QueryIndex queryIndex) override;
    void RecordFrameEnd(CommandBufferBase* cmd, uint32 frameIndex) override;

    GpuFrameTimings ResolveFrameResults(uint32 completedFrameIndex) override;

private:
    double ComputeDeltaMs(uint64 start, uint64 end) const;

    struct PerFrameState
    {
        VkQueryPool queryPool = VK_NULL_HANDLE;
        bool resultsPending = false;
    };

    VulkanDevice* m_device = nullptr;
    FixedArray<PerFrameState, NumFramesInFlight> m_frames;
    double m_timestampPeriod = 0.0;
    bool m_isSupported = false;
};

} // namespace Hyperion
