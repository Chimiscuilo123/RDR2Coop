@echo off
cd /d "%~dp0"

echo.
echo ============================================================
echo   RDR2Coop  ^|  Compilar CLIENTE (C++ DLL)
echo ============================================================
echo.

if not exist dist\Client\RDR2Coop mkdir dist\Client\RDR2Coop

echo Compilando...
echo.

for /f "delims=" %%I in ('C:\msys64\usr\bin\cygpath.exe -u "%~dp0"') do set "MSYS_PROJECT_DIR=%%I"
C:\msys64\usr\bin\bash.exe -c "export PATH=/mingw64/bin:/usr/bin:$PATH && cd '%MSYS_PROJECT_DIR%' && g++ -std=c++17 -shared -o 'dist/Client/RDR2Coop/RDR2Coop.asi' 'RDR2Coop.Client.Cpp/main.cpp' -I 'RDR2Coop.Client.Cpp' -Wl,-Bstatic -lstdc++ -lgcc_eh -lgcc -lwinpthread -Wl,-Bdynamic -lws2_32 -Wl,--subsystem,windows -O2"

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Fallo la compilacion. Revisa los mensajes de arriba.
) else (
    echo.
    echo [OK] dist\Client\RDR2Coop\RDR2Coop.asi generado.
    echo.
    copy "dist\Client\RDR2Coop\RDR2Coop.asi" "dist\Client\RDR2Coop.asi" >nul 2>&1
    echo  Copia la carpeta RDR2Coop a tu carpeta de RDR2
    echo  y renombra dist\Client\RDR2Coop.asi si tu ASI loader solo lee del root.
)

echo.
pause
