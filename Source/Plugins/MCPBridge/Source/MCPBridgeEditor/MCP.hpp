/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Types.hpp>

#include <Core/Logging/Logger.hpp>

#include <Core/DataProcessing/JSON/JSON.hpp>

namespace Hyperion {
namespace MCP {

HYP_DECLARE_LOG_CHANNEL(MCP);

static constexpr uint32 MCPMaxLogLines = 1024;
static constexpr const char* MCPImageContentKey = "__mcpImage";

} // namespace MCP
} // namespace Hyperion
