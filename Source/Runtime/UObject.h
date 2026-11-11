#pragma once

#include "FString.h"
#include "UClass.h"
#include "GarbageCollector.h"

#include <vector>

class UObject
{
public:
    virtual ~UObject();

    static UClass* StaticClass();
    virtual UClass* GetClass() const;

    void AddToRoot();
    void RemoveFromRoot();
    bool IsRooted() const { return bRooted; }

    std::vector<UObject*> GetReferencedObjects() const;

    void SetPropertyInt(const char* PropertyName, int32 Value);
    int32 GetPropertyInt(const char* PropertyName) const;

    void SetPropertyString(const char* PropertyName, const char* Value);
    FString GetPropertyString(const char* PropertyName) const;

    void SetPropertyObject(const char* PropertyName, UObject* Value);
    UObject* GetPropertyObject(const char* PropertyName) const;

    void ProcessEvent(const char* FunctionName);
    void CallNative(const char* FunctionName);

private:
    bool bRooted = false;
};

template <typename T>
T* NewObject()
{
    T::StaticClass();
    T* Object = new T();
    FGarbageCollector::RegisterObject(Object);
    return Object;
}

inline void CollectGarbage()
{
    FGarbageCollector::CollectGarbage();
}
