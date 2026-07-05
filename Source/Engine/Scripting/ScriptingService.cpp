/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Scripting/ScriptingService.hpp>

#include <Core/Core.hpp>

#include <Core/Memory/Allocator/ThreadAllocator.hpp>

#include <Framework/EngineGlobals.hpp>

namespace Hyperion {


bool ScriptingService::IsEnabled() const
{
    return EngineGlobals::IsEditor();
}

void ScriptingService::Update()
{
    if (!HasEvents() || !IsEnabled())
    {
        return;
    }

    HYP_NAMED_SCOPE("ScriptingService: Update");

    Array<ScriptEvent, ThreadAllocator> scriptEventQueue;

    { // pull events from queue
        HYP_NAMED_SCOPE("ScriptingService: Pull events from queue");

        Mutex::Guard guard(m_scriptEventQueueMutex);

        scriptEventQueue.Resize(m_scriptEventQueue.Size());
        
        // Move the elems into the local array
        std::move(m_scriptEventQueue.Begin(), m_scriptEventQueue.End(), scriptEventQueue.Begin());

        m_scriptEventQueue.Resize(0);

        m_scriptEventQueueCount.Decrement(scriptEventQueue.Size(), MemoryOrder::RELEASE);
    }

    if (scriptEventQueue.Empty())
    {
        return;
    }

    {
        HYP_NAMED_SCOPE("ScriptingService: Process events");

        for (ScriptEvent& event : scriptEventQueue)
        {
            switch (event.type)
            {
            case ScriptEventType::STATE_CHANGED:
                OnScriptStateChanged(*event.script);

                break;
            default:
                HYP_LOG(Engine, Error, "Unknown script event received: {}", uint32(event.type));

                break;
            }
        }
    }
}

void ScriptingService::PushScriptEvent(const ScriptEvent& event)
{
    if (!IsEnabled())
    {
        return;
    }

    Mutex::Guard guard(m_scriptEventQueueMutex);

    m_scriptEventQueue.PushBack(event);

    m_scriptEventQueueCount.Increment(1, MemoryOrder::RELEASE);
}

bool ScriptingService::HasEvents() const
{
    if (!IsEnabled())
    {
        return false;
    }

    return m_scriptEventQueueCount.Get(MemoryOrder::ACQUIRE);
}

} // namespace Hyperion
