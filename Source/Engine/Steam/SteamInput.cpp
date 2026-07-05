/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Steam/SteamInput.hpp>

#include <Input/Controller.hpp>
#include <Input/InputManager.hpp>
#include <Input/Event.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Core/Core.hpp>

#include <Core/Logging/Logger.hpp>

#include <Core/Utilities/BitField.hpp>

#include <System/AppContext.hpp>

#include <steam/steam_api.h>

namespace Hyperion {
namespace Steam {

HYP_DECLARE_LOG_CHANNEL(Steam);

bool IsInitialized();

enum ActionSet : uint8
{
    ActionSet_FPSControls
};

enum AnalogAction : uint8
{
    AnalogAction_Move,
    AnalogAction_Look,
    AnalogAction_LeftTrigger,
    AnalogAction_RightTrigger,
    AnalogAction_Max
};

enum DigitalAction : uint8
{
    DigitalAction_A,
    DigitalAction_B,
    DigitalAction_X,
    DigitalAction_Y,
    DigitalAction_DPad_Up,
    DigitalAction_DPad_Down,
    DigitalAction_DPad_Left,
    DigitalAction_DPad_Right,
    DigitalAction_Left_Bumper,
    DigitalAction_Right_Bumper,
    DigitalAction_Left_Trigger,
    DigitalAction_Right_Trigger,
    DigitalAction_Left_Stick,
    DigitalAction_Right_Stick,
    DigitalAction_Start,
    DigitalAction_Select,
    DigitalAction_Guide,
    DigitalAction_Max
};

static ControllerButton MapDigitalActionToButton(DigitalAction actionIndex)
{
    switch (actionIndex)
    {
    case DigitalAction_A:
        return ControllerButton::A;
    case DigitalAction_B:
        return ControllerButton::B;
    case DigitalAction_X:
        return ControllerButton::X;
    case DigitalAction_Y:
        return ControllerButton::Y;
    case DigitalAction_DPad_Up:
        return ControllerButton::DPad_Up;
    case DigitalAction_DPad_Down:
        return ControllerButton::DPad_Down;
    case DigitalAction_DPad_Left:
        return ControllerButton::DPad_Left;
    case DigitalAction_DPad_Right:
        return ControllerButton::DPad_Right;
    case DigitalAction_Left_Bumper:
        return ControllerButton::Left_Bumper;
    case DigitalAction_Right_Bumper:
        return ControllerButton::Right_Bumper;
    case DigitalAction_Left_Trigger:
        return ControllerButton::Left_Trigger;
    case DigitalAction_Right_Trigger:
        return ControllerButton::Right_Trigger;
    case DigitalAction_Left_Stick:
        return ControllerButton::Left_Stick;
    case DigitalAction_Right_Stick:
        return ControllerButton::Right_Stick;
    case DigitalAction_Start:
        return ControllerButton::Start;
    case DigitalAction_Select:
        return ControllerButton::Select;
    case DigitalAction_Guide:
        return ControllerButton::Guide;
    default:
        return ControllerButton::None;
    }
}

static inline ControllerHandle MakeSteamInputControllerHandle(uint8 controllerIndex)
{
    uint64 value = (1u << controllerIndex) | 0x100;
    return reinterpret_cast<ControllerHandle>(value);
}

struct ActionSetDesc
{
    const char* setName;
    const char* const* analogActions;
    const char* const* digitalActions;
};

static constexpr const char* FPSControls_AnalogActions[] = {
    "Move",
    "Look",
    "LeftTrigger",
    "RightTrigger",
    nullptr
};

static constexpr const char* FPSControls_DigitalActions[] = {
    "A",
    "B",
    "X",
    "Y",
    "DPad_Up",
    "DPad_Down",
    "DPad_Left",
    "DPad_Right",
    "Left_Bumper",
    "Right_Bumper",
    "Left_Trigger",
    "Right_Trigger",
    "Left_Stick",
    "Right_Stick",
    "Start",
    "Select",
    "Guide",
    nullptr
};

static constexpr const ActionSetDesc ActionSetDescs[] = {
    // FPSControls
    ActionSetDesc {
        "FPSControls",
        FPSControls_AnalogActions,
        FPSControls_DigitalActions
    }
};

SteamInputManager s_steamInputManager;

SteamInputManager::SteamInputManager()
    : m_isInitialized(false),
      m_currentActionSet(0),
      m_actionSets {},
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

    if (!Steam::IsInitialized())
    {
        HYP_LOG(Steam, Error, "Steam API is not initialized; must be initialized before initializing Steam Input.");
        return;
    }

    if (!SteamInput()->Init(true))
    {
        HYP_LOG(Steam, Error, "Failed to initialize Steam Input!");
        return;
    }

    for (size_t i = 0; i < std::size(ActionSetDescs); i++)
    {
        InitializeActionSet(ActionSetDescs[i], m_actionSets[i]);
    }

    m_onMainWindowChanged = AppContextBase::OnCurrentWindowChanged.Bind(
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
    else
    {
        Memory::Zero(&m_windowState, sizeof(WindowState));
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
}

bool SteamInputManager::InitializeActionSet(const ActionSetDesc& desc, ActionSet& outSet)
{
    Memory::Zero(&outSet, sizeof(ActionSet));

    outSet.setHandle = SteamInput()->GetActionSetHandle(desc.setName);

    if (outSet.setHandle == 0)
    {
        return false;
    }

    for (uint32 actionIndex = 0;; actionIndex++)
    {
        if (!desc.analogActions[actionIndex])
        {
            break;
        }

        outSet.analogActionHandles[actionIndex] = SteamInput()->GetAnalogActionHandle(desc.analogActions[actionIndex]);
    }

    for (uint32 actionIndex = 0;; actionIndex++)
    {
        if (!desc.digitalActions[actionIndex])
        {
            break;
        }

        outSet.digitalActionHandles[actionIndex] = SteamInput()->GetDigitalActionHandle(desc.digitalActions[actionIndex]);
    }

    return true;
}

void SteamInputManager::Update()
{
    AssertOnThread(g_mainThread);

    if (!m_isInitialized)
    {
        return;
    }

    if (m_actionSets[m_currentActionSet].setHandle == 0)
    {
        InitializeActionSet(ActionSetDescs[m_currentActionSet], m_actionSets[m_currentActionSet]);
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

    // @TODO Steamworks docs recommends only calling every 10hz or less.
    SteamAPI_RunCallbacks();

    SteamInput()->RunFrame();

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

                    SteamInput()->ActivateActionSet(steamHandle, m_actionSets[ActionSet_FPSControls].setHandle);

                    HYP_LOG(Steam, Info, "Steam controller connected at index {}", controllerIndex);

                    break;
                }
            }
        }
        else
        {
            SteamInput()->ActivateActionSet(steamHandle, m_actionSets[ActionSet_FPSControls].setHandle);
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

            // reset states for this controller
            m_windowState.digitalActionStates[controllerIndex] = {};

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

    const ActionSet& actionSet = m_actionSets[m_currentActionSet];

    if (!actionSet.setHandle)
    {
        HYP_LOG(Steam, Warning, "Invalid action set {}", ActionSetDescs[m_currentActionSet].setName);
        return;
    }

    PlatformEvent platformEvent = {};

    for (uint8 controllerIndex = 0; controllerIndex < MaxConnectedControllers; controllerIndex++)
    {
        const InputHandle_t steamHandle = static_cast<InputHandle_t>(m_windowState.m_controllers[controllerIndex]);

        if (steamHandle == 0)
        {
            continue;
        }

        for (uint32 actionIndex = 0; actionIndex < DigitalAction_Max; actionIndex++)
        {
            const InputDigitalActionData_t actionData = SteamInput()->GetDigitalActionData(
                steamHandle, actionSet.digitalActionHandles[actionIndex]);

            if (actionData.bActive)
            {
                if (actionData.bState != m_windowState.digitalActionStates[controllerIndex].Test(actionIndex))
                {
                    const EventType eventType = actionData.bState
                        ? EventType::CONTROLLER_BUTTON_DOWN
                        : EventType::CONTROLLER_BUTTON_UP;

                    Event event(eventType, m_windowState.window, platformEvent);
                    event.GetEventData().Set(MapDigitalActionToButton(static_cast<DigitalAction>(actionIndex)));

                    inputManager->ProcessEvent(std::move(event));

                    m_windowState.digitalActionStates[controllerIndex].Set(actionIndex, actionData.bState);
                }
            }
        }

        for (uint32 actionIndex = 0; actionIndex < AnalogAction_Max; actionIndex++)
        {
            const InputAnalogActionData_t actionData = SteamInput()->GetAnalogActionData(
                steamHandle, actionSet.analogActionHandles[actionIndex]);

            if (actionData.bActive)
            {
                ControllerAnalogData analogData = {};
                analogData.controllerIndex = controllerIndex;
                analogData.actionIndex = static_cast<uint8>(actionIndex);
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

    static_assert(std::is_trivial_v<WindowState>);
    Memory::Zero(&windowState, sizeof(WindowState));

    InputManager* inputManager = window->GetInputManager();
    Assert(inputManager != nullptr);

    windowState.window = window;
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

} // namespace Steam
} // namespace Hyperion