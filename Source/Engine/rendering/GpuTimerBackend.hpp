/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Types.hpp>

namespace Hyperion {

class DeviceBase;
class CommandBufferBase;

struct GpuFrameTimings
{
    double frameTotalMs = 0.0;
    double preRenderMs = 0.0;
    double mainRenderMs = 0.0;
    double postRenderMs = 0.0;
};

class GpuTimerBackend
{
public:
    static constexpr uint32 NumQueriesPerFrame = 4;

    enum QueryIndex : uint32
    {
        FrameStart = 0,
        AfterPreRender = 1,
        AfterMainRender = 2,
        FrameEnd = 3
    };

    virtual ~GpuTimerBackend() = default;

    virtual bool Initialize(DeviceBase* device) = 0;
    virtual void Destroy() = 0;

    virtual bool IsSupported() const = 0;
    virtual double GetTimestampPeriod() const = 0;

    virtual void RecordFrameStart(CommandBufferBase* cmd, uint32 frameIndex) = 0;
    virtual void WriteTimestamp(CommandBufferBase* cmd, uint32 frameIndex, QueryIndex queryIndex) = 0;
    virtual void RecordFrameEnd(CommandBufferBase* cmd, uint32 frameIndex) = 0;

    virtual GpuFrameTimings ResolveFrameResults(uint32 completedFrameIndex) = 0;
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <rendering/vulkan/VulkanGpuTimerBackend.hpp>
#elif HYP_DX12
#include <rendering/dx12/DX12GpuTimerBackend.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
