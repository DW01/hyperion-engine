/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Steam/Steam.hpp>

#include <Core/Core.hpp>

#include <Core/Logging/Logger.hpp>

#include <filesystem>

#include <steam/steam_api.h>

namespace Hyperion {
namespace Steam {

HYP_DEFINE_LOG_CHANNEL(Steam);

static bool s_isSteamApiInitialized = false;

void Initialize()
{
    if (s_isSteamApiInitialized)
    {
        return;
    }

    const FilePath& baseDir = CoreApi::GetBaseDirectory();

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

    s_isSteamApiInitialized = true;
}

void Shutdown()
{
    if (!s_isSteamApiInitialized)
    {
        return;
    }

    SteamAPI_Shutdown();

    s_isSteamApiInitialized = false;
}

bool IsInitialized()
{
    return s_isSteamApiInitialized;
}

} // namespace Steam
} // namespace Hyperion