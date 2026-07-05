/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/Functional/Delegate.hpp>

#include <Core/Utilities/BitField.hpp>

namespace Hyperion {

class ApplicationWindow;

namespace Steam {

class SteamInputManager
{
public:
    static SteamInputManager& GetInstance();

    SteamInputManager();

    SteamInputManager(const SteamInputManager& other) = delete;
    SteamInputManager& operator=(const SteamInputManager& other) = delete;

    SteamInputManager(SteamInputManager&& other) noexcept = delete;
    SteamInputManager& operator=(SteamInputManager&& other) noexcept = delete;

    ~SteamInputManager();

    void Initialize();
    void Shutdown();

    /// Update connected controllers and states (call from main thread)
    void Update();

private:
    static constexpr uint32 MaxConnectedControllers = 8;
    static constexpr uint32 MaxAnalogActionHandles = 16;
    static constexpr uint32 MaxDigitalActionHandles = 32;
    static constexpr uint32 MaxActionSetHandles = 8;

    struct ActionSet
    {
        uint8 index;
        uint64 handle;

        uint64 analogActionHandles[MaxAnalogActionHandles];
        uint64 digitalActionHandles[MaxDigitalActionHandles];
    };

    static bool InitializeActionSet(const struct ActionSetDesc& desc, ActionSet& outSet);

    void UpdateControllers(const ActionSet& set);
    void ProcessControllerInput(const ActionSet& set);

    bool m_isInitialized;

    uint8 m_currentActionSet;
    ActionSet m_actionSets[MaxActionSetHandles];

    /// Per-window states
    struct WindowState
    {
        ApplicationWindow* window;
        uint64 m_controllers[MaxConnectedControllers];
        BitField<MaxDigitalActionHandles> digitalActionStates[MaxConnectedControllers];
    };

    static void InitializeWindowState(WindowState&, ApplicationWindow* window);
    static void ShutdownWindowState(WindowState&);

    WindowState m_windowState;
    DelegateHandler m_onMainWindowChanged;
};

} // namespace Steam
} // namespace Hyperion
