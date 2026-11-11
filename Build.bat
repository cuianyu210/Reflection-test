@echo off
setlocal
cd /d "%~dp0"

echo Building MiniUBT...
dotnet build "%~dp0Tools\MiniUBT\MiniUBT.csproj" -c Release -v q
if errorlevel 1 exit /b 1

echo Running MiniUBT build...
dotnet "%~dp0Tools\MiniUBT\bin\Release\net10.0\MiniUBT.dll" build
if errorlevel 1 exit /b 1

echo.
echo Run: Binaries\HelloWorld.exe
echo Binaries contains exe/dll only. Generated files live in Intermediate.
endlocal
