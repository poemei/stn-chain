@echo off
setlocal

rem Copyright (c) 2026 STN-Labz. See docs\LICENSE.md.
rem
rem STN Chain Windows x64 command-line build
rem Small. Deterministic. Easy to Use.
rem
rem Run from an ordinary Command Prompt. MSVC Build Tools + Windows SDK suffice.

cd /d "%~dp0"

if /i "%~1"=="clean" goto clean
if /i "%~1"=="test-contract" goto setup
if not "%~1"=="" (
    echo Usage: build.cmd [clean^|test-contract]
    exit /b 1
)

:setup
rem Find either standalone Build Tools or an installed IDE toolchain.
set "STN_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "STN_TOOLCHAIN="
if exist "%STN_VSWHERE%" (
    for /f "usebackq delims=" %%I in (`"%STN_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "STN_TOOLCHAIN=%%I"
)
if defined STN_TOOLCHAIN (
    call "%STN_TOOLCHAIN%\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64
    if errorlevel 1 goto fail
) else (
    if /i not "%VSCMD_ARG_TGT_ARCH%"=="x64" (
        echo ERROR: Install Microsoft C++ Build Tools with the x64 compiler and Windows SDK.
        echo The Visual Studio IDE is not required.
        exit /b 1
    )
)
where cl >nul 2>nul
if errorlevel 1 goto fail
where link >nul 2>nul
if errorlevel 1 goto fail

set "BUILD_DIR=build"
set "OBJ_DIR=%BUILD_DIR%\obj"
set "TARGET=%BUILD_DIR%\stn-chain.exe"

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if errorlevel 1 goto fail

if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"
if errorlevel 1 goto fail

set "CFLAGS=/nologo /std:c17 /W4 /WX /O2 /MT /TC /DWIN32 /D_WINDOWS /D_CRT_SECURE_NO_WARNINGS"
set "INCLUDES=/Iincludes /Isrc /Isrc\crypto\ed25519_donna /Iplatforms\windows"

if /i "%~1"=="test-contract" goto test_contract

echo.
echo STN Chain Windows x64 build
echo Compiler: Microsoft cl.exe
echo Target:   %TARGET%
echo.

set "SOURCES= src\main.c  src\stn_address.c  src\stn_authority.c  src\stn_block.c  src\stn_chain.c  src\stn_contract.c  src\stn_fork.c  src\stn_identity.c  src\stn_sentinel_intelligence.c  src\stn_lifecycle.c  src\stn_mining.c  src\stn_node_service.c  src\stn_peer.c  src\stn_pending.c  src\stn_pow.c  src\stn_record.c  src\stn_replay.c  src\stn_rpc.c  src\stn_storage.c  src\stn_transaction.c  src\stn_validation.c  src\crypto\ed25519_donna\ed25519_provider.c  platforms\windows\stn_sha256.c  platforms\windows\stn_storage_windows.c  platforms\windows\stn_peer_windows.c  platforms\windows\stn_app_windows.c"

cl %CFLAGS% %INCLUDES% %SOURCES% ^
    /Fo"%OBJ_DIR%\\" ^
    /Fe"%TARGET%" ^
    /link /INCREMENTAL:NO ws2_32.lib bcrypt.lib crypt32.lib advapi32.lib user32.lib

if errorlevel 1 goto fail

echo.
echo BUILD SUCCESSFUL
echo %TARGET%
exit /b 0

:test_contract
set "TEST_TARGET=%BUILD_DIR%\test-contract.exe"

echo.
echo STN Chain Contract v1 qualification test
echo Compiler: Microsoft cl.exe
echo Target:   %TEST_TARGET%
echo.

cl %CFLAGS% %INCLUDES% /DSTN_CONTRACT_TEST_MAIN ^
    tests\test_contract.c ^
    src\stn_contract.c ^
    src\stn_address.c ^
    platforms\windows\stn_sha256.c ^
    /Fo"%OBJ_DIR%\\" ^
    /Fe"%TEST_TARGET%" ^
    /link /INCREMENTAL:NO bcrypt.lib

if errorlevel 1 goto fail

echo.
echo Running Contract v1 qualification test...
"%TEST_TARGET%"
if errorlevel 1 goto test_fail

echo.
echo CONTRACT TEST SUCCESSFUL
exit /b 0

:test_fail
echo.
echo CONTRACT TEST FAILED
exit /b 1

:clean
rem Validate the exact workspace child before native PowerShell deletion.
powershell -NoProfile -Command "$root=(Get-Location).Path; $target=[IO.Path]::GetFullPath((Join-Path $root 'build')); if ([IO.Path]::GetDirectoryName($target) -ne $root) { exit 1 }; if (Test-Path -LiteralPath $target) { $item=Get-Item -LiteralPath $target -Force; if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { exit 1 }; Remove-Item -LiteralPath $target -Recurse -Force -ErrorAction Stop }"
if errorlevel 1 goto fail
echo Build files removed.
exit /b 0

:fail
echo.
echo BUILD FAILED
exit /b 1
