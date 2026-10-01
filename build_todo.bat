@echo off
cd /d "%~dp0"

echo.
echo ============================================================
echo   RDR2Coop  ^|  Compilar TODO (Cliente + Plugins + Servidor)
echo ============================================================
echo.

set ERRORS=0

if not exist dist\Client mkdir dist\Client
if not exist dist\Server  mkdir dist\Server
if not exist "dist\Client\RDR2Coop\plugins" mkdir "dist\Client\RDR2Coop\plugins"

:: ── 1. Cerrar servidor si esta corriendo ───────────────────────────────────
echo [1/4] Cerrando servidor si esta en ejecucion...
taskkill /IM RDR2Coop.Server.exe /F >nul 2>&1

:: Esperar hasta que el proceso realmente termine (hasta 10 segundos)
set /a WAIT=0
:wait_loop
tasklist /FI "IMAGENAME eq RDR2Coop.Server.exe" 2>nul | find /I "RDR2Coop.Server.exe" >nul
if %ERRORLEVEL% == 0 (
    if %WAIT% LSS 10 (
        timeout /t 1 /nobreak >nul
        set /a WAIT+=1
        goto wait_loop
    ) else (
        echo [AVISO] El servidor no termino en 10s, continuando de todas formas...
    )
)
echo [OK] Servidor cerrado.
echo.

:: ── 2. Compilar cliente C++ ────────────────────────────────────────────────
echo [2/4] Compilando cliente C++...
echo.

:: Auto-backup before build
call backup.bat

for /f "delims=" %%I in ('C:\msys64\usr\bin\cygpath.exe -u "%~dp0"') do set "MSYS_PROJECT_DIR=%%I"
C:\msys64\usr\bin\bash.exe -c "export PATH=/mingw64/bin:/usr/bin:$PATH && cd '%MSYS_PROJECT_DIR%' && g++ -std=c++17 -shared -o 'dist/Client/RDR2Coop/RDR2Coop.asi' 'RDR2Coop.Client.Cpp/main.cpp' -I 'RDR2Coop.Client.Cpp' -Wl,-Bstatic -lstdc++ -lgcc_eh -lgcc -lwinpthread -Wl,-Bdynamic -lws2_32 -Wl,--subsystem,windows -O2"

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Fallo la compilacion del cliente.
    set ERRORS=1
) else (
    echo [OK] dist\Client\RDR2Coop\RDR2Coop.asi
    copy "dist\Client\RDR2Coop\RDR2Coop.asi" "dist\Client\RDR2Coop.asi" >nul 2>&1
)

:: ── 3. Compilar plugins ────────────────────────────────────────────────────
echo.
echo [3/4] Compilando plugins...
echo.

if not exist "RDR2Coop.Plugins\build" mkdir "RDR2Coop.Plugins\build"

:: WeatherControl
C:\msys64\usr\bin\bash.exe -c "export PATH=/mingw64/bin:/usr/bin:$PATH && cd '%MSYS_PROJECT_DIR%' && g++ -std=c++17 -shared -o 'RDR2Coop.Plugins/build/WeatherControl.dll' 'RDR2Coop.Plugins/WeatherControl/WeatherControl.cpp' -I 'RDR2Coop.Client.Cpp' -Wl,-Bstatic -lstdc++ -lgcc_eh -lgcc -lwinpthread -Wl,-Bdynamic -lws2_32 -Wl,--subsystem,windows -O2"

if %ERRORLEVEL% neq 0 (
    echo [ERROR] WeatherControl.dll fallo
    set ERRORS=1
) else (
    echo [OK] WeatherControl.dll
    copy "RDR2Coop.Plugins\build\WeatherControl.dll" "dist\Client\RDR2Coop\plugins\" >nul 2>&1
)

:: ── 4. Publicar servidor C# ────────────────────────────────────────────────
echo.
echo [4/4] Publicando servidor C#...
echo.

dotnet publish "RDR2Coop.Server\RDR2Coop.Server.csproj" -c Release -r win-x64 --self-contained false -o "dist\Server"

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Fallo la publicacion del servidor.
    set ERRORS=1
) else (
    echo [OK] dist\Server\
)

:: ── Resumen ────────────────────────────────────────────────────────────────
echo.
echo ============================================================
if %ERRORS% == 0 (
    echo   COMPLETADO SIN ERRORES
    echo.
    echo   Cliente  -^>  dist\Client\RDR2Coop\RDR2Coop.asi
    echo   Plugins  -^>  RDR2Coop.Plugins\build\*.dll
    echo   Servidor -^>  dist\Server\RDR2Coop.Server.exe
) else (
    echo   COMPLETADO CON ERRORES - revisa los mensajes de arriba
)
echo ============================================================
echo.
pause
