/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <scene/System.hpp>
#include <scene/components/ScriptComponent.hpp>

#include <engine/GameState.hpp>

#include <Core/functional/Delegate.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings, Serialize=false)
class ScriptSystem final : public SystemBase
{
    HYP_OBJECT_BODY(ScriptSystem);

public:
    ScriptSystem();
    ~ScriptSystem() override = default;

    // This system does not support parallel execution because scripts may modify
    // any component in the entity manager
    bool AllowParallelExecution() const override
    {
        return false;
    }

    bool RequiresSimThread() const override
    {
        return true;
    }

    void OnAddedToWorld(World* world) override;
    void OnRemovedFromWorld(World* world) override;

    void OnEntityAdded(Entity* entity) override;
    void OnEntityRemoved(Entity* entity) override;

    void Process(float delta, Span<Handle<Scene>> scenes) override;

private:
    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<ScriptComponent, ComponentAccess::READ_WRITE> {}
        };
    }

    void HandleGameStateChanged(GameStateMode gameStateMode, GameStateMode previousGameStateMode);

    void CallScriptMethod(UTF8StringView methodName);
    void CallScriptMethod(UTF8StringView methodName, ScriptComponent& target);
};

} // namespace Hyperion
