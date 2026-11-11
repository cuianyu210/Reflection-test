using System.Text;
using System.Text.RegularExpressions;

namespace UCC;

// UCC make：对应 UE1/UE2 的 ucc.exe make、UE3/UDK 的 Game.exe make（MakeCommandlet）。
// 扫 Development/Src/<Package>/Classes/*.uc，每个 .uc 一个 class，同包编进 Script/<Package>.u。
// 跨文件调用走 UE3 的 new class'X' + Other.Func()，不是把函数塞进同一个 class。

internal static class Program
{
    static int Main(string[] args)
    {
        Console.OutputEncoding = new UTF8Encoding(encoderShouldEmitUTF8Identifier: false);
        Console.InputEncoding = new UTF8Encoding(encoderShouldEmitUTF8Identifier: false);

        List<string> rest = args.ToList();
        if (rest.Count > 0 && rest[0].Equals("make", StringComparison.OrdinalIgnoreCase))
        {
            rest.RemoveAt(0);
        }

        if (rest.Count < 2)
        {
            Console.Error.WriteLine("Usage: UCC make <srcRoot> <scriptOut>");
            Console.Error.WriteLine("  srcRoot    Development/Src");
            Console.Error.WriteLine("  scriptOut  Script   (compiled .u packages)");
            return 1;
        }

        string scriptRoot = Path.GetFullPath(rest[0]);
        string outputRoot = Path.GetFullPath(rest[1]);
        Directory.CreateDirectory(outputRoot);

        if (!Directory.Exists(scriptRoot))
        {
            Console.WriteLine($"make: no script folder yet ({scriptRoot})");
            return 0;
        }

        Encoding utf8 = new UTF8Encoding(false);
        var packages = new Dictionary<string, List<ParsedScript>>(StringComparer.OrdinalIgnoreCase);

        foreach (string path in Directory.GetFiles(scriptRoot, "*.uc", SearchOption.AllDirectories))
        {
            ParsedScript parsed = ScriptParser.Parse(path);
            string packageName = GuessPackageName(path, parsed.ClassName);
            if (!packages.TryGetValue(packageName, out List<ParsedScript>? classes))
            {
                classes = new List<ParsedScript>();
                packages[packageName] = classes;
            }
            classes.Add(parsed);
        }

        int count = 0;
        foreach (KeyValuePair<string, List<ParsedScript>> package in packages)
        {
            string outputPath = Path.Combine(outputRoot, package.Key + ".u");
            string text = BytecodeWriter.WritePackage(package.Key, package.Value);
            if (!File.Exists(outputPath) || File.ReadAllText(outputPath, utf8) != text)
            {
                File.WriteAllText(outputPath, text, utf8);
                Console.WriteLine($"make: {package.Key} wrote -> {Path.GetFileName(outputPath)} ({package.Value.Count} class(es))");
            }
            else
            {
                Console.WriteLine($"make: {package.Key} up-to-date -> {Path.GetFileName(outputPath)}");
            }
            count++;
        }

        Console.WriteLine($"make finished. {count} package(s) compiled.");
        return 0;
    }

    static string GuessPackageName(string ucPath, string className)
    {
        DirectoryInfo? classesDir = Directory.GetParent(ucPath);
        DirectoryInfo? packageDir = classesDir?.Parent;
        if (packageDir != null &&
            classesDir != null &&
            classesDir.Name.Equals("Classes", StringComparison.OrdinalIgnoreCase))
        {
            return packageDir.Name;
        }

        return className;
    }
}

internal sealed class ParsedScript
{
    public required string ClassName { get; init; }
    public required string SuperClass { get; init; }
    public List<string> NativeFunctions { get; } = new();
    public List<ParsedFunction> Functions { get; } = new();
}

internal sealed class ParsedFunction
{
    public required string Name { get; init; }
    public List<string> BodyLines { get; } = new();
}

internal static class ScriptParser
{
    static readonly Regex ClassRegex = new(@"class\s+(\w+)\s+extends\s+(\w+)\s*;", RegexOptions.IgnoreCase);
    static readonly Regex NativeFunctionRegex = new(@"native\s+function\s+(\w+)\s*\(\s*\)\s*;", RegexOptions.IgnoreCase);
    static readonly Regex FunctionRegex = new(@"^(?!native\b)(?:event|function)\s+(\w+)\s*\(\s*\)\s*\{?", RegexOptions.IgnoreCase);

    public static ParsedScript Parse(string path)
    {
        Encoding utf8 = new UTF8Encoding(encoderShouldEmitUTF8Identifier: false);
        string[] rawLines = File.ReadAllLines(path, utf8);
        var lines = new List<string>();
        foreach (string raw in rawLines)
        {
            string line = raw;
            int comment = line.IndexOf("//", StringComparison.Ordinal);
            if (comment >= 0)
            {
                line = line[..comment];
            }
            line = line.Trim();
            if (line.Length > 0)
            {
                lines.Add(line);
            }
        }

        string joined = string.Join("\n", lines);
        Match classMatch = ClassRegex.Match(joined);
        if (!classMatch.Success)
        {
            throw new InvalidOperationException($"{path}: missing 'class X extends Y;'");
        }

        var parsed = new ParsedScript
        {
            ClassName = classMatch.Groups[1].Value,
            SuperClass = classMatch.Groups[2].Value
        };

        foreach (Match native in NativeFunctionRegex.Matches(joined))
        {
            parsed.NativeFunctions.Add(native.Groups[1].Value);
        }

        for (int i = 0; i < lines.Count; i++)
        {
            Match fn = FunctionRegex.Match(lines[i]);
            if (!fn.Success)
            {
                continue;
            }

            var function = new ParsedFunction { Name = fn.Groups[1].Value };
            int depth = CountChar(lines[i], '{') - CountChar(lines[i], '}');
            if (depth <= 0)
            {
                i++;
                while (i < lines.Count && !lines[i].Contains('{'))
                {
                    i++;
                }
                if (i >= lines.Count)
                {
                    break;
                }
                depth = CountChar(lines[i], '{') - CountChar(lines[i], '}');
            }
            i++;
            while (i < lines.Count && depth > 0)
            {
                string body = lines[i];
                depth += CountChar(body, '{') - CountChar(body, '}');
                if (depth > 0)
                {
                    if (body.EndsWith(';'))
                    {
                        body = body[..^1].Trim();
                    }
                    if (body.Length > 0 && body != "}" &&
                        !body.StartsWith("local ", StringComparison.OrdinalIgnoreCase))
                    {
                        function.BodyLines.Add(body);
                    }
                }
                i++;
            }
            i--;
            parsed.Functions.Add(function);
        }

        return parsed;
    }

    static int CountChar(string text, char value)
    {
        int count = 0;
        foreach (char ch in text)
        {
            if (ch == value)
            {
                count++;
            }
        }
        return count;
    }
}

internal static class BytecodeWriter
{
    static readonly Regex LogString = new(@"^log\s*\(\s*""(.*)""\s*\)$", RegexOptions.IgnoreCase);
    static readonly Regex LogIdent = new(@"^log\s*\(\s*(\w+)\s*\)$", RegexOptions.IgnoreCase);
    static readonly Regex SetString = new(@"^(\w+)\s*=\s*""(.*)""$", RegexOptions.IgnoreCase);
    static readonly Regex SetInt = new(@"^(\w+)\s*=\s*(-?\d+)$", RegexOptions.IgnoreCase);
    static readonly Regex NewClass = new(@"^(\w+)\s*=\s*new\s+class'(\w+)'$", RegexOptions.IgnoreCase);
    static readonly Regex CallOther = new(@"^(\w+)\.(\w+)\s*\(\s*\)$", RegexOptions.IgnoreCase);
    static readonly Regex Call = new(@"^(\w+)\s*\(\s*\)$", RegexOptions.IgnoreCase);

    public static string WritePackage(string packageName, List<ParsedScript> classes)
    {
        var sb = new StringBuilder();
        sb.AppendLine("# Compiled UnrealScript package. Generated by UCC make.");
        sb.AppendLine($"PACKAGE {packageName}");

        foreach (ParsedScript parsed in classes)
        {
            sb.AppendLine($"CLASS {parsed.ClassName}");
            sb.AppendLine($"EXTENDS {parsed.SuperClass}");

            foreach (ParsedFunction function in parsed.Functions)
            {
                sb.AppendLine($"FUNC {function.Name}");
                foreach (string stmt in function.BodyLines)
                {
                    WriteStatement(sb, parsed, stmt);
                }
                sb.AppendLine("ENDFUNC");
            }

            sb.AppendLine("ENDCLASS");
        }

        return sb.ToString();
    }

    static void WriteStatement(StringBuilder sb, ParsedScript parsed, string stmt)
    {
        Match m;
        if ((m = LogString.Match(stmt)).Success)
        {
            sb.AppendLine($"  LOG_STR {Escape(m.Groups[1].Value)}");
        }
        else if ((m = LogIdent.Match(stmt)).Success)
        {
            sb.AppendLine($"  LOG_VAR {m.Groups[1].Value}");
        }
        else if ((m = SetString.Match(stmt)).Success)
        {
            sb.AppendLine($"  SET_STR {m.Groups[1].Value} {Escape(m.Groups[2].Value)}");
        }
        else if ((m = SetInt.Match(stmt)).Success)
        {
            sb.AppendLine($"  SET_INT {m.Groups[1].Value} {m.Groups[2].Value}");
        }
        else if ((m = NewClass.Match(stmt)).Success)
        {
            sb.AppendLine($"  NEW {m.Groups[1].Value} {m.Groups[2].Value}");
        }
        else if ((m = CallOther.Match(stmt)).Success)
        {
            sb.AppendLine($"  CALL_OTHER {m.Groups[1].Value} {m.Groups[2].Value}");
        }
        else if ((m = Call.Match(stmt)).Success)
        {
            string callee = m.Groups[1].Value;
            bool isNative = parsed.NativeFunctions.Any(name =>
                string.Equals(name, callee, StringComparison.OrdinalIgnoreCase));
            sb.AppendLine(isNative ? $"  CALL_NATIVE {callee}" : $"  CALL {callee}");
        }
        else
        {
            throw new InvalidOperationException($"Unsupported statement: {stmt}");
        }
    }

    static string Escape(string value)
    {
        return value.Replace("\\", "\\\\").Replace(" ", "\\s");
    }
}
