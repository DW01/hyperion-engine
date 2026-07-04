/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Input/SteamInput.hpp>

#include <Core/Logging/Logger.hpp>

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

SteamInputManager s_steamInputManager;

SteamInputManager::SteamInputManager()
    : m_isInitialized(false),
      m_controllers {},
      m_setHandles {},
      m_actionHandles {}
{
}

SteamInputManager::~SteamInputManager()
{
}

SteamInputManager& SteamInputManager::GetInstance()
{
    return s_steamInputManager;
}

void SteamInputManager::Initialize()
{
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

    m_isInitialized = true;
}

void SteamInputManager::Shutdown()
{
    if (!m_isInitialized)
    {
        return;
    }

    m_isInitialized = false;

    if (!SteamInput()->Shutdown())
    {
        HYP_LOG(Steam, Error, "Failed to shutdown Steam Input!");
    }
    
    SteamAPI_Shutdown();
}

void SteamInputManager::Update()
{
    UpdateControllers();
}

void SteamInputManager::UpdateControllers()
{
    // int controllerCount = SteamInput()->GetConnectedControllers(m_controllers);

    // for (int i = 0; i < controllerCount; ++i)
    // {
    //     InputHandle_t controller = m_controllers[i];

    //     SteamInput()->ActivateActionSet(controller, m_setHandles[Set_InGame]);
    // }
}

} // namespace Hyperion