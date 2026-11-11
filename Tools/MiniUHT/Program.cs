using System.Text;
using System.Text.RegularExpressions;

namespace MiniUHT;

// MiniUHT：扫带头文件里的 UCLASS / UPROPERTY / UFUNCTION / GENERATED_BODY，
// 写出 Xxx.generated.h 和 Xxx.gen.cpp。对应 UE 的 UnrealHeaderTool。

internal static class Program
{
    static int Main(string[] args)
    {
        Console.OutputEncoding = new UTF8Encoding(encoderShouldEmitUTF8Identifier: false);
        Console.InputEncoding = new UTF8Encoding(encoderShouldEmitUTF8Identifier: false);

        if (args.Length < 2)
        {
            Console.Error.WriteLine("Usage: MiniUHT <sourceRoot> <generatedRoot>");
            return 1;
        }

        string sourceRoot = Path.GetFullPath(args[0]);
        string generatedRoot = Path.GetFullPath(args[1]);

        Directory.CreateDirectory(generatedRoot);

        var headers = Directory.GetFiles(sourceRoot, "*.h", SearchOption.AllDirectories)
            .Where(path => !path.EndsWith(".generated.h", StringComparison.OrdinalIgnoreCase))
            .ToList();

        int generatedCount = 0;
        foreach (string header in headers)
        {
            ParsedClass? parsed = HeaderParser.Parse(header);
            if (parsed == null)
            {
                continue;
            }

            string generatedHeader = Path.Combine(generatedRoot, parsed.HeaderStem + ".generated.h");
            string generatedCpp = Path.Combine(generatedRoot, parsed.HeaderStem + ".gen.cpp");

            Encoding utf8NoBom = new UTF8Encoding(encoderShouldEmitUTF8Identifier: false);
            bool headerChanged = WriteIfChanged(generatedHeader, CodeGen.WriteGeneratedHeader(parsed), utf8NoBom);
            bool cppChanged = WriteIfChanged(generatedCpp, CodeGen.WriteGeneratedCpp(parsed), utf8NoBom);
            string status = (headerChanged || cppChanged) ? "wrote" : "up-to-date";

            Console.WriteLine($"UHT: {parsed.ClassName} {status} -> {Path.GetFileName(generatedHeader)}, {Path.GetFileName(generatedCpp)}");
            generatedCount++;
        }

        Console.WriteLine($"UHT finished. {generatedCount} class(es) generated.");
        return 0;
    }

    static bool WriteIfChanged(string path, string content, Encoding encoding)
    {
        if (File.Exists(path) && File.ReadAllText(path, encoding) == content)
        {
            return false;
        }

        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path, content, encoding);
        return true;
    }
}

internal sealed class ParsedClass
{
    public required string HeaderPath { get; init; }
    public required string HeaderStem { get; init; }
    public required string ClassName { get; init; }
    public required string SuperClass { get; init; }
    public List<ParsedProperty> Properties { get; } = new();
    public List<ParsedFunction> Functions { get; } = new();
}

internal sealed class ParsedProperty
{
    public required string Type { get; init; }
    public required string Name { get; init; }
    public string Specifiers { get; init; } = "";
}

internal sealed class ParsedFunction
{
    public required string Name { get; init; }
    public string Specifiers { get; init; } = "";
}

internal static class HeaderParser
{
    // 只认最简写法：
    //   UCLASS()
    //   class UHelloWorld : public UObject
    //   {
    //       GENERATED_BODY()
    //       UPROPERTY()
    //       FString Message;
    //       UFUNCTION()
    //       void PrintHello();
    //   };
    static readonly Regex ClassRegex = new(
        @"UCLASS\s*\(([^)]*)\)\s*class\s+(\w+)\s*:\s*public\s+(\w+)",
        RegexOptions.Compiled);

    static readonly Regex PropertyRegex = new(
        @"UPROPERTY\s*\(([^)]*)\)\s*(int32|FString|UObject\*)\s+(\w+)\s*(?:=[^;]+)?;",
        RegexOptions.Compiled);

    static readonly Regex FunctionRegex = new(
        @"UFUNCTION\s*\(([^)]*)\)\s*void\s+(\w+)\s*\(\s*\)\s*;",
        RegexOptions.Compiled);

    public static ParsedClass? Parse(string headerPath)
    {
        // 只读人写的头文件文本，不跟踪 #include，所以没有 .generated.h 也能生成反射。
        Encoding utf8 = new UTF8Encoding(encoderShouldEmitUTF8Identifier: false);
        string text = File.ReadAllText(headerPath, utf8);
        Match classMatch = ClassRegex.Match(text);
        if (!classMatch.Success)
        {
            return null;
        }

        if (!text.Contains("GENERATED_BODY()", StringComparison.Ordinal))
        {
            Console.Error.WriteLine($"UHT warning: {headerPath} has UCLASS but no GENERATED_BODY().");
            return null;
        }

        var parsed = new ParsedClass
        {
            HeaderPath = headerPath,
            HeaderStem = Path.GetFileNameWithoutExtension(headerPath),
            ClassName = classMatch.Groups[2].Value,
            SuperClass = classMatch.Groups[3].Value
        };

        foreach (Match match in PropertyRegex.Matches(text))
        {
            parsed.Properties.Add(new ParsedProperty
            {
                Specifiers = match.Groups[1].Value,
                Type = match.Groups[2].Value,
                Name = match.Groups[3].Value
            });
        }

        foreach (Match match in FunctionRegex.Matches(text))
        {
            parsed.Functions.Add(new ParsedFunction
            {
                Specifiers = match.Groups[1].Value,
                Name = match.Groups[2].Value
            });
        }

        return parsed;
    }
}

internal static class CodeGen
{
    public static string WriteGeneratedHeader(ParsedClass parsed)
    {
        // GENERATED_BODY 展开成 StaticClass / GetClass 声明。
        // 必须在 #include "Xxx.generated.h" 之后、class 定义之前被重新定义。
        var sb = new StringBuilder();
        sb.AppendLine("// AUTO-GENERATED by MiniUHT. Do not edit.");
        sb.AppendLine("#pragma once");
        sb.AppendLine();
        sb.AppendLine("#ifdef GENERATED_BODY");
        sb.AppendLine("#undef GENERATED_BODY");
        sb.AppendLine("#endif");
        sb.AppendLine();
        sb.AppendLine($"#define GENERATED_BODY() \\");
        sb.AppendLine("public: \\");
        sb.AppendLine($"    static UClass* StaticClass(); \\");
        sb.AppendLine($"    virtual UClass* GetClass() const override;");
        sb.AppendLine();
        return sb.ToString();
    }

    public static string WriteGeneratedCpp(ParsedClass parsed)
    {
        string headerInclude = Path.GetFileName(parsed.HeaderPath);
        var sb = new StringBuilder();
        sb.AppendLine("// AUTO-GENERATED by MiniUHT. Do not edit.");
        sb.AppendLine($"#include \"{headerInclude}\"");
        sb.AppendLine();
        sb.AppendLine($"UClass* {parsed.ClassName}::StaticClass()");
        sb.AppendLine("{");
        sb.AppendLine("    static UClass* Class = nullptr;");
        sb.AppendLine("    if (!Class)");
        sb.AppendLine("    {");
        sb.AppendLine($"        Class = new UClass(\"{parsed.ClassName}\", sizeof({parsed.ClassName}), {parsed.SuperClass}::StaticClass());");

        foreach (ParsedProperty property in parsed.Properties)
        {
            sb.AppendLine($"        Class->AddProperty(\"{property.Name}\", \"{property.Type}\", GetPropertyOffset(&{parsed.ClassName}::{property.Name}), {ToPropertyFlagsExpr(property.Specifiers)});");
        }

        foreach (ParsedFunction function in parsed.Functions)
        {
            sb.AppendLine($"        Class->AddFunction(\"{function.Name}\", [](UObject* Object)");
            sb.AppendLine("        {");
            sb.AppendLine($"            static_cast<{parsed.ClassName}*>(Object)->{function.Name}();");
            sb.AppendLine($"        }}, {ToFunctionFlagsExpr(function.Specifiers)});");
        }

        sb.AppendLine("        UClass::RegisterClass(Class);");
        sb.AppendLine("    }");
        sb.AppendLine("    return Class;");
        sb.AppendLine("}");
        sb.AppendLine();
        sb.AppendLine($"UClass* {parsed.ClassName}::GetClass() const");
        sb.AppendLine("{");
        sb.AppendLine("    return StaticClass();");
        sb.AppendLine("}");
        sb.AppendLine();
        return sb.ToString();
    }

    static string ToPropertyFlagsExpr(string specifiers)
    {
        var flags = new List<string>();
        foreach (string token in SplitSpecifiers(specifiers))
        {
            switch (token)
            {
                case "BlueprintVisible": flags.Add("CPF_BlueprintVisible"); break;
                case "BlueprintReadOnly": flags.Add("CPF_BlueprintReadOnly"); break;
                case "BlueprintReadWrite": flags.Add("CPF_BlueprintReadWrite"); break;
            }
        }
        return flags.Count == 0 ? "CPF_None" : string.Join(" | ", flags);
    }

    static string ToFunctionFlagsExpr(string specifiers)
    {
        var flags = new List<string>();
        foreach (string token in SplitSpecifiers(specifiers))
        {
            switch (token)
            {
                case "BlueprintCallable": flags.Add("FUNC_BlueprintCallable"); break;
                case "BlueprintImplementableEvent": flags.Add("FUNC_BlueprintImplementableEvent"); break;
                case "BlueprintNativeEvent": flags.Add("FUNC_BlueprintNativeEvent"); break;
            }
        }
        return flags.Count == 0 ? "FUNC_None" : string.Join(" | ", flags);
    }

    static IEnumerable<string> SplitSpecifiers(string specifiers)
    {
        foreach (string part in specifiers.Split(','))
        {
            string token = part.Trim();
            int paren = token.IndexOf('(');
            if (paren >= 0)
            {
                token = token[..paren].Trim();
            }
            if (token.Length > 0)
            {
                yield return token;
            }
        }
    }
}
