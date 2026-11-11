using System.Diagnostics;
using System.Text;

namespace MiniUBT;

// MiniUBT：对应 UE 的 UnrealBuildTool。
//   generate  MiniUHT + UCC make（给 VS 当预编译器用）
//   build     generate 之后再调 cl.exe（给 Build.bat 用）

internal static class Program
{
    static int Main(string[] args)
    {
        Console.OutputEncoding = new UTF8Encoding(encoderShouldEmitUTF8Identifier: false);
        Console.InputEncoding = new UTF8Encoding(encoderShouldEmitUTF8Identifier: false);

        string command = args.Length > 0 ? args[0].Trim().ToLowerInvariant() : "build";
        return command switch
        {
            "generate" => GenerateOnly(),
            "build" => GenerateThenCompile(),
            _ => PrintUsage()
        };
    }

    static int PrintUsage()
    {
        Console.Error.WriteLine("Usage: MiniUBT [generate|build]");
        Console.Error.WriteLine("  generate  MiniUHT + UCC make (VS pre-build)");
        Console.Error.WriteLine("  build     generate then compile with MSVC");
        return 1;
    }

    static int GenerateOnly()
    {
        Console.WriteLine("==== MiniUBT generate (precompile) ====");
        int uht = RunUHT();
        if (uht != 0)
        {
            return uht;
        }
        return RunMake();
    }

    static int GenerateThenCompile()
    {
        int generate = GenerateOnly();
        if (generate != 0)
        {
            return generate;
        }

        return CompileWithMsvc();
    }

    static int RunUHT()
    {
        string repoRoot = FindRepoRoot();
        string sourceRoot = Path.Combine(repoRoot, "Source");
        string generatedRoot = Path.Combine(repoRoot, "Intermediate", "Generated");
        string uhtProject = Path.Combine(repoRoot, "Tools", "MiniUHT", "MiniUHT.csproj");

        Directory.CreateDirectory(generatedRoot);

        Console.WriteLine("==== MiniUBT: MiniUHT ====");
        int uhtBuild = Run("dotnet", $"build \"{uhtProject}\" -c Release -v q", repoRoot);
        if (uhtBuild != 0)
        {
            return uhtBuild;
        }

        string uhtDll = Path.Combine(repoRoot, "Tools", "MiniUHT", "bin", "Release", "net10.0", "MiniUHT.dll");
        return Run("dotnet", $"\"{uhtDll}\" \"{sourceRoot}\" \"{generatedRoot}\"", repoRoot);
    }

    static int RunMake()
    {
        string repoRoot = FindRepoRoot();
        string scriptRoot = Path.Combine(repoRoot, "Development", "Src");
        string scriptOut = Path.Combine(repoRoot, "Script");
        string uccProject = Path.Combine(repoRoot, "Tools", "UCC", "UCC.csproj");

        Directory.CreateDirectory(scriptOut);

        Console.WriteLine("==== MiniUBT: UCC make ====");
        int uccBuild = Run("dotnet", $"build \"{uccProject}\" -c Release -v q", repoRoot);
        if (uccBuild != 0)
        {
            return uccBuild;
        }

        string uccDll = Path.Combine(repoRoot, "Tools", "UCC", "bin", "Release", "net10.0", "UCC.dll");
        return Run("dotnet", $"\"{uccDll}\" make \"{scriptRoot}\" \"{scriptOut}\"", repoRoot);
    }

    static int CompileWithMsvc()
    {
        string repoRoot = FindRepoRoot();
        string sourceRoot = Path.Combine(repoRoot, "Source");
        string generatedRoot = Path.Combine(repoRoot, "Intermediate", "Generated");
        string objRoot = Path.Combine(repoRoot, "Intermediate", "Obj");
        string binariesRoot = Path.Combine(repoRoot, "Binaries");
        string outputExe = Path.Combine(binariesRoot, "HelloWorld.exe");
        string pdbPath = Path.Combine(objRoot, "HelloWorld.pdb");
        string ilkPath = Path.Combine(objRoot, "HelloWorld.ilk");

        Directory.CreateDirectory(binariesRoot);
        Directory.CreateDirectory(objRoot);

        Console.WriteLine("==== MiniUBT: compile with MSVC ====");
        string? vcvars = FindVcvars64();
        if (vcvars == null)
        {
            Console.Error.WriteLine("Cannot find vcvars64.bat. Install Visual Studio C++ workload.");
            return 1;
        }

        var cppFiles = Directory.GetFiles(sourceRoot, "*.cpp", SearchOption.AllDirectories)
            .Concat(Directory.GetFiles(generatedRoot, "*.cpp", SearchOption.AllDirectories))
            .ToList();

        string includeRuntime = Path.Combine(sourceRoot, "Runtime");
        string includeDemo = Path.Combine(sourceRoot, "Demo");
        string rspPath = Path.Combine(objRoot, "compile.rsp");

        var rsp = new StringBuilder();
        rsp.AppendLine("/nologo");
        rsp.AppendLine("/EHsc");
        rsp.AppendLine("/std:c++17");
        rsp.AppendLine("/W3");
        rsp.AppendLine("/utf-8");
        rsp.AppendLine("/Zi");
        rsp.AppendLine($"/I\"{includeRuntime}\"");
        rsp.AppendLine($"/I\"{includeDemo}\"");
        rsp.AppendLine($"/I\"{generatedRoot}\"");
        rsp.AppendLine($"/Fe\"{outputExe}\"");
        rsp.AppendLine($"/Fo\"{objRoot}/\"");
        rsp.AppendLine($"/Fd\"{pdbPath}\"");
        foreach (string cpp in cppFiles)
        {
            rsp.AppendLine($"\"{cpp}\"");
        }
        rsp.AppendLine("/link");
        rsp.AppendLine($"/PDB:\"{pdbPath}\"");
        rsp.AppendLine($"/ILK:\"{ilkPath}\"");
        File.WriteAllText(rspPath, rsp.ToString());

        string bat = Path.Combine(Path.GetTempPath(), "miniubt_compile.bat");
        File.WriteAllText(bat,
            $"@echo off{Environment.NewLine}" +
            $"call \"{vcvars}\"{Environment.NewLine}" +
            $"cl @\"{rspPath}\"{Environment.NewLine}");

        int compile = Run("cmd.exe", $"/c \"{bat}\"", repoRoot);
        if (compile != 0)
        {
            return compile;
        }

        KeepOnlyBinaries(binariesRoot);
        Console.WriteLine($"==== MiniUBT: done -> {outputExe} ====");
        return 0;
    }

    static void KeepOnlyBinaries(string binariesRoot)
    {
        if (!Directory.Exists(binariesRoot))
        {
            return;
        }

        foreach (string file in Directory.GetFiles(binariesRoot))
        {
            string ext = Path.GetExtension(file).ToLowerInvariant();
            if (ext == ".exe" || ext == ".dll")
            {
                continue;
            }

            File.Delete(file);
        }
    }

    static string FindRepoRoot()
    {
        string? dir = AppContext.BaseDirectory;
        while (dir != null)
        {
            if (Directory.Exists(Path.Combine(dir, "Source")) &&
                Directory.Exists(Path.Combine(dir, "Tools")))
            {
                return dir;
            }

            DirectoryInfo? parent = Directory.GetParent(dir);
            dir = parent?.FullName;
        }

        return Directory.GetCurrentDirectory();
    }

    static string? FindVcvars64()
    {
        string vswhere = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),
            "Microsoft Visual Studio", "Installer", "vswhere.exe");

        if (!File.Exists(vswhere))
        {
            return null;
        }

        var start = new ProcessStartInfo
        {
            FileName = vswhere,
            Arguments = "-latest -products * -property installationPath",
            RedirectStandardOutput = true,
            UseShellExecute = false,
            CreateNoWindow = true
        };

        using Process process = Process.Start(start)!;
        string installPath = process.StandardOutput.ReadToEnd().Trim();
        process.WaitForExit();

        if (string.IsNullOrWhiteSpace(installPath))
        {
            return null;
        }

        string vcvars = Path.Combine(installPath, "VC", "Auxiliary", "Build", "vcvars64.bat");
        return File.Exists(vcvars) ? vcvars : null;
    }

    static int Run(string fileName, string arguments, string workingDirectory)
    {
        Console.WriteLine($"> {fileName} {arguments}");
        var start = new ProcessStartInfo
        {
            FileName = fileName,
            Arguments = arguments,
            WorkingDirectory = workingDirectory,
            UseShellExecute = false
        };

        using Process process = Process.Start(start)!;
        process.WaitForExit();
        return process.ExitCode;
    }
}
