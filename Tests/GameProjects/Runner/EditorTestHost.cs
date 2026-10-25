using System.Runtime.InteropServices;
using System.Text.Json;

namespace Hyperion.GameProjectTests;

public sealed record SimulationReport(
    bool Loaded,
    string? LoadError,
    string? GameClass,
    bool Launched,
    int SimulatedFrames,
    double GameTime,
    bool Stopped);

public sealed record SimulationRunResult(ProcessResult Process, SimulationReport? Report);

public static class EditorTestHost
{
    private static readonly JsonSerializerOptions s_reportJsonOptions = new JsonSerializerOptions { PropertyNameCaseInsensitive = true };

    private static readonly Lazy<ProcessResult> s_build = new(Build);

    private static string ProjectDirectory => Path.Combine(HarnessEnvironment.RepositoryRoot, "Tests", "GameProjects", "EditorTestHost");

    public static string ExecutablePath => Path.Combine(HarnessEnvironment.EngineBinDir, "EditorTestHost" + HarnessEnvironment.ExecutableExtension);

    public static ProcessResult CreateProject(GameProject project)
    {
        string typeStr = project.Language == GameProjectLanguage.Managed
            ? "managed"
            : "native";

        return Run(
            $"Creating {project.Name} in the editor",
            project,
            "create",
            [project.ProjectFilePath, typeStr]);
    }

    public static SimulationRunResult Simulate(GameProject project, int numFrames, string label)
    {
        string resultsPath = Path.Combine(project.LogsDirectory, $"results-{label}.json");

        if (File.Exists(resultsPath))
        {
            File.Delete(resultsPath);
        }

        ProcessResult process = Run(
            $"Simulating {project.Name} in the editor ({label})",
            project,
            label,
            [project.Directory, numFrames.ToString(), resultsPath],
            command: "simulate");

        SimulationReport? report = File.Exists(resultsPath)
            ? JsonSerializer.Deserialize<SimulationReport>(File.ReadAllText(resultsPath), s_reportJsonOptions)
            : null;

        return new SimulationRunResult(process, report);
    }

    private static ProcessResult Run(string description, GameProject project, string label, string[] arguments, string? command = null)
    {
        if (!s_build.Value.Succeeded)
        {
            return s_build.Value;
        }

        Directory.CreateDirectory(project.LogsDirectory);

        return ProcessRunner.Run(
            description,
            ExecutablePath,
            new[] { command ?? label }.Concat(arguments),
            HarnessEnvironment.EngineBinDir,
            Path.Combine(project.LogsDirectory, $"editor-{label}.log"),
            HarnessEnvironment.RunTimeout,
            new Dictionary<string, string> { ["DOTNET_ROOT"] = FindDotNetRoot() });
    }

    private static ProcessResult Build()
    {
        string binDir = HarnessEnvironment.EngineBinDir.Replace('\\', '/').TrimEnd('/');
        string baseDir = HarnessEnvironment.RepositoryRoot.Replace('\\', '/').TrimEnd('/');

        return ProcessRunner.Run(
            "Building EditorTestHost",
            "dotnet",
            [
                "build", Path.Combine(ProjectDirectory, "EditorTestHost.csproj"), "--configuration", "Release", "--disable-build-servers", "--nologo",
                $"-p:HyperionBinDir={binDir}/", $"-p:HyperionBaseDir={baseDir}/"
            ],
            ProjectDirectory,
            Path.Combine(HarnessEnvironment.WorkDir, "EditorTestHost-build.log"),
            HarnessEnvironment.BuildTimeout);
    }

    private static string FindDotNetRoot()
    {
        string? dotnetRoot = Environment.GetEnvironmentVariable("DOTNET_ROOT");

        if (!string.IsNullOrEmpty(dotnetRoot))
        {
            return dotnetRoot;
        }

        // <root>/shared/Microsoft.NETCore.App/<version>/
        return Path.GetFullPath(Path.Combine(RuntimeEnvironment.GetRuntimeDirectory(), "..", "..", ".."));
    }
}
