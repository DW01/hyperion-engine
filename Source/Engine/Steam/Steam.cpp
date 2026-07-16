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

#if 0
    // @TODO: Use application steam app id! 480 is a placeholder for now
    if (SteamAPI_RestartAppIfNecessary(480))
    {
        std::exit(0); // we are restarting through steam.
    }
#endif

    if (!SteamAPI_IsSteamRunning())
    {
        HYP_LOG(Steam, Info, "Steam is not currently running. Steam Input and services will not be accessible.");

        return;
    }

    HYP_LOG(Steam, Info, "Initializing Steam integration...");

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

    HYP_LOG(Steam, Info, "Shutting down Steam...");

    SteamAPI_Shutdown();

    s_isSteamApiInitialized = false;
}

bool IsInitialized()
{
    return s_isSteamApiInitialized;
}

} // namespace Steam
} // namespace Hyperion