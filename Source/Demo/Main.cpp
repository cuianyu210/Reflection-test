#include "HelloWorld.h"

#include <iostream>

int main()
{
    InitUtf8Console();
    FScriptVM::LoadDefault();

    UHelloWorld* Hello = NewObject<UHelloWorld>();
    Hello->RepeatCount = 2;

    std::cout << "==== 1. 脚本调用 C++ (native function) ====" << std::endl;
    Hello->ProcessEvent("RunDemo");

    std::cout << "==== 2. C++ 调用脚本 (event / ProcessEvent) ====" << std::endl;
    Hello->ProcessEvent("BeginPlay");

    std::cout << "==== 3. 脚本调用脚本（同文件 / 跨文件）====" << std::endl;
    Hello->ProcessEvent("GreetFromScript");
    Hello->ProcessEvent("CallOtherScript");

    std::cout << "==== 类元数据 ====" << std::endl;
    Hello->GetClass()->Dump();

    std::cout << "==== BlueprintReadOnly 拒绝脚本/反射写入 ====" << std::endl;
    Hello->SetPropertyInt("RepeatCount", 99);

    std::cout << "==== GC：UPROPERTY UObject* 保活，裸指针不保活 ====" << std::endl;
    Hello->AddToRoot();
    UHelloWorld* Kept = NewObject<UHelloWorld>();
    UHelloWorld* Dropped = NewObject<UHelloWorld>();
    Hello->Child = Kept;
    std::cout << "gc objects before = " << FGarbageCollector::NumObjects() << std::endl;
    CollectGarbage();
    std::cout << "gc objects after  = " << FGarbageCollector::NumObjects() << std::endl;
    std::cout << "Child still alive = " << (Hello->Child ? Hello->Child->GetClass()->Name : "null") << std::endl;
    (void)Dropped;

    Hello->RemoveFromRoot();
    CollectGarbage();
    std::cout << "gc objects after unroot = " << FGarbageCollector::NumObjects() << std::endl;
    return 0;
}
