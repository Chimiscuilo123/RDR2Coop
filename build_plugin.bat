@echo off
cd /d "%~dp0"
:: Build a single plugin from RDR2Coop.Plugins/<name>/
:: Usage: build_plugin.bat WeatherControl
:: Output: RDR2Coop.Plugins/build/<name>.dll
::         and copies to dist\Client\RDR2Coop\plugins\

if "%1"=="" (
    echo Usage: build_plugin.bat PluginName
    echo   PluginName = folder inside RDR2Coop.Plugins\
    echo   Compiles RDR2Coop.Plugins/%1/%1.cpp
    exit /b 1
)

set PLUGIN=%~1
set SRC=RDR2Coop.Plugins/%PLUGIN%/%PLUGIN%.cpp

if not exist "%SRC%" (
    echo [ERROR] %SRC% not found
    exit /b 1
)

if not exist "RDR2Coop.Plugins\build" mkdir "RDR2Coop.Plugins\build"

echo Compilando plugin: %PLUGIN%
echo.

for /f "delims=" %%I in ('C:\msys64\usr\bin\cygpath.exe -u "%~dp0"') do set "MSYS_PROJECT_DIR=%%I"
C:\msys64\usr\bin\bash.exe -c "export PATH=/mingw64/bin:/usr/bin:$PATH && cd '%MSYS_PROJECT_DIR%' && g++ -std=c++17 -shared -o 'RDR2Coop.Plugins/build/%PLUGIN%.dll' '%SRC%' -I 'RDR2Coop.Client.Cpp' -Wl,-Bstatic -lstdc++ -lgcc_eh -lgcc -lwinpthread -Wl,-Bdynamic -lws2_32 -Wl,--subsystem,windows -O2"

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Fallo la compilacion
    exit /b 1
)

echo [OK] RDR2Coop.Plugins\build\%PLUGIN%.dll

if exist "dist\Client\RDR2Coop\plugins" (
    copy "RDR2Coop.Plugins\build\%PLUGIN%.dll" "dist\Client\RDR2Coop\plugins\" >nul 2>&1
    echo      Copiado a dist\Client\RDR2Coop\plugins\
)
