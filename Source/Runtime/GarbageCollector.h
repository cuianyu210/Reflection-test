#pragma once

#include <cstdint>
#include <vector>

typedef int32_t int32;

class UObject;

// 简易 UE4 GC：
// NewObject 进 GUObjectArray。
// AddToRoot 的对象是根。
// UPROPERTY() 的 UObject* 是可达边。
// 没有 UPROPERTY 的裸指针不算引用，会被收掉。
class FGarbageCollector
{
public:
    static void RegisterObject(UObject* Object);
    static void UnregisterObject(UObject* Object);
    static void CollectGarbage();
    static int32 NumObjects();
    static const std::vector<UObject*>& GetObjects();

private:
    static std::vector<UObject*>& GetArray();
};
