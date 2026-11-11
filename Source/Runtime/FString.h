#pragma once

#include <string>

// 极简 FString：底层就是 std::string，只保留 UE 里最常用的几个接口。
class FString
{
public:
    FString() = default;
    FString(const char* InValue) : Data(InValue ? InValue : "") {}
    FString(const std::string& InValue) : Data(InValue) {}

    FString& operator=(const char* InValue)
    {
        Data = InValue ? InValue : "";
        return *this;
    }

    const char* c_str() const { return Data.c_str(); }
    bool IsEmpty() const { return Data.empty(); }

private:
    std::string Data;
};
