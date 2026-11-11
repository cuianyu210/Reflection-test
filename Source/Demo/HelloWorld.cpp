#include "HelloWorld.h"

#include <iostream>

void UHelloWorld::PrintHello()
{
    if (Message.IsEmpty())
    {
        Message = "你好，来自 C++";
    }
    if (RepeatCount <= 0)
    {
        RepeatCount = 1;
    }

    for (int32 Index = 0; Index < RepeatCount; ++Index)
    {
        std::cout << "[native] PrintHello: " << Message.c_str() << std::endl;
    }
}

void UHelloWorld::NativePing()
{
    std::cout << "[native] NativePing: C++ 被脚本调到了" << std::endl;
}
