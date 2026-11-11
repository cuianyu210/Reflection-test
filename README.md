# Mini Reflection

一套给学习用的极简虚幻反射模型：C++ 写法靠近 **UE4**（`UCLASS` / `UPROPERTY` / `UFUNCTION` + GC），脚本靠近 **UE3**（UnrealScript + `UCC make`）。

打开工程、点生成，就能看到：

- 脚本调用 C++
- C++ 调用脚本
- 脚本跨文件调用另一个脚本
- `BlueprintReadOnly` 拒绝脚本写入
- 简易 GC：`UPROPERTY` 的对象指针保活，裸指针不保活

没有真正的蓝图编辑器、没有模块系统，刻意做得很短，方便把管线看懂。

---

## 环境

| 软件 | 用途 |
| --- | --- |
| Visual Studio（勾选「使用 C++ 的桌面开发」） | 提供 `cl.exe`，也能 F5 调试 |
| .NET SDK 10 | 跑 MiniUHT / MiniUBT / UCC |

在 PowerShell 里确认：

```bat
dotnet --version
```

能打出版本号即可。

---

## 最快的玩法

### 方法一：Visual Studio（推荐）

1. 双击打开 `Reflection.slnx`
2. 把 **HelloWorld** 设为启动项目
3. 按 **F5**（或生成 → 生成 HelloWorld）

预生成事件会先跑 MiniUHT（反射）和 `UCC make`（脚本），再编 C++。exe 在 `Binaries\HelloWorld.exe`。

控制台如果中文乱码，用 Windows Terminal，或先执行 `chcp 65001`。

### 方法二：命令行

在仓库根目录：

```bat
Build.bat
Binaries\HelloWorld.exe
```

只想生成反射和脚本、不编 C++：

```bat
Generate.bat
```

---

## 跑起来你会看到什么

程序会依次演示三件事：

1. **脚本 → C++**  
   `RunDemo` 写 `Message`，再调 native 的 `PrintHello` / `NativePing`。  
   `RepeatCount` 是 `BlueprintReadOnly`，脚本写入会被拒绝。

2. **C++ → 脚本**  
   `ProcessEvent("BeginPlay")` 进 `event BeginPlay`，里面再跨文件调 `Greeter.SayHello`。

3. **GC**  
   根对象通过 `UPROPERTY UObject* Child` 保住另一个对象；没挂在 `UPROPERTY` 上的对象会被收掉。根去掉之后，剩下的也会被收掉。

正常结尾大致是：

```
[gc] collect UHelloWorld
gc objects after unroot = 0
```

---

## 目录（和引擎的对应）

```
Reflection/
├── Source/Demo/HelloWorld.h          C++ 类（人写）
├── Source/Runtime/                   UObject / UClass / GC / 脚本 VM
├── Development/Src/HelloWorld/Classes/
│   ├── HelloWorld.uc                 UnrealScript 源码
│   └── Greeter.uc                    另一个 class（跨文件调用）
├── Script/HelloWorld.u               UCC make 编出的脚本包
├── Intermediate/Generated/           UHT 生成的 .generated.h / .gen.cpp
├── Intermediate/Obj/                 obj / pdb
├── Binaries/HelloWorld.exe           最终可执行文件（只放 exe/dll）
├── Content/                          留给资源，脚本不放这里
├── Tools/MiniUHT                     对应 UnrealHeaderTool
├── Tools/MiniUBT                     对应 UnrealBuildTool
├── Tools/UCC                         对应 UCC make
├── Reflection.slnx
├── Build.bat
└── Generate.bat
```

| 路径 | 引擎里是什么 |
| --- | --- |
| `Tools/MiniUHT` | UnrealHeaderTool（UE4/5） |
| `Tools/MiniUBT` | UnrealBuildTool（UE4/5） |
| `Tools/UCC` + `UCC make` | UE3 的 MakeCommandlet |
| `Development/Src/<包>/Classes/*.uc` | UnrealScript 源码 |
| `Script/*.u` | 编译后的脚本包 |
| `Content/` | 资源（UE3 的 `.upk`），不是 `.uc` |
| `Binaries/` | 只允许 exe / dll |

---

## 自己改一改

改完 **重新生成 HelloWorld**（或再跑一次 `Build.bat`）就会重新扫宏、重新 `make` 脚本。

### 1. 改 C++ 类

`Source/Demo/HelloWorld.h`：

```cpp
UCLASS()
class UHelloWorld : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite)
    FString Message;

    UPROPERTY(BlueprintReadOnly)
    int32 RepeatCount = 0;

    UPROPERTY()
    UObject* Child = nullptr;

    UFUNCTION(BlueprintCallable)
    void PrintHello();
};
```

注意：

- `#include "HelloWorld.generated.h"` 必须写在 `UCLASS()` **前面**
- 类里要有 `GENERATED_BODY()`
- 实现写在 `HelloWorld.cpp`

说明符（没有编辑器，所以没有 `EditAnywhere`）：

| 宏 | 作用 |
| --- | --- |
| `UPROPERTY()` | 进 GC。类型是 `UObject*` 时，GC 会沿着它扫 |
| `UPROPERTY(BlueprintReadWrite)` | 脚本和 `SetProperty*` 可读写 |
| `UPROPERTY(BlueprintReadOnly)` | 脚本和反射只读；C++ 直接写 `Hello->RepeatCount` 不受限 |
| `UFUNCTION(BlueprintCallable)` | 脚本 / `ProcessEvent` 可以调这个 native 函数 |

目前属性只认 `int32`、`FString`、`UObject*`。函数只认无参 `void Foo()`。

### 2. 改 UnrealScript

一个 `.uc` 一个 class，文件名和类名相同，放在：

```
Development/Src/HelloWorld/Classes/
```

`HelloWorld.uc` 扩在 C++ 的 `UHelloWorld` 上：

```
class HelloWorld extends UHelloWorld;

native function PrintHello();   // 实现在 C++

function RunDemo()
{
    Message = "你好，来自虚幻脚本";
    PrintHello();
}

event BeginPlay()
{
    CallOtherScript();
}
```

三种调用（都是引擎里有的概念）：

| 方向 | 怎么写 |
| --- | --- |
| 脚本 → C++ | `native function PrintHello();` 然后直接 `PrintHello();` |
| C++ → 脚本 | `.uc` 写 `event BeginPlay()`，C++ 调 `ProcessEvent("BeginPlay")` |
| 脚本 → 脚本（同文件） | 普通 `function`，直接调用 |
| 脚本 → 脚本（跨文件） | 另一个 `.uc` 一个 class，`Other = new class'Greeter'; Other.SayHello();` |

脚本目前只认：

```
log("文字");
log(变量名);
变量 = "文字";
变量 = 123;
Func();
local Greeter Other;
Other = new class'Greeter';
Other.SayHello();
```

### 3. 改主程序入口

`Source/Demo/Main.cpp` 里可以改调用顺序，例如：

```cpp
UHelloWorld* Hello = NewObject<UHelloWorld>();
Hello->AddToRoot();
Hello->ProcessEvent("RunDemo");
Hello->ProcessEvent("BeginPlay");
CollectGarbage();
```

---

## 管线（生成 → 编译）

和虚幻一样分两步，人不用手写 `.generated.h`：

1. **MiniUHT** 扫 `UCLASS` / `UPROPERTY` / `UFUNCTION`，写出  
   `Intermediate/Generated/HelloWorld.generated.h` 和 `.gen.cpp`
2. **UCC make** 扫 `Development/Src/**/*.uc`，写出 `Script/HelloWorld.u`
3. **MSVC** 把 `Source` + 生成代码编成 `Binaries/HelloWorld.exe`

VS 预生成和 `Build.bat` 都会做前两步。空仓库、删掉 `Intermediate` 也能从零生成。

---

## GC（简易 UE4）

- `NewObject<T>()` 创建的对象进入 GC 数组
- `AddToRoot()` 标成根，`RemoveFromRoot()` 取消
- `CollectGarbage()` 从根出发，只沿着 **`UPROPERTY` 的 `UObject*`** 走
- 普通 C++ 裸指针 **不算** 引用，会被收掉

`HelloWorld` 里的演示：

```cpp
Hello->AddToRoot();
Hello->Child = Kept;   // UPROPERTY，Kept 活着
Dropped;               // 没挂到 UPROPERTY 上，会被收掉
CollectGarbage();
```

---

## 常见问题

**F5 之后没有脚本输出**  
看控制台有没有 `Loaded package HelloWorld`。没有的话检查 `Script\HelloWorld.u` 在不在，以及是否在仓库根目录运行 exe（VS 的工作目录一般就是仓库根）。

**中文乱码**  
源文件和 `.u` 都是 UTF-8。程序启动会切控制台到 UTF-8。请用 Windows Terminal，或 `chcp 65001`。

**改了 `.uc` 没生效**  
重新生成 HelloWorld，让预生成事件再跑 `UCC make`。

**`Property not Blueprint writable`**  
这是预期行为：`BlueprintReadOnly` 的字段，脚本和 `SetProperty*` 不能写，C++ 直接赋成员可以。

**不要把 `.uc` 放进 `Content/`**  
UE3 也是这样：`Content` 是资源，脚本源码在 `Development/Src/<包>/Classes/`。

---

## 刻意没做的

模块、蓝图编辑器、多参数函数、`Super.`、跨包 `EditPackages`、完整 UnrealScript、真正的 UObject 序列化。属性、函数、GC 都只保留能把概念讲清楚的最小集合。
