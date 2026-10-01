@echo off
cd /d "%~dp0"

echo.
echo ============================================================
echo   RDR2Coop  ^|  Compilar PLUGINS
echo ============================================================
echo.

if not exist "RDR2Coop.Plugins\build" mkdir "RDR2Coop.Plugins\build"
set ERRORS=0

:: ── WeatherControl ──────────────────────────────────────────────────────────
echo [1/1] WeatherControl.dll
echo.

for /f "delims=" %%I in ('C:\msys64\usr\bin\cygpath.exe -u "%~dp0"') do set "MSYS_PROJECT_DIR=%%I"
C:\msys64\usr\bin\bash.exe -c "export PATH=/mingw64/bin:/usr/bin:$PATH && cd '%MSYS_PROJECT_DIR%' && g++ -std=c++17 -shared -o 'RDR2Coop.Plugins/build/WeatherControl.dll' 'RDR2Coop.Plugins/WeatherControl/WeatherControl.cpp' -I 'RDR2Coop.Client.Cpp' -Wl,-Bstatic -lstdc++ -lgcc_eh -lgcc -lwinpthread -Wl,-Bdynamic -lws2_32 -Wl,--subsystem,windows -O2"

if %ERRORLEVEL% neq 0 (
    echo [ERROR] WeatherControl.dll fallo
    set ERRORS=1
) else (
    echo [OK] RDR2Coop.Plugins\build\WeatherControl.dll
    if exist "dist\Client\RDR2Coop\plugins" (
        copy "RDR2Coop.Plugins\build\WeatherControl.dll" "dist\Client\RDR2Coop\plugins\" >nul 2>&1
        echo      Copiado a dist\Client\RDR2Coop\plugins\
    )
)

:: ── Resumen ────────────────────────────────────────────────────────────────
echo.
echo ============================================================
if %ERRORS% == 0 (
    echo   PLUGINS COMPILADOS OK
) else (
    echo   COMPLETADO CON ERRORES
)
echo ============================================================
echo.
pause
