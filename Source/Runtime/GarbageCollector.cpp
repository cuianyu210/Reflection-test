#include "GarbageCollector.h"
#include "UObject.h"

#include <algorithm>
#include <iostream>
#include <unordered_set>

std::vector<UObject*>& FGarbageCollector::GetArray()
{
    static std::vector<UObject*> Objects;
    return Objects;
}

void FGarbageCollector::RegisterObject(UObject* Object)
{
    if (Object)
    {
        GetArray().push_back(Object);
    }
}

void FGarbageCollector::UnregisterObject(UObject* Object)
{
    auto& Objects = GetArray();
    Objects.erase(std::remove(Objects.begin(), Objects.end(), Object), Objects.end());
}

int32 FGarbageCollector::NumObjects()
{
    return (int32)GetArray().size();
}

const std::vector<UObject*>& FGarbageCollector::GetObjects()
{
    return GetArray();
}

void FGarbageCollector::CollectGarbage()
{
    auto& Objects = GetArray();
    std::unordered_set<UObject*> Reachable;
    std::vector<UObject*> Stack;

    for (UObject* Object : Objects)
    {
        if (Object && Object->IsRooted())
        {
            Stack.push_back(Object);
        }
    }

    while (!Stack.empty())
    {
        UObject* Current = Stack.back();
        Stack.pop_back();
        if (!Current || !Reachable.insert(Current).second)
        {
            continue;
        }

        for (UObject* Child : Current->GetReferencedObjects())
        {
            if (Child)
            {
                Stack.push_back(Child);
            }
        }
    }

    std::vector<UObject*> Dead;
    for (UObject* Object : Objects)
    {
        if (Object && Reachable.find(Object) == Reachable.end())
        {
            Dead.push_back(Object);
        }
    }

    for (UObject* Object : Dead)
    {
        std::cout << "[gc] collect " << Object->GetClass()->Name << std::endl;
        delete Object;
    }
}
