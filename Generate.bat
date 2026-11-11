@echo off
setlocal
cd /d "%~dp0"

REM MiniUBT generate: scan headers, write .generated.h / .gen.cpp.
REM Does not compile C++. Safe to run with an empty Intermediate folder.

echo ==== MiniUBT generate ====
dotnet run --project "%~dp0Tools\MiniUBT\MiniUBT.csproj" -c Release -- generate
if errorlevel 1 (
    echo Generate failed.
    exit /b 1
)
endlocal
