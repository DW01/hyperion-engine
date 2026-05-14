/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <VulkanPch.hpp>

#include "VulkanGpuTimerBackend.hpp"
#include "VulkanDevice.hpp"
#include "VulkanCommandBuffer.hpp"
#include "VulkanFeatures.hpp"

namespace Hyperion {

VulkanGpuTimerBackend::VulkanGpuTimerBackend()
    : m_frames { }
{
}

VulkanGpuTimerBackend::~VulkanGpuTimerBackend()
{
    Destroy();
}

bool VulkanGpuTimerBackend::Initialize(VulkanDevice* device)
{
    m_device = device;

    if (!m_device)
    {
        return false;
    }

    const VkPhysicalDeviceLimits& limits = m_device->GetFeatures().GetPhysicalDeviceProperties().limits;
    m_timestampPeriod = double(limits.timestampPeriod);

    // Check the graphics queue family's timestampValidBits to determine if
    // timestamp queries are supported on the queue we will be using.
    const uint32 graphicsFamilyIndex = m_device->GetQueueFamilyIndices().graphicsFamily.Get();

    uint32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_device->GetPhysicalDevice(), &queueFamilyCount, nullptr);

    if (queueFamilyCount == 0 || graphicsFamilyIndex >= queueFamilyCount)
    {
        return false;
    }

    Array<VkQueueFamilyProperties> queueFamilyProperties;
    queueFamilyProperties.Resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_device->GetPhysicalDevice(), &queueFamilyCount, queueFamilyProperties.Data());

    if (queueFamilyProperties[graphicsFamilyIndex].timestampValidBits == 0)
    {
        return false;
    }

    const VkQueryPoolCreateInfo queryPoolCreateInfo {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = NumQueriesPerFrame,
        .pipelineStatistics = 0
    };

    for (uint32 i = 0; i < NumFramesInFlight; i++)
    {
        if (vkCreateQueryPool(m_device->GetDevice(), &queryPoolCreateInfo, nullptr, &m_frames[i].queryPool) != VK_SUCCESS)
        {
            Destroy();
            return false;
        }
    }

    m_isSupported = true;
    return true;
}

void VulkanGpuTimerBackend::Destroy()
{
    if (!m_device)
    {
        return;
    }

    for (uint32 i = 0; i < NumFramesInFlight; i++)
    {
        if (m_frames[i].queryPool != VK_NULL_HANDLE)
        {
            vkDestroyQueryPool(m_device->GetDevice(), m_frames[i].queryPool, nullptr);
            m_frames[i].queryPool = VK_NULL_HANDLE;
        }
        m_frames[i].resultsPending = false;
    }

    m_isSupported = false;
}

void VulkanGpuTimerBackend::RecordFrameStart(VulkanCommandBuffer* cmd, uint32 frameIndex)
{
    if (!m_isSupported || !cmd)
    {
        return;
    }

    VkCommandBuffer vkCmd = cmd->GetVulkanHandle();
    PerFrameState& frameState = m_frames[frameIndex];

    vkCmdResetQueryPool(vkCmd, frameState.queryPool, 0, NumQueriesPerFrame);
    vkCmdWriteTimestamp(vkCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, frameState.queryPool, QueryIndex::FrameStart);

    frameState.resultsPending = true;
}

void VulkanGpuTimerBackend::WriteTimestamp(VulkanCommandBuffer* cmd, uint32 frameIndex, QueryIndex queryIndex)
{
    if (!m_isSupported || !cmd)
    {
        return;
    }

    VkCommandBuffer vkCmd = cmd->GetVulkanHandle();
    PerFrameState& frameState = m_frames[frameIndex];

    vkCmdWriteTimestamp(vkCmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, frameState.queryPool, uint32(queryIndex));
}

void VulkanGpuTimerBackend::RecordFrameEnd(VulkanCommandBuffer* cmd, uint32 frameIndex)
{
    if (!m_isSupported || !cmd)
    {
        return;
    }

    VkCommandBuffer vkCmd = cmd->GetVulkanHandle();
    PerFrameState& frameState = m_frames[frameIndex];

    vkCmdWriteTimestamp(vkCmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, frameState.queryPool, QueryIndex::FrameEnd);
}

double VulkanGpuTimerBackend::ComputeDeltaMs(uint64 start, uint64 end) const
{
    if (end <= start)
    {
        return 0.0;
    }

    return double(end - start) * m_timestampPeriod * 1e-6;
}

GpuFrameTimings VulkanGpuTimerBackend::ResolveFrameResults(uint32 completedFrameIndex)
{
    GpuFrameTimings timings { };

    if (!m_isSupported)
    {
        return timings;
    }

    PerFrameState& frameState = m_frames[completedFrameIndex];

    if (!frameState.resultsPending)
    {
        return timings;
    }

    uint64 timestamps[NumQueriesPerFrame] = { };

    const VkResult result = vkGetQueryPoolResults(
        m_device->GetDevice(),
        frameState.queryPool,
        0,
        NumQueriesPerFrame,
        sizeof(timestamps),
        timestamps,
        sizeof(uint64),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

    frameState.resultsPending = false;

    if (result != VK_SUCCESS)
    {
        return timings;
    }

    timings.preRenderMs = ComputeDeltaMs(timestamps[FrameStart], timestamps[AfterPreRender]);
    timings.mainRenderMs = ComputeDeltaMs(timestamps[AfterPreRender], timestamps[AfterMainRender]);
    timings.postRenderMs = ComputeDeltaMs(timestamps[AfterMainRender], timestamps[FrameEnd]);
    timings.frameTotalMs = ComputeDeltaMs(timestamps[FrameStart], timestamps[FrameEnd]);

    return timings;
}

} // namespace Hyperion
