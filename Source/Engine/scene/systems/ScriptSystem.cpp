/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <scene/World.hpp>
#include <scene/Scene.hpp>
#include <scene/EntityManager.hpp>
#include <scene/systems/ScriptSystem.hpp>

#include <scene/util/EntityScripting.hpp>

#include <scripting/asset/ScriptAsset.hpp>

#include <scripting/ScriptObjectResource.hpp>

#include <Core/threading/Threads.hpp>

#include <Core/memory/resource/Resource.hpp>

#include <dotnet/ManagedClass.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/DotNETHost.hpp>

#include <scripting/ScriptingService.hpp>

#include <asset/AssetRegistry.hpp>

#include <engine/EngineStats.hpp>
#include <engine/Game.hpp>

#include <system/DirectoryInitializer.hpp>

#include <HyperionEngine.hpp>

#ifdef HYP_SCRIPT
#include <Lang/HypScript.hpp>
#endif

#include <ScriptSystem.generated.inl>

namespace Hyperion {

EngineStatTimer g_statScriptUpdate("CPU/Script/Update");

#if HYP_EDITOR
constexpr bool EnableScriptReloading = true;
#else
constexpr bool EnableScriptReloading = false;
#endif

template <class ReturnType, class... ArgTypes>
static void InvokeScriptMethodT(ReturnType* outReturnValue, ScriptObjectResource* sor, const char* methodName, ArgTypes&&... args)
{
    Assert(sor != nullptr);

    const uint32 mask = sor->GetScriptLanguageMask();

#ifdef HYP_DOTNET
    if (mask & (1u << uint32(ScriptLanguage::CSharp)))
    {
        AssertDebug(sor->GetManagedObject() != nullptr);

        if (dotnet::ManagedClass* managedClass = sor->GetManagedObject()->GetClass())
        {
            if (dotnet::ManagedMethod* managedMethod = managedClass->GetMethod(methodName))
            {
                if (!managedMethod->GetAttributes().HasAttribute("ScriptMethodStub"))
                {
                    if constexpr (!std::is_void_v<ReturnType>)
                    {
                        AssertDebug(outReturnValue != nullptr);
                        new (outReturnValue) ReturnType(sor->GetManagedObject()->InvokeMethod<ReturnType>(managedMethod, std::forward<ArgTypes>(args)...));
                    }
                    else
                    {
                        sor->GetManagedObject()->InvokeMethod<void>(managedMethod, std::forward<ArgTypes>(args)...);
                    }
                }
            }
        }
    }
#endif
#ifdef HYP_SCRIPT
    if (mask & (1u << uint32(ScriptLanguage::HypScript)))
    {
        auto* data = sor->GetScriptObjectData_HypScript();
        Assert(data != nullptr);

        HypScript& hs = HypScript::GetInstance();

        BoxedValue functionValue;

        if (hs.GetFunctionHandle(data->instance, methodName, functionValue)
            && IsFunction(functionValue))
        {
            BoxedValue returnValue = hs.CallFunction(data->instance, functionValue, std::forward<ArgTypes>(args)...);

            if constexpr (!std::is_void_v<ReturnType>)
            {
                Assert(returnValue.IsValid());
                Assert(returnValue.Is<ReturnType>());

                AssertDebug(outReturnValue != nullptr);

                new (outReturnValue) ReturnType(std::move(returnValue.Get<ReturnType>()));
            }
        }
    }
#endif

    /// \todo add native script support here
}

#pragma region ScriptTracker

class ScriptTracker
{
public:
    ScriptTracker()
    {
        // @TODO will this be an issue, if running from Editor?
        if (!DotNETHost::GetInstance().IsInitialized())
        {
            return;
        }

        RC<dotnet::Assembly> managedAssembly = DotNETHost::GetInstance().LoadAssembly("Hyperion.NET.Scripting.dll");
        Assert(managedAssembly != nullptr, "Failed to load Hyperion.NET.Scripting assembly");

        RC<dotnet::ManagedClass> managedClass = managedAssembly->FindClassByName("ScriptTracker");
        Assert(managedClass != nullptr, "Failed to load ScriptTracker class from Hyperion.NET.Scripting assembly (Guid: {})",
            managedAssembly->GetGuid());

        object = UniquePtr<dotnet::ManagedObject>(managedClass->NewObject());
        assembly = std::move(managedAssembly);
    }

    ~ScriptTracker()
    {
        Shutdown();
    }

    void Initialize(
        const Array<FilePath>& sourceDirectories,
        const FilePath& intermediateDirectory,
        const FilePath& binaryOutputDirectory,
        void* callbackPtr,
        void* callbackSelfPtr)
    {
        if (!object || !object->IsValid())
        {
            return;
        }

        object->InvokeMethodByName<void>(
            "Initialize",
            sourceDirectories,
            intermediateDirectory,
            binaryOutputDirectory,
            callbackPtr,
            callbackSelfPtr);
    }

    void InvokeUpdate()
    {
        if (!object || !object->IsValid())
        {
            return;
        }

        object->InvokeMethodByName<void>("Update");
    }

    void Shutdown()
    {
        if (!object || !object->IsValid())
        {
            return;
        }

        object->InvokeMethodByName<void>("Shutdown");

        object.Reset();
        assembly.Reset();
    }

    RC<dotnet::Assembly> assembly;
    UniquePtr<dotnet::ManagedObject> object;
};

#pragma endregion ScriptTracker

#pragma region ScriptSystem

static const FilePath& GetScriptsSourceDirectory()
{
    static DirectoryInitializer<HYP_STATIC_STRING("Data/Scripts"), /* RelativeToExecutablePath */ false> s_directory;
    return s_directory.path;
}

ScriptSystem::ScriptSystem()
{
}

ScriptSystem::~ScriptSystem() = default;

void ScriptSystem::OnAddedToWorld(World* world)
{
    SystemBase::OnAddedToWorld(world);

    Game* gameInstance = world->GetGame();
    Assert(gameInstance != nullptr);

    m_delegateHandlers.Add(
        NAME("OnGameStateChange"),
        gameInstance->OnGameStateChange.Bind([this](Game* gameInstance, GameStateMode previousGameStateMode, GameStateMode currentGameStateMode)
            {
                AssertOnThread(g_simThread);

                HandleGameStateChanged(currentGameStateMode, previousGameStateMode);
            }));

    if (EnableScriptReloading)
    {
        m_scriptingService = MakeUnique<ScriptingService>();

        m_delegateHandlers.Add(
            NAME("OnScriptStateChanged"),
            m_scriptingService->OnScriptStateChanged.Bind([this](const ScriptDesc& script)
                {
                    AssertOnThread(g_simThread);

                    if (!(script.compileStatus & uint32(ScriptCompileStatus::Compiled)))
                    {
                        return;
                    }

                    World* world = GetWorld();
                    Assert(world != nullptr);

                    if (!world)
                    {
                        return;
                    }

                    for (Scene* scene : world->GetScenes())
                    {
                        for (auto [entity, scriptComponent] : scene->GetEntityManager()->GetEntitySet<ScriptComponent>().GetScopedView(GetComponentInfos()))
                        {
                            const Handle<ScriptAsset>& scriptAsset = scriptComponent.script;
                            Assert(scriptAsset != nullptr);

                            auto resGuard = scriptAsset->GetReadScope();

                            ScriptDesc& scriptDesc = scriptAsset->GetScriptDesc();

                            if (Memory::StrCmp(script.assemblyPath.Data(), scriptDesc.assemblyPath.Data(), MathUtil::Min(ArraySize(script.assemblyPath), ArraySize(scriptDesc.assemblyPath))) == 0)
                            {
                                HYP_LOG(Script, Info, "ScriptSystem: Reloading script for entity #{}", entity->Id());

                                scriptComponent.flags |= ScriptComponentFlags::RELOADING;

                                scriptDesc.uuid = script.uuid;
                                scriptDesc.compileStatus = script.compileStatus;
                                scriptDesc.hotReloadVersion = script.hotReloadVersion;
                                scriptDesc.lastModifiedTimestamp = script.lastModifiedTimestamp;

                                resGuard.Reset();

                                EntityScripting::DeinitEntityScriptComponent(entity, scriptComponent);

                                scriptComponent.assembly.Reset();

                                EntityScripting::InitEntityScriptComponent(entity, scriptComponent);

                                scriptComponent.flags &= ~ScriptComponentFlags::RELOADING;

                                HYP_LOG(Script, Info, "ScriptSystem: Script reloaded for entity #{}", entity->Id());
                            }
                        }
                    }
                }));

        m_scriptTracker = MakeUnique<ScriptTracker>();

        Array<FilePath> scriptSourceDirectories;
        scriptSourceDirectories.PushBack(GetScriptsSourceDirectory());

        // Add project-specific scripts directory from asset registry
        if (Handle<AssetRegistry> assetRegistry = GetCurrentAssetRegistry(); assetRegistry.IsValid())
        {
            if (assetRegistry->GetRootPath().Exists())
            {
                const FilePath projectScriptsPath = assetRegistry->GetRootPath() / "Scripts";

                if (projectScriptsPath.Exists())
                {
                    scriptSourceDirectories.PushBack(projectScriptsPath);
                }
            }
        }

        m_scriptTracker->Initialize(
            scriptSourceDirectories,
            GetTempDirectory() / "ScriptProjects",
            CoreApi::GetExecutablePath(),
            (void*)[](void* selfPtr, ScriptEvent event)
            {
                static_cast<ScriptingService*>(selfPtr)->PushScriptEvent(event);
            },
            m_scriptingService.Get()
        );
    }
}

void ScriptSystem::OnRemovedFromWorld(World* world)
{
    SystemBase::OnRemovedFromWorld(world);

    m_delegateHandlers.Remove("OnGameStateChange"_sh);
    m_delegateHandlers.Remove("OnScriptStateChanged"_sh);

    if (m_scriptTracker)
    {
        m_scriptTracker->Shutdown();
        m_scriptTracker.Reset();
    }

    if (m_scriptingService)
    {
        m_scriptingService.Reset();
    }
}

void ScriptSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    ScriptComponent& scriptComponent = entity->GetEntityManager()->GetComponent<ScriptComponent>(entity);

    EntityScripting::InitEntityScriptComponent(entity, scriptComponent);
}

void ScriptSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    ScriptComponent& scriptComponent = entity->GetEntityManager()->GetComponent<ScriptComponent>(entity);

    EntityScripting::DeinitEntityScriptComponent(entity, scriptComponent);
}

void ScriptSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    if (m_scriptingService)
    {
        m_scriptingService->Update();
    }

    if (m_scriptTracker)
    {
        m_scriptTracker->InvokeUpdate();
    }

    World* world = GetWorld();

    if (!world)
    {
        return;
    }

    // Only update scripts if we're in simulation mode
    if (world->GetGameState().mode != GameStateMode::SIMULATING)
    {
        return;
    }

    ENGINE_STAT_SCOPE(&g_statScriptUpdate);

    for (Scene* scene : scenes)
    {
        if (!ShouldProcessScene(scene))
        {
            continue;
        }

        for (auto [entity, scriptComponent] : scene->GetEntityManager()->GetEntitySet<ScriptComponent>().GetScopedView(GetComponentInfos()))
        {
            if (!(scriptComponent.flags & ScriptComponentFlags::INITIALIZED))
            {
                continue;
            }

            InvokeScriptMethodT<void>(nullptr, scriptComponent.scriptObjectResource, "Update", float(delta));
        }
    }
}

void ScriptSystem::HandleGameStateChanged(GameStateMode gameStateMode, GameStateMode previousGameStateMode)
{
    if (previousGameStateMode == GameStateMode::SIMULATING)
    {
        CallScriptMethod("OnPlayStop");
    }

    if (gameStateMode == GameStateMode::SIMULATING)
    {
        CallScriptMethod("OnPlayStart");
    }
}

void ScriptSystem::CallScriptMethod(UTF8StringView methodName)
{
    World* world = GetWorld();
    AssertDebug(world != nullptr);

    if (!world)
    {
        return;
    }

    for (Scene* scene : world->GetScenes())
    {
        for (auto [entity, scriptComponent] : scene->GetEntityManager()->GetEntitySet<ScriptComponent>().GetScopedView(GetComponentInfos()))
        {
            if (!(scriptComponent.flags & ScriptComponentFlags::INITIALIZED))
            {
                continue;
            }

            InvokeScriptMethodT<void>(nullptr, scriptComponent.scriptObjectResource, *methodName);
        }
    }
}

void ScriptSystem::CallScriptMethod(UTF8StringView methodName, ScriptComponent& target)
{
    if (!(target.flags & ScriptComponentFlags::INITIALIZED))
    {
        return;
    }

    InvokeScriptMethodT<void>(nullptr, target.scriptObjectResource, *methodName);
}

#pragma endregion ScriptSystem

} // namespace Hyperion
