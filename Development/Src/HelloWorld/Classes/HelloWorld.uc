// UnrealScript (UE3).
// native function  : 脚本调 C++
// event            : C++ 经 ProcessEvent 回调脚本
// function         : 本对象上的脚本函数
// new class'X'     : 跨 .uc 创建另一个 class 的实例再点调

class HelloWorld extends UHelloWorld;

native function PrintHello();
native function NativePing();

function RunDemo()
{
    log("==== 脚本 -> C++ (native) ====");
    Message = "你好，来自虚幻脚本";
    RepeatCount = 2;
    log("RepeatCount 是 BlueprintReadOnly，脚本写入应被拒绝");
    PrintHello();
    NativePing();
}

event BeginPlay()
{
    log("==== C++ -> 脚本 (event BeginPlay) ====");
    log(Message);
    CallOtherScript();
}

function CallOtherScript()
{
    local Greeter Other;
    log("==== 脚本 -> 脚本（跨文件 new class'Greeter'）====");
    Other = new class'Greeter';
    Other.SayHello();
}

function GreetFromScript()
{
    log("==== 脚本 -> 脚本（同一 class）====");
    log(Message);
}
