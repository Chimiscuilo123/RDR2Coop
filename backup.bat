@echo off
cd /d "%~dp0"
:: Auto-backup main.cpp before each build
:: Run manually or hook into build scripts

set BACKUP_DIR=backups
if not exist "%BACKUP_DIR%" mkdir "%BACKUP_DIR%"

set TIMESTAMP=%date:~-4,4%%date:~-7,2%%date:~-10,2%_%time:~0,2%%time:~3,2%%time:~6,2%
set TIMESTAMP=%TIMESTAMP: =0%

copy "RDR2Coop.Client.Cpp\main.cpp" "%BACKUP_DIR%\main_%TIMESTAMP%.cpp" >nul 2>&1
echo [Backup] %BACKUP_DIR%\main_%TIMESTAMP%.cpp

:: Keep only last 10 backups
dir /b /o-d "%BACKUP_DIR%\main_*.cpp" 2>nul | more +10 > "%TEMP%\delbackup.txt"
for /f "delims=" %%f in (%TEMP%\delbackup.txt) do del "%BACKUP_DIR%\%%f" >nul 2>&1
