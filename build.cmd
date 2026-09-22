@echo off
setlocal

rem Copyright (c) 2026 STN-Labz. See docs\LICENSE.md.
rem
rem STN Chain Windows x64 command-line build
rem Small. Deterministic. Easy to Use.
rem
rem Run from an x64 Native Tools Command Prompt for Visual Studio.

cd /d "%~dp0"

where cl >nul 2>nul
if errorlevel 1 (
    echo ERROR: cl.exe was not found.
    echo Run build.cmd from an x64 Native Tools Command Prompt for Visual Studio.
    exit /b 1
)

where lib >nul 2>nul
if errorlevel 1 (
    echo ERROR: lib.exe was not found.
    echo Run build.cmd from an x64 Native Tools Command Prompt for Visual Studio.
    exit /b 1
)

if /i "%~1"=="clean" goto clean
if not "%~1"=="" (
    echo Usage: build.cmd [clean]
    exit /b 1
)

set "BUILD_DIR=build"
set "OBJ_DIR=%BUILD_DIR%\obj"
set "TARGET=%BUILD_DIR%\stn-chain.exe"

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if errorlevel 1 goto fail

if exist "%OBJ_DIR%" rmdir /s /q "%OBJ_DIR%"
mkdir "%OBJ_DIR%"
if errorlevel 1 goto fail

echo.
echo STN Chain Windows x64 build
echo Compiler: Microsoft cl.exe
echo Target:   %TARGET%
echo.

set "CFLAGS=/nologo /std:c17 /W4 /O2 /TC /DWIN32 /D_WINDOWS /D_CRT_SECURE_NO_WARNINGS"
set "INCLUDES=/Iincludes /Isrc /Isrc\crypto\ed25519_donna /Iplatforms\windows"

set "SOURCES=^
src\main.c ^
src\stn_address.c ^
src\stn_authority.c ^
src\stn_block.c ^
src\stn_chain.c ^
src\stn_contract.c ^
src\stn_fork.c ^
src\stn_identity.c ^
src\stn_sentinel_intelligence.c ^
src\stn_lifecycle.c ^
src\stn_mining.c ^
src\stn_node_service.c ^
src\stn_peer.c ^
src\stn_pending.c ^
src\stn_pow.c ^
src\stn_record.c ^
src\stn_replay.c ^
src\stn_rpc.c ^
src\stn_storage.c ^
src\stn_transaction.c ^
src\stn_validation.c ^
src\crypto\ed25519_donna\ed25519_provider.c ^
platforms\windows\stn_sha256.c ^
platforms\windows\stn_storage_windows.c ^
platforms\windows\stn_peer_windows.c ^
platforms\windows\stn_app_windows.c"

cl %CFLAGS% %INCLUDES% %SOURCES% ^
    /Fo"%OBJ_DIR%\\" ^
    /Fe"%TARGET%" ^
    /link /INCREMENTAL:NO ws2_32.lib bcrypt.lib crypt32.lib advapi32.lib user32.lib

if errorlevel 1 goto fail

echo.
echo BUILD SUCCESSFUL
echo %TARGET%
exit /b 0

:clean
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
echo Build files removed.
exit /b 0

:fail
echo.
echo BUILD FAILED
exit /b 1
