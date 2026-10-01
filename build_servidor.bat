@echo off
setlocal
cd /d "%~dp0"

echo.
echo ============================================================
echo   RDR2Coop ^| Compilar SERVIDOR (C# .NET)
echo ============================================================
echo.

set PROJ=RDR2Coop.Server\RDR2Coop.Server.csproj
set OUT=dist\Server

if not exist dist\Server mkdir dist\Server

:: Matar proceso del servidor si esta corriendo (evita archivo bloqueado)
echo [0/2] Cerrando servidor si esta en ejecucion...
taskkill /IM RDR2Coop.Server.exe /F >nul 2>&1
timeout /t 1 /nobreak >nul

echo [1/2] Publicando %PROJ% ...
echo.

dotnet publish "%PROJ%" ^
  -c Release ^
  -r win-x64 ^
  --self-contained false ^
  -o "%OUT%"

if %ERRORLEVEL% neq 0 goto :fail

echo.
echo [OK] Servidor publicado en %OUT%
echo.
echo  Ejecuta  dist\Server\RDR2Coop.Server.exe  para iniciar el servidor.
echo.
pause
exit /b 0

:fail
echo.
echo [ERROR] La publicacion fallo. Revisa los mensajes de arriba.
echo         Si el servidor estaba corriendo, cierralo y vuelve a intentarlo.
echo.
pause
exit /b 1
