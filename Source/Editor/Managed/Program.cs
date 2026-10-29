using Avalonia;
using System;
using System.Reflection;
using System.Runtime.InteropServices;

namespace Hyperion.Editor
{
    class Program
    {
        [STAThread]
        public static int Main(string[] args)
        {
            if (Array.IndexOf(args, "--exec") >= 0)
            {
                return RunCommandlet(args);
            }

            BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);

            return 0;
        }

        public static AppBuilder BuildAvaloniaApp()
            => AppBuilder.Configure<App>()
                .With(new MacOSPlatformOptions { ShowInDock = true })
                .With(new Win32PlatformOptions {  })
                .UsePlatformDetect()
                .LogToTrace();

        private static int RunCommandlet(string[] args)
        {
            int argc = args.Length + 1;
            IntPtr[] argPtrs = new IntPtr[argc];
            IntPtr argv = Marshal.AllocHGlobal(IntPtr.Size * argc);

            try
            {
                argPtrs[0] = Marshal.StringToCoTaskMemUTF8(Environment.ProcessPath ?? "");

                for (int i = 0; i < args.Length; i++)
                {
                    argPtrs[i + 1] = Marshal.StringToCoTaskMemUTF8(args[i]);
                }

                for (int i = 0; i < argc; i++)
                {
                    Marshal.WriteIntPtr(argv, i * IntPtr.Size, argPtrs[i]);
                }

                return NativeBindings.Hyp_Initialize(argc, argv) != 0 ? 0 : 1;
            }
            finally
            {
                foreach (IntPtr argPtr in argPtrs)
                {
                    if (argPtr != IntPtr.Zero)
                    {
                        Marshal.FreeCoTaskMem(argPtr);
                    }
                }

                Marshal.FreeHGlobal(argv);
            }
        }
    }
}
