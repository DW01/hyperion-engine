/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Constants.hpp>
#include <Core/containers/FixedArray.hpp>

#include <vulkan/vulkan.h>

namespace Hyperion {

class VulkanDevice;
class VulkanCommandBuffer;

struct GpuFrameTimings
{
    double frameTotalMs = 0.0;
    double preRenderMs = 0.0;
    double mainRenderMs = 0.0;
    double postRenderMs = 0.0;
};

class VulkanGpuTimerBackend
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

    VulkanGpuTimerBackend();
    ~VulkanGpuTimerBackend();

    bool Initialize(VulkanDevice* device);
    void Destroy();

    /*! \brief Reset query pool and write frame-start timestamp.
     *  Must be called after commandBuffer->Begin() and before any render work. */
    void RecordFrameStart(VulkanCommandBuffer* cmd, uint32 frameIndex);

    /*! \brief Write an intermediate timestamp at the given query index. */
    void WriteTimestamp(VulkanCommandBuffer* cmd, uint32 frameIndex, QueryIndex queryIndex);

    /*! \brief Write frame-end timestamp.
     *  Must be called just before commandBuffer->End(). */
    void RecordFrameEnd(VulkanCommandBuffer* cmd, uint32 frameIndex);

    /*! \brief Read back GPU timestamps for a completed frame.
     *  Must be called after the GPU has finished work for the given frame slot
     *  (i.e. after the fence/timeline semaphore wait in PrepareFrame).
     *  \return All GPU timing values in milliseconds. Zero values indicate not available. */
    GpuFrameTimings ResolveFrameResults(uint32 completedFrameIndex);

    HYP_FORCE_INLINE bool IsSupported() const
    {
        return m_isSupported;
    }

    HYP_FORCE_INLINE double GetTimestampPeriod() const
    {
        return m_timestampPeriod;
    }

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
