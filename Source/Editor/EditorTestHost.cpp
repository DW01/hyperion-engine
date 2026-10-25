/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/EditorHelpers.hpp>
#include <Editor/EditorNativeModule.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorProjectLayout.hpp>
#include <Editor/EditorProjectSettings.hpp>
#include <Editor/EditorSubsystem.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/EngineGlobals.hpp>
#include <Framework/Game.hpp>
#include <Framework/Threads/MainThread.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Scene/World.hpp>

#include <Scripting/ScriptObjectResource.hpp>

#include <Core/Reflection/Class.hpp>

#include <Core/DataProcessing/JSON/JSON.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <Core/Utilities/Time.hpp>

#include <Core/Logging/Logger.hpp>

#ifdef HYP_DOTNET
#include <DotNET/ManagedClass.hpp>
#endif

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

namespace {

constexpr int64 LaunchTimeoutMs = 60000;

String GetGameClassName(Game* game)
{
#ifdef HYP_DOTNET
    if (ScriptObjectResource* scriptObjectResource = game->GetScriptObjectResource())
    {
        if (const SharedPtr<dotnet::ManagedClass> managedClass = scriptObjectResource->GetManagedClass())
        {
            return String(managedClass->GetName());
        }
    }
#endif

    return String(*game->InstanceClass()->GetName());
}

bool StartEngine()
{
    if (!g_engineDriver->StartThreads())
    {
        HYP_LOG(Editor, Error, "Failed to start the engine threads");

        return false;
    }

    return true;
}

bool WriteResults(const FilePath& resultsPath, JSON::Object&& results)
{
    FileByteWriter writer { resultsPath };

    if (!writer.IsOpen())
    {
        HYP_LOG(Editor, Error, "Failed to open {} for writing", resultsPath);

        return false;
    }

    writer.WriteString(JSON::Value(std::move(results)).ToString(true).ToUtf8());
    writer.Close();

    return true;
}

} // namespace

} // namespace Hyperion

using namespace Hyperion;

extern "C" {

HYP_EXPORT int EditorTestHost_CreateProject(const char* projectFilepath, int isManaged)
{
    AssertOnThread(g_mainThread);

    if (!StartEngine())
    {
        return 0;
    }

    Handle<EditorProject> project = EditorProject::CreateNew();

    EditorHelpers::PopulateNewWorld(*project->GetWorld(), project->GetGame()->GetAssetRegistry(), NAME("MainScene"));

    if (Result saveResult = project->SaveAs(FilePath(projectFilepath)); saveResult.HasError())
    {
        HYP_LOG(Editor, Error, "Failed to save project to {}: {}", projectFilepath, saveResult.GetError().GetMessage());

        return 0;
    }

    project->GetProjectSettings()->moduleName = project->GetModuleName();

    if (Result saveResult = project->Save(); saveResult.HasError())
    {
        HYP_LOG(Editor, Error, "Failed to save project {}: {}", projectFilepath, saveResult.GetError().GetMessage());

        return 0;
    }

    if (Result generateResult = GenerateGameProjectFiles(project->GetFilePath(), isManaged ? GameProjectLanguage::Managed : GameProjectLanguage::Native); generateResult.HasError())
    {
        HYP_LOG(Editor, Error, "Failed to generate the game project for {}: {}", projectFilepath, generateResult.GetError().GetMessage());

        return 0;
    }

    project->Close();

    return 1;
}

HYP_EXPORT int EditorTestHost_Simulate(const char* projectDirectory, int numFrames, const char* resultsPath)
{
    AssertOnThread(g_mainThread);

    if (!StartEngine())
    {
        return 0;
    }

    JSON::Object results;

    TResult<Handle<EditorProject>> loadResult = EditorProject::Load(FindProjectFile(FilePath(projectDirectory)));

    results.Set("loaded", JSON::Value(!loadResult.HasError()));

    if (loadResult.HasError())
    {
        results.Set("loadError", JSON::Value(String(loadResult.GetError().GetMessage())));

        return WriteResults(FilePath(resultsPath), std::move(results)) ? 1 : 0;
    }

    Handle<EditorProject> project = std::move(loadResult.GetValue());
    InitObject(project);

    Handle<Game> game = project->GetGame();

    results.Set("gameClass", JSON::Value(GetGameClassName(game.Get())));

    game->SetStartupWorldName(project->GetEditWorldName());

    // the sim thread is running, so it launches the game on its edit World now
    g_engineDriver->SetGameInstance(game.Get());

    const Time startTime = Time::Now();

    bool startedSimulating = false;
    int simulatedFrames = 0;

    // headless, the sim thread is the main thread, so each update is a frame
    while (simulatedFrames < numFrames)
    {
        g_mainThreadInstance->Update();

        if (!game->IsLaunched())
        {
            if (int64(Time::Now() - startTime) > LaunchTimeoutMs)
            {
                break;
            }

            continue;
        }

        if (!startedSimulating)
        {
            game->StartSimulating();
            startedSimulating = true;

            continue;
        }

        if (game->GetGameState().mode != GameStateMode::SIMULATING)
        {
            break;
        }

        simulatedFrames++;
    }

    results.Set("launched", JSON::Value(game->IsLaunched()));
    results.Set("simulatedFrames", JSON::Value(simulatedFrames));
    results.Set("gameTime", JSON::Value(double(game->GetGameState().gameTime)));

    game->StopSimulating();

    results.Set("stopped", JSON::Value(game->GetGameState().mode == GameStateMode::STOPPED));

    project->Close();

    return WriteResults(FilePath(resultsPath), std::move(results)) ? 1 : 0;
}

} // extern "C"
