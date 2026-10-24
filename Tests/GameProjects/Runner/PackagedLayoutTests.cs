using Xunit;

namespace Hyperion.GameProjectTests;

/// The managed template built against the engine as each editor package lays it out (Tools/Scripts/PackageEditor*), rather
/// than the build tree, where every Hyperion.NET.* assembly sits next to the native library
public class PackagedLayoutTests
{
    private static readonly string[] s_managedAssemblies = { "Hyperion.NET.Shared.dll", "Hyperion.NET.Runtime.dll", "Hyperion.NET.Interop.dll" };
    private const string GeneratorsAssembly = "Hyperion.NET.Generators.dll";

    // a game built without Hyperion.NET.Generators has no type registry, so the engine finds none of its classes
    // ("Game assembly ... has no Game subclass named ...")
    [Theory]
    [InlineData("Unix")]
    [InlineData("WindowsSingleFile")]
    [InlineData("WindowsAppLocal")]
    public void ManagedTemplate_HasTypeRegistry(string layout)
    {
        string name = "Packaged" + layout;
        string packageDir = StagePackage(Path.Combine(HarnessEnvironment.WorkDir, name + "Package"), layout);

        GameProject project = GameProject.Create(name, GameProjectLanguage.Managed, engineBinDir: packageDir);

        GameAssert.Succeeded(project.Build());

        string assemblyPath = Path.Combine(project.BinariesDirectory, name + ".dll");
        IReadOnlyDictionary<string, AssemblyType> types = AssemblyTypes.Read(assemblyPath);

        Assert.True(types.ContainsKey("Hyperion.Generated.HypTypeRegistry_" + name),
            $"{assemblyPath} has no type registry: Hyperion.NET.Generators didn't run against the {layout} package layout");
        Assert.True(types.TryGetValue(project.ClassName, out AssemblyType? gameType), $"{assemblyPath} has no {project.ClassName}");
        Assert.Equal("Hyperion.Game", gameType!.BaseTypeName);
    }

    private static string StagePackage(string packageDir, string layout)
    {
        if (Directory.Exists(packageDir))
        {
            Directory.Delete(packageDir, recursive: true);
        }

        string refDir = Path.Combine(packageDir, "ref");
        Directory.CreateDirectory(packageDir);

        // the build only checks the native library exists, and nothing here runs the game
        File.WriteAllBytes(Path.Combine(packageDir, HarnessEnvironment.NativeLibraryName), Array.Empty<byte>());

        switch (layout)
        {
        // PackageEditorLinux.sh / PackageEditorMacOS.sh: the editor's publish puts the managed assemblies next to it, and
        // the generator goes only in ref/
        case "Unix":
            CopyFromEngine(packageDir, s_managedAssemblies);
            CopyFromEngine(refDir, s_managedAssemblies.Append(GeneratorsAssembly));
            break;
        // PackageEditorWindows.ps1, monolithic: the managed assemblies are inside the single-file editor
        case "WindowsSingleFile":
            CopyFromEngine(refDir, s_managedAssemblies.Append(GeneratorsAssembly));
            break;
        // PackageEditorWindows.ps1, app-local
        case "WindowsAppLocal":
            CopyFromEngine(packageDir, s_managedAssemblies.Append(GeneratorsAssembly));
            break;
        default:
            throw new ArgumentException($"Unknown package layout {layout}", nameof(layout));
        }

        return packageDir;
    }

    private static void CopyFromEngine(string destinationDir, IEnumerable<string> fileNames)
    {
        Directory.CreateDirectory(destinationDir);

        foreach (string fileName in fileNames)
        {
            File.Copy(Path.Combine(HarnessEnvironment.EngineBinDir, fileName), Path.Combine(destinationDir, fileName));
        }
    }
}
