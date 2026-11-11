#pragma once

#include <string>
#include <unordered_map>
#include <vector>

class UObject;
class UClass;

// 一条脚本指令。UCC make 把 .uc 编进 .u 包。
enum class EScriptOp
{
    LogStr,
    LogVar,
    SetStr,
    SetInt,
    Call,
    CallNative,
    New,
    CallOther
};

struct FScriptOp
{
    EScriptOp Op = EScriptOp::LogStr;
    std::string Arg0;
    std::string Arg1;
};

struct FScriptFunction
{
    std::string Name;
    std::vector<FScriptOp> Ops;
};

struct FScriptClass
{
    std::string Name;
    std::string NativeClass;
    std::unordered_map<std::string, FScriptFunction> Functions;
};

class FScriptVM
{
public:
    static bool LoadFile(const char* Path);
    static bool LoadDirectory(const char* Directory);
    static bool LoadDefault();
    static FScriptClass* FindScriptClass(const char* Name);
    static FScriptFunction* FindFunction(UClass* NativeClass, const char* FunctionName);
    static FScriptFunction* FindScriptFunction(const char* ScriptClassName, const char* FunctionName);
    static void Execute(UObject* Object, const FScriptFunction& Function);

private:
    static std::vector<FScriptClass>& GetScripts();
};
