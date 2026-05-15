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
class EngineStatGpuTimer;

class GpuTimerBackend
{
public:
    virtual ~GpuTimerBackend() = default;

    virtual bool Initialize(DeviceBase* device) = 0;
    virtual void Destroy() = 0;

    virtual bool IsSupported() const = 0;
    virtual double GetTimestampPeriod() const = 0;

    virtual void WriteTimestamp(CommandBufferBase* cmd, uint32 frameIndex, EngineStatGpuTimer* timer, bool isStart) = 0;

    virtual void ResolveFrameResults(uint32 completedFrameIndex) = 0;
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
