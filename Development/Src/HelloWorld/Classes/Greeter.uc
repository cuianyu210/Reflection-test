// 独立 class，一个 .uc 一个 class（UE3 规则）。
// HelloWorld.uc 用 new class'Greeter' 再点调 SayHello。

class Greeter extends Object;

function SayHello()
{
    log("Greeter.SayHello: 跨文件脚本调用到了");
}
