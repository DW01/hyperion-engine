using Xunit;

namespace Hyperion.GameProjectTests;

public class SimulationTests
{
    private const int NumFrames = 30;

    [Theory]
    [InlineData(GameProjectLanguage.Managed)]
    [InlineData(GameProjectLanguage.Native)]
    public void NewProject_SimulatesWithItsGame(GameProjectLanguage language)
    {
        GameProject project = GameProject.CreateInEditor("Simulate" + language, language);

        GameAssert.Succeeded(project.Build());

        AssertSimulated(project, EditorTestHost.Simulate(project, NumFrames, "simulate"));
    }

    [Fact]
    public void ManagedProject_WithStaleBuild_OpensAndRebuilds()
    {
        GameProject project = GameProject.CreateInEditor("StaleManaged", GameProjectLanguage.Managed);

        string propsPath = Path.Combine(project.SourceDirectory, "Hyperion.Local.props");
        string missingInstall = Path.Combine(HarnessEnvironment.WorkDir, "RemovedInstall").Replace('\\', '/');

        string notAGenerator = Path.Combine(HarnessEnvironment.EngineBinDir, "Hyperion.NET.Interop.dll");

        GameAssert.Succeeded(project.Build($"-p:HyperionGeneratorsPath={notAGenerator}"));

        Assert.DoesNotContain("Hyperion.Generated.HypTypeRegistry_" + project.Name, AssemblyTypes.Read(Path.Combine(project.BinariesDirectory, project.Name + ".dll")).Keys);

        File.WriteAllText(propsPath,
            "<Project>\n  <PropertyGroup>\n"
            + $"    <HyperionBinDir>{missingInstall}/</HyperionBinDir>\n"
            + $"    <HyperionBaseDir>{missingInstall}/</HyperionBaseDir>\n"
            + $"    <HyperionSdkDir>{missingInstall}/Source/Engine/DotNET/Sdk/</HyperionSdkDir>\n"
            + "  </PropertyGroup>\n</Project>\n");

        SimulationRunResult staleResult = EditorTestHost.Simulate(project, 0, "stale");

        Assert.True(staleResult.Report != null, $"The editor wrote no results.\n{staleResult.Process.Summary}");
        Assert.True(staleResult.Report!.Loaded, $"The project didn't open: {staleResult.Report.LoadError}\n{staleResult.Process.Summary}");
        Assert.Equal("Game", staleResult.Report.GameClass);
        Assert.DoesNotContain(missingInstall, File.ReadAllText(propsPath));

        GameAssert.Succeeded(project.Build());

        AssertSimulated(project, EditorTestHost.Simulate(project, NumFrames, "rebuilt"));
    }

    private static void AssertSimulated(GameProject project, SimulationRunResult result)
    {
        Assert.True(result.Report != null, $"The editor wrote no results.\n{result.Process.Summary}");

        SimulationReport report = result.Report!;

        Assert.True(report.Loaded, $"The project didn't open: {report.LoadError}\n{result.Process.Summary}");
        Assert.Equal(project.ClassName, report.GameClass);
        Assert.True(report.Launched, $"{project.ClassName} never launched.\n{result.Process.Summary}");
        Assert.Contains($"{project.ClassName} launched", result.Process.Output);
        Assert.Equal(NumFrames, report.SimulatedFrames);
        Assert.True(report.GameTime > 0.0, "Game time didn't advance while simulating");
        Assert.True(report.Stopped, "The game didn't stop simulating");
        Assert.True(result.Process.Succeeded, $"The game simulated, but the editor did not exit cleanly.\n{result.Process.Summary}");
    }
}
