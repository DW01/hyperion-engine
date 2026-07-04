/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>

namespace Hyperion {

class ENGINE_API SteamInputManager
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
    static constexpr size_t MaxConnectedControllers = 8;

    void UpdateControllers();

    bool m_isInitialized;

    uint64 m_controllers[MaxConnectedControllers];

    uint64 m_setHandles[64];
    uint64 m_actionHandles[64];
};

} // namespace Hyperion
