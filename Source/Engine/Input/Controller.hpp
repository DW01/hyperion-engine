/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Types.hpp>

#include <Core/Defines.hpp>

namespace Hyperion {

/// Opaque handle to a Gamepad/Controller
struct ControllerT;
using ControllerHandle = ControllerT*;

static constexpr ControllerHandle InvalidControllerHandle = {};

static constexpr uint8 MaxAttachedControllers = 8;

bool IsValidController(ControllerHandle);
bool IsSteamInputController(ControllerHandle);
uint8 GetControllerIndex(ControllerHandle);

#include <Input/Controller.inl>

} // namespace Hyperion