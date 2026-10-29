using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Threading.Tasks;

namespace Hyperion.Editor.Services
{
    internal static partial class BuildProcess
    {
        public static string EditorExecutablePath => Path.Combine(AppContext.BaseDirectory, GameBuildPlatform.Current.GetExecutableFileName("Hyperion.Editor"));

        public static ProcessStartInfo CreateCommandletStartInfo(string commandletName, string workingDirectory, IEnumerable<string> arguments)
        {
            return CreateStartInfo(EditorExecutablePath, workingDirectory, CommandletArguments(commandletName, arguments));
        }

        public static async Task<int> RunAsync(ProcessStartInfo startInfo, LogChannel logChannel, Action<string> onOutputLine)
        {
            using Process process = new Process { StartInfo = startInfo };

            void OnLine(string? line)
            {
                if (line == null)
                {
                    return;
                }

                Logger.Log(logChannel, LogLevel.Info, line);

                onOutputLine(line);
            }

            process.OutputDataReceived += (_, eventArgs) => OnLine(eventArgs.Data);
            process.ErrorDataReceived += (_, eventArgs) => OnLine(eventArgs.Data);

            process.Start();

            process.BeginOutputReadLine();
            process.BeginErrorReadLine();

            await process.WaitForExitAsync().ConfigureAwait(false);

            return process.ExitCode;
        }
    }
}
