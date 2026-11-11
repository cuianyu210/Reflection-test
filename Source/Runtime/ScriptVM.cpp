#include "ScriptVM.h"
#include "UObject.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include "Utf8.h"

static std::string Unescape(const std::string& Value)
{
    std::string Result;
    Result.reserve(Value.size());
    for (size_t i = 0; i < Value.size(); ++i)
    {
        if (Value[i] == '\\' && i + 1 < Value.size())
        {
            if (Value[i + 1] == 's')
            {
                Result.push_back(' ');
                ++i;
                continue;
            }
            if (Value[i + 1] == '\\')
            {
                Result.push_back('\\');
                ++i;
                continue;
            }
        }
        Result.push_back(Value[i]);
    }
    return Result;
}

std::vector<FScriptClass>& FScriptVM::GetScripts()
{
    static std::vector<FScriptClass> Scripts;
    return Scripts;
}

bool FScriptVM::LoadFile(const char* Path)
{
    std::ifstream File(Path, std::ios::binary);
    if (!File)
    {
        std::cout << "Script load failed: " << Path << std::endl;
        return false;
    }

    FScriptClass* CurrentClass = nullptr;
    FScriptFunction* Current = nullptr;
    std::string PackageName;
    std::string Line;
    bool FirstLine = true;
    bool LoadedAny = false;
    while (std::getline(File, Line))
    {
        if (FirstLine)
        {
            FirstLine = false;
            if (Line.size() >= 3 &&
                (unsigned char)Line[0] == 0xEF &&
                (unsigned char)Line[1] == 0xBB &&
                (unsigned char)Line[2] == 0xBF)
            {
                Line.erase(0, 3);
            }
        }

        if (Line.empty() || Line[0] == '#')
        {
            continue;
        }

        std::istringstream Input(Line);
        std::string Opcode;
        Input >> Opcode;

        if (Opcode == "PACKAGE")
        {
            Input >> PackageName;
        }
        else if (Opcode == "CLASS")
        {
            FScriptClass Script;
            Input >> Script.Name;
            GetScripts().push_back(Script);
            CurrentClass = &GetScripts().back();
            LoadedAny = true;
        }
        else if (Opcode == "EXTENDS" && CurrentClass)
        {
            Input >> CurrentClass->NativeClass;
        }
        else if (Opcode == "NATIVE" && CurrentClass)
        {
            Input >> CurrentClass->NativeClass;
        }
        else if (Opcode == "ENDCLASS")
        {
            CurrentClass = nullptr;
            Current = nullptr;
        }
        else if (Opcode == "FUNC" && CurrentClass)
        {
            FScriptFunction Function;
            Input >> Function.Name;
            CurrentClass->Functions[Function.Name] = Function;
            Current = &CurrentClass->Functions[Function.Name];
        }
        else if (Opcode == "ENDFUNC")
        {
            Current = nullptr;
        }
        else if (Opcode == "LOG_STR" && Current)
        {
            std::string Text;
            Input >> Text;
            Current->Ops.push_back({ EScriptOp::LogStr, Unescape(Text), "" });
        }
        else if (Opcode == "LOG_VAR" && Current)
        {
            std::string Name;
            Input >> Name;
            Current->Ops.push_back({ EScriptOp::LogVar, Name, "" });
        }
        else if (Opcode == "SET_STR" && Current)
        {
            std::string Name;
            std::string Value;
            Input >> Name >> Value;
            Current->Ops.push_back({ EScriptOp::SetStr, Name, Unescape(Value) });
        }
        else if (Opcode == "SET_INT" && Current)
        {
            std::string Name;
            std::string Value;
            Input >> Name >> Value;
            Current->Ops.push_back({ EScriptOp::SetInt, Name, Value });
        }
        else if (Opcode == "CALL_NATIVE" && Current)
        {
            std::string Name;
            Input >> Name;
            Current->Ops.push_back({ EScriptOp::CallNative, Name, "" });
        }
        else if (Opcode == "CALL" && Current)
        {
            std::string Name;
            Input >> Name;
            Current->Ops.push_back({ EScriptOp::Call, Name, "" });
        }
        else if (Opcode == "NEW" && Current)
        {
            std::string LocalName;
            std::string ClassName;
            Input >> LocalName >> ClassName;
            Current->Ops.push_back({ EScriptOp::New, LocalName, ClassName });
        }
        else if (Opcode == "CALL_OTHER" && Current)
        {
            std::string LocalName;
            std::string FunctionName;
            Input >> LocalName >> FunctionName;
            Current->Ops.push_back({ EScriptOp::CallOther, LocalName, FunctionName });
        }
    }

    if (!LoadedAny)
    {
        return false;
    }

    std::cout << "Loaded package " << (PackageName.empty() ? Path : PackageName.c_str()) << std::endl;
    return true;
}

bool FScriptVM::LoadDirectory(const char* Directory)
{
    namespace fs = std::filesystem;
    if (!fs::exists(Directory) || !fs::is_directory(Directory))
    {
        return false;
    }

    bool Any = false;
    for (const fs::directory_entry& Entry : fs::directory_iterator(Directory))
    {
        if ( Entry.path().extension() == ".u")
        {
            Any |= LoadFile(Entry.path().string().c_str());
        }
    }
    return Any;
}

bool FScriptVM::LoadDefault()
{
    std::vector<std::string> Candidates = {
        "Script",
        "..\\Script",
        "..\\..\\Script"
    };

    char ModulePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, ModulePath, MAX_PATH) > 0)
    {
        std::filesystem::path ExeDir = std::filesystem::path(ModulePath).parent_path();
        Candidates.insert(Candidates.begin(), (ExeDir / ".." / "Script").string());
        Candidates.insert(Candidates.begin(), (ExeDir / "Script").string());
    }

    bool Any = false;
    for (const std::string& Directory : Candidates)
    {
        Any |= LoadDirectory(Directory.c_str());
    }
    return Any;
}

FScriptClass* FScriptVM::FindScriptClass(const char* Name)
{
    for (FScriptClass& Script : GetScripts())
    {
        if (Script.Name == Name || Script.NativeClass == Name)
        {
            return &Script;
        }
    }
    return nullptr;
}

FScriptFunction* FScriptVM::FindFunction(UClass* NativeClass, const char* FunctionName)
{
    if (!NativeClass)
    {
        return nullptr;
    }

    FScriptClass* Script = FindScriptClass(NativeClass->Name);
    if (Script)
    {
        auto It = Script->Functions.find(FunctionName);
        if (It != Script->Functions.end())
        {
            return &It->second;
        }
    }

    return FindFunction(NativeClass->SuperClass, FunctionName);
}

FScriptFunction* FScriptVM::FindScriptFunction(const char* ScriptClassName, const char* FunctionName)
{
    FScriptClass* Script = FindScriptClass(ScriptClassName);
    if (!Script)
    {
        return nullptr;
    }

    auto It = Script->Functions.find(FunctionName);
    if (It == Script->Functions.end())
    {
        return nullptr;
    }
    return &It->second;
}

void FScriptVM::Execute(UObject* Object, const FScriptFunction& Function)
{
    std::unordered_map<std::string, std::string> Locals;
    for (const FScriptOp& Op : Function.Ops)
    {
        switch (Op.Op)
        {
        case EScriptOp::LogStr:
            std::cout << "[script] " << Op.Arg0 << std::endl;
            break;
        case EScriptOp::LogVar:
        {
            FProperty* Property = Object->GetClass()->FindProperty(Op.Arg0.c_str());
            if (!Property)
            {
                std::cout << "[script] unknown var " << Op.Arg0 << std::endl;
                break;
            }
            if (std::strcmp(Property->Type, "int32") == 0)
            {
                std::cout << "[script] " << Op.Arg0 << " = "
                          << Object->GetPropertyInt(Op.Arg0.c_str()) << std::endl;
            }
            else
            {
                FString Value = Object->GetPropertyString(Op.Arg0.c_str());
                std::cout << "[script] " << Op.Arg0 << " = " << Value.c_str() << std::endl;
            }
            break;
        }
        case EScriptOp::SetStr:
            Object->SetPropertyString(Op.Arg0.c_str(), Op.Arg1.c_str());
            break;
        case EScriptOp::SetInt:
            Object->SetPropertyInt(Op.Arg0.c_str(), std::atoi(Op.Arg1.c_str()));
            break;
        case EScriptOp::Call:
            Object->ProcessEvent(Op.Arg0.c_str());
            break;
        case EScriptOp::CallNative:
            Object->CallNative(Op.Arg0.c_str());
            break;
        case EScriptOp::New:
            Locals[Op.Arg0] = Op.Arg1;
            std::cout << "[script] new class'" << Op.Arg1 << "'" << std::endl;
            break;
        case EScriptOp::CallOther:
        {
            auto It = Locals.find(Op.Arg0);
            if (It == Locals.end())
            {
                std::cout << "[script] None object: " << Op.Arg0 << std::endl;
                break;
            }

            FScriptFunction* Other = FindScriptFunction(It->second.c_str(), Op.Arg1.c_str());
            if (!Other)
            {
                std::cout << "[script] function not found: " << It->second << "." << Op.Arg1 << std::endl;
                break;
            }

            Execute(Object, *Other);
            break;
        }
        }
    }
}
