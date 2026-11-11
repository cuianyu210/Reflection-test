#include "UObject.h"
#include "GarbageCollector.h"
#include "ScriptVM.h"

#include <cstring>
#include <iostream>
#include <vector>

UClass* UObject::StaticClass()
{
    static UClass* Class = nullptr;
    if (!Class)
    {
        Class = new UClass("UObject", sizeof(UObject), nullptr);
        UClass::RegisterClass(Class);
    }
    return Class;
}

UClass* UObject::GetClass() const
{
    return StaticClass();
}

UObject::~UObject()
{
    FGarbageCollector::UnregisterObject(this);
}

void UObject::AddToRoot()
{
    bRooted = true;
}

void UObject::RemoveFromRoot()
{
    bRooted = false;
}

std::vector<UObject*> UObject::GetReferencedObjects() const
{
    std::vector<UObject*> Refs;
    const UClass* Class = GetClass();
    while (Class)
    {
        for (const FProperty& Property : Class->Properties)
        {
            if (std::strcmp(Property.Type, "UObject*") == 0)
            {
                UObject* Child = *reinterpret_cast<UObject* const*>((const uint8*)this + Property.Offset);
                if (Child)
                {
                    Refs.push_back(Child);
                }
            }
        }
        Class = Class->SuperClass;
    }
    return Refs;
}

static bool CanBlueprintRead(const FProperty* Property)
{
    return (Property->Flags & (CPF_BlueprintVisible | CPF_BlueprintReadOnly | CPF_BlueprintReadWrite)) != 0;
}

static bool CanBlueprintWrite(const FProperty* Property)
{
    if (Property->Flags & CPF_BlueprintReadOnly)
    {
        return false;
    }
    return (Property->Flags & (CPF_BlueprintVisible | CPF_BlueprintReadWrite)) != 0;
}

static uint8* GetPropertyAddress(const UObject* Object, const char* PropertyName, const char* ExpectedType, bool bWrite)
{
    FProperty* Property = Object->GetClass()->FindProperty(PropertyName);
    if (!Property)
    {
        std::cout << "Property not found: " << PropertyName << std::endl;
        return nullptr;
    }

    if (std::strcmp(Property->Type, ExpectedType) != 0)
    {
        std::cout << "Property type mismatch: " << PropertyName
                  << " is " << Property->Type
                  << ", expected " << ExpectedType << std::endl;
        return nullptr;
    }

    if (bWrite && !CanBlueprintWrite(Property))
    {
        std::cout << "Property not Blueprint writable: " << PropertyName << std::endl;
        return nullptr;
    }
    if (!bWrite && !CanBlueprintRead(Property))
    {
        std::cout << "Property not Blueprint readable: " << PropertyName << std::endl;
        return nullptr;
    }

    return (uint8*)Object + Property->Offset;
}

void UObject::SetPropertyInt(const char* PropertyName, int32 Value)
{
    uint8* Address = GetPropertyAddress(this, PropertyName, "int32", true);
    if (Address)
    {
        *reinterpret_cast<int32*>(Address) = Value;
    }
}

int32 UObject::GetPropertyInt(const char* PropertyName) const
{
    uint8* Address = GetPropertyAddress(this, PropertyName, "int32", false);
    if (!Address)
    {
        return 0;
    }
    return *reinterpret_cast<int32*>(Address);
}

void UObject::SetPropertyString(const char* PropertyName, const char* Value)
{
    uint8* Address = GetPropertyAddress(this, PropertyName, "FString", true);
    if (Address)
    {
        *reinterpret_cast<FString*>(Address) = Value;
    }
}

FString UObject::GetPropertyString(const char* PropertyName) const
{
    uint8* Address = GetPropertyAddress(this, PropertyName, "FString", false);
    if (!Address)
    {
        return FString();
    }
    return *reinterpret_cast<FString*>(Address);
}

void UObject::SetPropertyObject(const char* PropertyName, UObject* Value)
{
    uint8* Address = GetPropertyAddress(this, PropertyName, "UObject*", true);
    if (Address)
    {
        *reinterpret_cast<UObject**>(Address) = Value;
    }
}

UObject* UObject::GetPropertyObject(const char* PropertyName) const
{
    uint8* Address = GetPropertyAddress(this, PropertyName, "UObject*", false);
    if (!Address)
    {
        return nullptr;
    }
    return *reinterpret_cast<UObject**>(Address);
}

void UObject::CallNative(const char* FunctionName)
{
    FFunction* Function = GetClass()->FindFunction(FunctionName);
    if (Function && Function->Invoke)
    {
        Function->Invoke(this);
        return;
    }

    std::cout << "Native function not found: " << FunctionName << std::endl;
}

void UObject::ProcessEvent(const char* FunctionName)
{
    // 脚本和 C++ 同时存在：同名函数脚本优先（UE3 event override），
    // 没有脚本实现再走 native。C++ 直接调成员函数永远走 native。
    FScriptFunction* ScriptFunction = FScriptVM::FindFunction(GetClass(), FunctionName);
    if (ScriptFunction)
    {
        FScriptVM::Execute(this, *ScriptFunction);
        return;
    }

    CallNative(FunctionName);
}
