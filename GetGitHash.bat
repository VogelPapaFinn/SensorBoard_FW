@echo off
setlocal

set "GIT_HASH=unknown"
set "TARGET_DIR=include"
set "TARGET_FILE=%TARGET_DIR%\GitHash.h"

if not exist ".git" goto :WriteFile

where git >nul 2>nul
if %errorlevel% neq 0 goto :WriteFile
for /f "tokens=*" %%i in ('git rev-parse --short HEAD 2^>nul') do set "GIT_HASH=%%i"

git diff-index --quiet HEAD --
if %errorlevel% neq 0 set "GIT_HASH=%GIT_HASH%-dirty"

:WriteFile
if not exist "%TARGET_DIR%" mkdir "%TARGET_DIR%"

echo #pragma once > "%TARGET_FILE%"

echo #define GIT_HASH "%GIT_HASH%" >> "%TARGET_FILE%"

echo File created: %TARGET_FILE% (Hash: %GIT_HASH%)

endlocal