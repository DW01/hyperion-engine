/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Framework/Threads/RenderWorkerThread.hpp>

#include <Core/Logging/Logger.hpp>

#include <Core/Profiling/ProfileScope.hpp>
#include <Core/Profiling/PerformanceClock.hpp>

#include <Core/Memory/Allocator/ThreadAllocator.hpp>

namespace Hyperion {

namespace threading {
extern void SetCurrentThreadIndex(uint32 threadIndex);
} // namespace threading

RenderWorkerThreadPool* g_renderWorkerThreadPool = nullptr;

RenderWorkerThread::RenderWorkerThread(const ThreadId& threadId, ThreadPriorityValue priority)
    : Thread(threadId, priority),
      m_numTasks(0),
      m_ownerPool(nullptr),
      m_threadIndex(0)
{
}

RenderWorkerThread::RenderWorkerThread(Name name, ThreadPriorityValue priority)
    : Thread(ThreadId(name, threading::THREAD_CATEGORY_TASK), priority),
      m_numTasks(0),
      m_ownerPool(nullptr),
      m_threadIndex(0)
{
}

void RenderWorkerThread::SetPriority(ThreadPriorityValue priority)
{
    /// \todo
    HYP_NOT_IMPLEMENTED();
}

void RenderWorkerThread::SetThreadIndex(uint32 threadIndex)
{
    m_threadIndex = threadIndex;

    if (IsOnThread(Id()))
    {
        SetCurrentThreadIndex(threadIndex);
    }
}

void RenderWorkerThread::operator()()
{
    InitThreadAllocator();

    SetCurrentThreadIndex(m_threadIndex);

    while (HYP_LIKELY(!m_stopRequested.LoadVolatile()))
    {
        Scheduler::ScheduledTask scheduledTask;
        bool gotTask = false;

        if (m_scheduler->TryPop(scheduledTask))
        {
            gotTask = true;
        }

        if (!gotTask)
        {
            bool stopRequested = false;
            m_scheduler->WaitForTasks(&stopRequested);

            if (stopRequested)
            {
                Stop();

                break;
            }

            continue;
        }

        HYP_PROFILE_BEGIN;

        m_numTasks.Set(m_scheduler->NumEnqueued() + 1, MemoryOrder::RELEASE);

        BeforeExecuteTasks();

        {
            HYP_NAMED_SCOPE("Executing tasks");

            scheduledTask.Execute();

        }

        AfterExecuteTasks();

        m_numTasks.Set(m_scheduler->NumEnqueued(), MemoryOrder::RELEASE);
    }
}

void RenderWorkerThread::ResetThreadLinearAllocator()
{
    if (m_threadAllocator != nullptr)
    {
        m_threadAllocator->Reset();
    }
}

} // namespace Hyperion
