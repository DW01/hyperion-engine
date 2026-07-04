/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Input/SteamInput.hpp>
#include <Input/Controller.hpp>
#include <Input/InputManager.hpp>
#include <Input/Event.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Core/Core.hpp>

#include <Core/Logging/Logger.hpp>

#include <Core/Utilities/BitField.hpp>

#include <System/AppContext.hpp>

#include <steam/steam_api.h>

#include <filesystem>

namespace Hyperion {

ENGINE_API HYP_DEFINE_LOG_CHANNEL(Steam);

enum SetHandles
{
    Set_InGame
};

enum AnalogActionHandles
{
    AnAct_Move,
    AnAct_Look,
    AnAct_LeftTrigger,
    AnAct_RightTrigger,
    AnAct_MAX
};

enum DigitalActionHandles
{
    DigAct_A,
    DigAct_B,
    DigAct_X,
    DigAct_Y,
    DigAct_DPad_Up,
    DigAct_DPad_Down,
    DigAct_DPad_Left,
    DigAct_DPad_Right,
    DigAct_Left_Bumper,
    DigAct_Right_Bumper,
    DigAct_Left_Trigger,
    DigAct_Right_Trigger,
    DigAct_Left_Stick,
    DigAct_Right_Stick,
    DigAct_Start,
    DigAct_Select,
    DigAct_Guide,
    DigAct_MAX
};

static ControllerButton MapDigitalActionToButton(DigitalActionHandles digAct)
{
    switch (digAct)
    {
    case DigAct_A:              return ControllerButton::A;
    case DigAct_B:              return ControllerButton::B;
    case DigAct_X:              return ControllerButton::X;
    case DigAct_Y:              return ControllerButton::Y;
    case DigAct_DPad_Up:        return ControllerButton::DPad_Up;
    case DigAct_DPad_Down:      return ControllerButton::DPad_Down;
    case DigAct_DPad_Left:      return ControllerButton::DPad_Left;
    case DigAct_DPad_Right:     return ControllerButton::DPad_Right;
    case DigAct_Left_Bumper:    return ControllerButton::Left_Bumper;
    case DigAct_Right_Bumper:   return ControllerButton::Right_Bumper;
    case DigAct_Left_Trigger:   return ControllerButton::Left_Trigger;
    case DigAct_Right_Trigger:  return ControllerButton::Right_Trigger;
    case DigAct_Left_Stick:     return ControllerButton::Left_Stick;
    case DigAct_Right_Stick:    return ControllerButton::Right_Stick;
    case DigAct_Start:          return ControllerButton::Start;
    case DigAct_Select:         return ControllerButton::Select;
    case DigAct_Guide:          return ControllerButton::Guide;
    default:                    return ControllerButton::INVALID;
    }
}

static inline ControllerHandle MakeSteamInputControllerHandle(uint8 controllerIndex)
{
    uint64 value = (1u << controllerIndex) | 0x100;
    return reinterpret_cast<ControllerHandle>(value);
}

SteamInputManager s_steamInputManager;

SteamInputManager::SteamInputManager()
    : m_isInitialized(false),
      m_setHandles {},
      m_analogActionHandles {},
      m_digitalActionHandles {},
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

    // @TODO
    // SteamAPI_RestartAppIfNecessary() call in Hyp_Initialize()

    const String baseDir = CoreApi::GetBaseDirectory(); 

    std::filesystem::path oldPath = std::filesystem::current_path();
    std::filesystem::current_path(baseDir.Data());

    SteamErrMsg errMsg;
    ESteamAPIInitResult steamInitResult = SteamAPI_InitEx(&errMsg);

    std::filesystem::current_path(oldPath);

    if (steamInitResult != k_ESteamAPIInitResult_OK)
    {
        HYP_LOG(Steam, Error, "Failed to initialize Steam! Error msg was: {}", (const char*)errMsg);
        return;
    }

    if (!SteamInput()->Init(true))
    {
        HYP_LOG(Steam, Error, "Failed to initialize Steam Input!");
        return;
    }

    m_setHandles[Set_InGame] = SteamInput()->GetActionSetHandle("InGameControls");

    m_analogActionHandles[AnAct_Move] = SteamInput()->GetAnalogActionHandle("Move");
    m_analogActionHandles[AnAct_Look] = SteamInput()->GetAnalogActionHandle("Look");
    m_analogActionHandles[AnAct_LeftTrigger] = SteamInput()->GetAnalogActionHandle("LeftTrigger");
    m_analogActionHandles[AnAct_RightTrigger] = SteamInput()->GetAnalogActionHandle("RightTrigger");

    m_digitalActionHandles[DigAct_A] = SteamInput()->GetDigitalActionHandle("A");
    m_digitalActionHandles[DigAct_B] = SteamInput()->GetDigitalActionHandle("B");
    m_digitalActionHandles[DigAct_X] = SteamInput()->GetDigitalActionHandle("X");
    m_digitalActionHandles[DigAct_Y] = SteamInput()->GetDigitalActionHandle("Y");
    m_digitalActionHandles[DigAct_DPad_Up] = SteamInput()->GetDigitalActionHandle("DPad_Up");
    m_digitalActionHandles[DigAct_DPad_Down] = SteamInput()->GetDigitalActionHandle("DPad_Down");
    m_digitalActionHandles[DigAct_DPad_Left] = SteamInput()->GetDigitalActionHandle("DPad_Left");
    m_digitalActionHandles[DigAct_DPad_Right] = SteamInput()->GetDigitalActionHandle("DPad_Right");
    m_digitalActionHandles[DigAct_Left_Bumper] = SteamInput()->GetDigitalActionHandle("Left_Bumper");
    m_digitalActionHandles[DigAct_Right_Bumper] = SteamInput()->GetDigitalActionHandle("Right_Bumper");
    m_digitalActionHandles[DigAct_Left_Trigger] = SteamInput()->GetDigitalActionHandle("Left_Trigger");
    m_digitalActionHandles[DigAct_Right_Trigger] = SteamInput()->GetDigitalActionHandle("Right_Trigger");
    m_digitalActionHandles[DigAct_Left_Stick] = SteamInput()->GetDigitalActionHandle("Left_Stick");
    m_digitalActionHandles[DigAct_Right_Stick] = SteamInput()->GetDigitalActionHandle("Right_Stick");
    m_digitalActionHandles[DigAct_Start] = SteamInput()->GetDigitalActionHandle("Start");
    m_digitalActionHandles[DigAct_Select] = SteamInput()->GetDigitalActionHandle("Select");
    m_digitalActionHandles[DigAct_Guide] = SteamInput()->GetDigitalActionHandle("Guide");

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
    AssertOnThread(g_mainThread);

    if (!m_isInitialized)
    {
        return;
    }

    UpdateControllers();
    ProcessControllerInput();
}

void SteamInputManager::UpdateControllers()
{
    if (m_windowState.window == nullptr)
    {
        return;
    }

    SteamInput()->RunFrame();

    // @TODO Steamworks docs recommends only calling every 10hz or less.
    SteamAPI_RunCallbacks();

    Handle<InputManager> inputManager = m_windowState.window->GetInputManager();
    Assert(inputManager.IsValid());

    InputHandle_t steamControllers[MaxConnectedControllers];
    const int controllerCount = SteamInput()->GetConnectedControllers(steamControllers);

    BitField<MaxConnectedControllers> mask {};

    for (int i = 0; i < controllerCount && i < static_cast<int>(MaxConnectedControllers); i++)
    {
        const InputHandle_t steamHandle = steamControllers[i];

        // Check if this steam handle is already mapped to a slot
        bool found = false;
        for (uint8 controllerIndex = 0; controllerIndex < MaxConnectedControllers; controllerIndex++)
        {
            if (m_windowState.m_controllers[controllerIndex] == steamHandle)
            {
                mask.Set(controllerIndex, true);
                found = true;

                break;
            }
        }

        if (!found)
        {
            for (uint8 controllerIndex = 0; controllerIndex < MaxConnectedControllers; controllerIndex++)
            {
                if (m_windowState.m_controllers[controllerIndex] == 0)
                {
                    m_windowState.m_controllers[controllerIndex] = steamHandle;
                    mask.Set(controllerIndex, true);

                    ControllerHandle controllerHandle = MakeSteamInputControllerHandle(controllerIndex);
                    inputManager->AddController(controllerHandle);

                    SteamInput()->ActivateActionSet(steamHandle, m_setHandles[Set_InGame]);

                    HYP_LOG(Steam, Info, "Steam controller connected at index {}", controllerIndex);

                    break;
                }
            }
        }
        else
        {
            SteamInput()->ActivateActionSet(steamHandle, m_setHandles[Set_InGame]);
        }
    }

    // disconnect removed controllers
    for (uint8 controllerIndex = 0; controllerIndex < MaxConnectedControllers; controllerIndex++)
    {
        if (m_windowState.m_controllers[controllerIndex] != 0 && !mask.Test(controllerIndex))
        {
            ControllerHandle controllerHandle = MakeSteamInputControllerHandle(controllerIndex);
            inputManager->RemoveController(controllerHandle);

            m_windowState.m_controllers[controllerIndex] = 0;

            HYP_LOG(Steam, Info, "Steam controller disconnected from index {}", controllerIndex);
        }
    }
}

void SteamInputManager::ProcessControllerInput()
{
    if (m_windowState.window == nullptr)
    {
        return;
    }

    Handle<InputManager> inputManager = m_windowState.window->GetInputManager();
    Assert(inputManager.IsValid());

    PlatformEvent platformEvent = {};

    for (uint8 controllerIndex = 0; controllerIndex < MaxConnectedControllers; controllerIndex++)
    {
        const InputHandle_t steamHandle = static_cast<InputHandle_t>(m_windowState.m_controllers[controllerIndex]);
        if (steamHandle == 0)
        {
            continue;
        }

        for (uint32 digAct = 0; digAct < DigAct_MAX; digAct++)
        {
            const InputDigitalActionData_t actionData = SteamInput()->GetDigitalActionData(
                steamHandle, m_digitalActionHandles[digAct]);

            if (actionData.bActive)
            {
                const EventType eventType = actionData.bState
                    ? EventType::CONTROLLER_BUTTON_DOWN
                    : EventType::CONTROLLER_BUTTON_UP;

                Event event(eventType, m_windowState.window, platformEvent);
                event.GetEventData().Set(MapDigitalActionToButton(static_cast<DigitalActionHandles>(digAct)));
                
                inputManager->ProcessEvent(std::move(event));
            }
        }

        for (uint32 anAct = 0; anAct < AnAct_MAX; anAct++)
        {
            const InputAnalogActionData_t actionData = SteamInput()->GetAnalogActionData(
                steamHandle, m_analogActionHandles[anAct]);

            if (actionData.bActive)
            {
                ControllerAnalogData analogData = {};
                analogData.controllerIndex = controllerIndex;
                analogData.actionIndex = static_cast<uint8>(anAct);
                analogData.value = Vec2f(actionData.x, actionData.y);

                Event event(EventType::CONTROLLER_ANALOG_MOVE, m_windowState.window, platformEvent);
                event.GetEventData().Set(analogData);
                
                inputManager->ProcessEvent(std::move(event));
            }
        }
    }
}

void SteamInputManager::InitializeWindowState(WindowState& windowState, ApplicationWindow* window)
{
    Assert(window != nullptr);

    windowState.window = window;

    for (uint8 controllerIndex = 0; controllerIndex < MaxConnectedControllers; controllerIndex++)
    {
        windowState.m_controllers[controllerIndex] = 0;
    }

    InputManager* inputManager = window->GetInputManager();
    Assert(inputManager != nullptr);
}

void SteamInputManager::ShutdownWindowState(WindowState& windowState)
{
    Assert(windowState.window != nullptr);

    InputManager* inputManager = windowState.window->GetInputManager();

    for (uint8 controllerIndex = 0; controllerIndex < MaxConnectedControllers; controllerIndex++)
    {
        if (windowState.m_controllers[controllerIndex] != 0)
        {
            if (inputManager != nullptr)
            {
                ControllerHandle controllerHandle = MakeSteamInputControllerHandle(controllerIndex);
                inputManager->RemoveController(controllerHandle);
            }

            windowState.m_controllers[controllerIndex] = 0;
        }
    }

    windowState.window = nullptr;
}

} // namespace Hyperion