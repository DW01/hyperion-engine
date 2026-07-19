/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Threading/Thread.hpp>
#include <Core/Threading/ThreadPool.hpp>
#include <Core/Threading/FastScheduler.hpp>

#include <Core/Defines.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

namespace threading {
class ThreadId;
} // namespace threading

class RenderWorkerThread final : public Thread<FastScheduler>
{
public:
    explicit RenderWorkerThread(const ThreadId& threadId, ThreadPriorityValue priority = ThreadPriorityValue::NORMAL);
    explicit RenderWorkerThread(Name name, ThreadPriorityValue priority = ThreadPriorityValue::NORMAL);

    virtual ~RenderWorkerThread() override = default;

    void SetPriority(ThreadPriorityValue priority);

    HYP_FORCE_INLINE void SetOwnerPool(TaskThreadPool* pool)
    {
        m_ownerPool = pool;
    }

    HYP_FORCE_INLINE TaskThreadPool* GetOwnerPool() const
    {
        return m_ownerPool;
    }

    void SetThreadIndex(uint32 threadIndex);

    HYP_FORCE_INLINE uint32 GetThreadIndex() const
    {
        return m_threadIndex;
    }

    HYP_FORCE_INLINE bool IsFree() const
    {
        return NumTasks() == 0;
    }

    HYP_FORCE_INLINE uint32 NumTasks() const
    {
        return m_numTasks.Get(MemoryOrder::ACQUIRE);
    }

    void ResetThreadLinearAllocator();

protected:
    virtual void BeforeExecuteTasks()
    {
    }

    virtual void AfterExecuteTasks()
    {
    }

    virtual void operator()() override;

    AtomicVar<uint32> m_numTasks;

    TaskThreadPool* m_ownerPool;

private:
    uint32 m_threadIndex; // index of thread in owner pool
};

class RenderWorkerThreadPool final : public TaskThreadPool
{
public:
    RenderWorkerThreadPool(uint32 numTaskThreads, ThreadPriorityValue priority)
        : TaskThreadPool(TypeWrapper<RenderWorkerThread>(), "RenderWorker", numTaskThreads)
    {
    }

    virtual ~RenderWorkerThreadPool() override = default;
};

extern RenderWorkerThreadPool* g_renderWorkerThreadPool;

} // namespace Hyperion
