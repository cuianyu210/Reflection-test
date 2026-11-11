#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class UObject;

typedef int32_t int32;
typedef uint8_t uint8;

// 成员偏移：用成员指针算出字段在对象里的字节位置，给反射读写用。
template <typename ClassType, typename MemberType>
int32 GetPropertyOffset(MemberType ClassType::* Member)
{
    return (int32)(reinterpret_cast<uintptr_t>(&(reinterpret_cast<ClassType*>(0)->*Member)));
}

enum EPropertyFlags : uint32_t
{
    CPF_None = 0,
    CPF_BlueprintVisible = 1 << 0,
    CPF_BlueprintReadOnly = 1 << 1,
    CPF_BlueprintReadWrite = 1 << 2,
};

enum EFunctionFlags : uint32_t
{
    FUNC_None = 0,
    FUNC_BlueprintCallable = 1 << 0,
    FUNC_BlueprintImplementableEvent = 1 << 1,
    FUNC_BlueprintNativeEvent = 1 << 2,
};

struct FProperty
{
    const char* Name = "";
    const char* Type = ""; // "int32" / "FString" / "UObject*"
    int32 Offset = 0;
    uint32_t Flags = CPF_None;
};

struct FFunction
{
    const char* Name = "";
    void (*Invoke)(UObject* Object) = nullptr;
    uint32_t Flags = FUNC_None;
};

class UClass
{
public:
    const char* Name = "";
    int32 ClassSize = 0;
    UClass* SuperClass = nullptr;
    std::vector<FProperty> Properties;
    std::vector<FFunction> Functions;

    UClass(const char* InName, int32 InSize, UClass* InSuper);

    void AddProperty(const char* InName, const char* InType, int32 InOffset, uint32_t InFlags = CPF_None);
    void AddFunction(const char* InName, void (*InInvoke)(UObject*), uint32_t InFlags = FUNC_None);

    FProperty* FindProperty(const char* InName);
    FFunction* FindFunction(const char* InName);

    void Dump() const;

    static void RegisterClass(UClass* Class);
    static UClass* FindClass(const char* InName);

private:
    static std::unordered_map<std::string, UClass*>& GetRegistry();
};
