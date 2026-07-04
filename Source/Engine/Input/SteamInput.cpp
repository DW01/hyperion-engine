/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Input/SteamInput.hpp>
#include <Input/Controller.hpp>
#include <Input/InputManager.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Core/Logging/Logger.hpp>

#include <System/AppContext.hpp>

#include <steam/steam_api.h>

namespace Hyperion {

ENGINE_API HYP_DEFINE_LOG_CHANNEL(Steam);

enum SetHandles
{
    Set_InGame
};

enum ActionHandles
{
    Action_Move
};

static inline ControllerHandle MakeSteamInputControllerHandle(uint8 controllerIndex)
{
    uint64 value = (1u << controllerIndex) | 0x100 | 0x200;
    return reinterpret_cast<ControllerHandle>(value);
}

SteamInputManager s_steamInputManager;

SteamInputManager::SteamInputManager()
    : m_isInitialized(false),
      m_setHandles {},
      m_actionHandles {},
      m_windowState {}
{
}

SteamInputManager::~SteamInputManager()
{
    Shutdown();
}

SteamInputManager& SteamInputManager::GetInstance()
{
    return s_steamInputManager;
}

void SteamInputManager::Initialize()
{
    AssertOnThread(g_mainThread);

    if (m_isInitialized)
    {
        return;
    }

    // @TODO init SteamAPI elsewhere.
    // If SteamInput init fails this will be left initialized and not properly shutdown,
    // so that needs to be fixed. Will be fixed when we move it
    SteamErrMsg errMsg;
    ESteamAPIInitResult steamInitResult = SteamAPI_InitEx(&errMsg);

    if (steamInitResult != k_ESteamAPIInitResult_OK)
    {
        HYP_LOG(Steam, Error, "Failed to initialize Steam! Error msg was: {}", (const char*)errMsg);
        return;
    }

    if (!SteamInput()->Init(false))
    {
        HYP_LOG(Steam, Error, "Failed to initialize Steam Input!");
        return;
    }

    m_setHandles[Set_InGame] = SteamInput()->GetActionSetHandle("InGameControls");
    m_actionHandles[Action_Move] = SteamInput()->GetAnalogActionHandle("Move");

    m_onMainWindowChanged = g_appContext->OnCurrentWindowChanged.Bind(
        g_appContext,
        [this](ApplicationWindow* window)
        {
            if (window == m_windowState.window)
            {
                return;
            }

            if (m_windowState.window != nullptr)
            {
                ShutdownWindowState(m_windowState);
            }

            if (window != nullptr)
            {
                InitializeWindowState(m_windowState, window);
            }
        });

    ApplicationWindow* window = g_appContext->GetMainWindow();
    if (window != nullptr)
    {
        InitializeWindowState(m_windowState, window);
    }

    m_isInitialized = true;
}

void SteamInputManager::Shutdown()
{
    AssertOnThread(g_mainThread);

    if (!m_isInitialized)
    {
        return;
    }

    m_isInitialized = false;

    if (m_windowState.window != nullptr)
    {
        ShutdownWindowState(m_windowState);
    }

    m_onMainWindowChanged.Reset();

    if (!SteamInput()->Shutdown())
    {
        HYP_LOG(Steam, Error, "Failed to properly shutdown Steam Input!");
    }

    SteamAPI_Shutdown();
}

void SteamInputManager::Update()
{
    UpdateControllers();
}

void SteamInputManager::UpdateControllers()
{

    // @TODO Add / Remove Controllers to the input manager.

    // int controllerCount = SteamInput()->GetConnectedControllers(m_controllers);

    // for (int i = 0; i < controllerCount; ++i)
    // {
    //     InputHandle_t controller = m_controllers[i];

    //     SteamInput()->ActivateActionSet(controller, m_setHandles[Set_InGame]);
    // }
}

void SteamInputManager::InitializeWindowState(WindowState& windowState, ApplicationWindow* window)
{
    Assert(window != nullptr);

    windowState.window = window;

    InputManager* inputManager = window->GetInputManager();
    Assert(inputManager != nullptr);
}

void SteamInputManager::ShutdownWindowState(WindowState& windowState)
{
    Assert(windowState.window != nullptr);

    windowState.window = nullptr;
}

} // namespace Hyperion