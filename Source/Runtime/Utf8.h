#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#ifdef RegisterClass
#undef RegisterClass
#endif

// Windows 控制台默认是系统代码页（简体中文是 GBK）。
// 源文件、.uc、.u 都按 UTF-8 存，启动时把控制台切到 UTF-8，中文才能对上。
inline void InitUtf8Console()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}
