#include "UClass.h"
#include "ScriptVM.h"
#include "UObject.h"

#include <cstring>
#include <iostream>
#include <string>

static std::string DescribePropertyFlags(uint32_t Flags)
{
    std::string Text;
    if (Flags & CPF_BlueprintVisible) Text += "BlueprintVisible ";
    if (Flags & CPF_BlueprintReadOnly) Text += "BlueprintReadOnly ";
    if (Flags & CPF_BlueprintReadWrite) Text += "BlueprintReadWrite ";
    if (Text.empty())
    {
        return "";
    }
    if (!Text.empty() && Text.back() == ' ')
    {
        Text.pop_back();
    }
    return Text;
}

static std::string DescribeFunctionFlags(uint32_t Flags)
{
    std::string Text;
    if (Flags & FUNC_BlueprintCallable) Text += "BlueprintCallable ";
    if (Flags & FUNC_BlueprintImplementableEvent) Text += "BlueprintImplementableEvent ";
    if (Flags & FUNC_BlueprintNativeEvent) Text += "BlueprintNativeEvent ";
    if (Text.empty())
    {
        return "";
    }
    if (!Text.empty() && Text.back() == ' ')
    {
        Text.pop_back();
    }
    return Text;
}

UClass::UClass(const char* InName, int32 InSize, UClass* InSuper)
    : Name(InName)
    , ClassSize(InSize)
    , SuperClass(InSuper)
{
}

void UClass::AddProperty(const char* InName, const char* InType, int32 InOffset, uint32_t InFlags)
{
    FProperty Property;
    Property.Name = InName;
    Property.Type = InType;
    Property.Offset = InOffset;
    Property.Flags = InFlags;
    Properties.push_back(Property);
}

void UClass::AddFunction(const char* InName, void (*InInvoke)(UObject*), uint32_t InFlags)
{
    FFunction Function;
    Function.Name = InName;
    Function.Invoke = InInvoke;
    Function.Flags = InFlags;
    Functions.push_back(Function);
}

FProperty* UClass::FindProperty(const char* InName)
{
    for (FProperty& Property : Properties)
    {
        if (std::strcmp(Property.Name, InName) == 0)
        {
            return &Property;
        }
    }

    if (SuperClass)
    {
        return SuperClass->FindProperty(InName);
    }

    return nullptr;
}

FFunction* UClass::FindFunction(const char* InName)
{
    for (FFunction& Function : Functions)
    {
        if (std::strcmp(Function.Name, InName) == 0)
        {
            return &Function;
        }
    }

    if (SuperClass)
    {
        return SuperClass->FindFunction(InName);
    }

    return nullptr;
}

void UClass::Dump() const
{
    std::cout << "Class " << Name
              << "  size=" << ClassSize
              << "  super=" << (SuperClass ? SuperClass->Name : "(none)")
              << std::endl;

    for (const FProperty& Property : Properties)
    {
        std::cout << "  UPROPERTY  " << Property.Type << " " << Property.Name
                  << "  offset=" << Property.Offset
                  << "  " << DescribePropertyFlags(Property.Flags) << std::endl;
    }

    for (const FFunction& Function : Functions)
    {
        std::cout << "  UFUNCTION  " << Function.Name << "()  [native]  "
                  << DescribeFunctionFlags(Function.Flags) << std::endl;
    }

    FScriptClass* Script = FScriptVM::FindScriptClass(Name);
    if (Script)
    {
        for (const auto& Pair : Script->Functions)
        {
            std::cout << "  UFUNCTION  " << Pair.first << "()  [script " << Script->Name << "]" << std::endl;
        }
    }
}

std::unordered_map<std::string, UClass*>& UClass::GetRegistry()
{
    static std::unordered_map<std::string, UClass*> Registry;
    return Registry;
}

void UClass::RegisterClass(UClass* Class)
{
    GetRegistry()[Class->Name] = Class;
}

UClass* UClass::FindClass(const char* InName)
{
    auto& Registry = GetRegistry();
    auto It = Registry.find(InName);
    if (It == Registry.end())
    {
        return nullptr;
    }
    return It->second;
}
