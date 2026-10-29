/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include "MCP.hpp"

#include <Core/Reflection/Handle.hpp>

namespace Hyperion {

class World;

namespace MCP {

Handle<World> GetActiveEditorWorld();

void ClearRegisteredInstances();

} // namespace MCP
} // namespace Hyperion
