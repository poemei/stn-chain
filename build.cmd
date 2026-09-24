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
if /i "%~1"=="test-contract-consensus" goto setup
if /i "%~1"=="test-contract-lineage" goto setup
if /i "%~1"=="test-contract-state" goto setup
if /i "%~1"=="test-contract-snapshot" goto setup
if /i "%~1"=="test-economy" goto setup
if /i "%~1"=="test-share" goto setup
if /i "%~1"=="test-share-replay" goto setup
if /i "%~1"=="test-issuance" goto setup
if /i "%~1"=="test-economic-state" goto setup
if /i "%~1"=="test-compensation-state" goto setup
if /i "%~1"=="test-issuance-binding" goto setup
if /i "%~1"=="test-wallet" goto setup
if /i "%~1"=="test-transfer" goto setup
if /i "%~1"=="test-transfer-replay" goto setup
if /i "%~1"=="test-transfer-binding" goto setup
if /i "%~1"=="test-transfer-authorization" goto setup
if /i "%~1"=="test-transfer-acceptance" goto setup
if /i "%~1"=="test-transfer-envelope" goto setup
if /i "%~1"=="test-transfer-envelope-replay" goto setup
if /i "%~1"=="test-transfer-envelope-authorization" goto setup
if /i "%~1"=="test-transfer-envelope-acceptance" goto setup
if /i "%~1"=="test-transfer-transaction" goto setup
if /i "%~1"=="test-chain" goto setup
if not "%~1"=="" (
    echo Usage: build.cmd [clean^|test-contract^|test-contract-consensus^|test-contract-lineage^|test-contract-state^|test-contract-snapshot^|test-economy^|test-share^|test-share-replay^|test-issuance^|test-economic-state^|test-compensation-state^|test-issuance-binding^|test-wallet^|test-transfer^|test-transfer-replay^|test-transfer-binding^|test-transfer-authorization^|test-transfer-acceptance^|test-transfer-envelope^|test-transfer-envelope-replay^|test-transfer-envelope-authorization^|test-transfer-envelope-acceptance^|test-transfer-transaction^|test-chain]
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
if /i "%~1"=="test-contract-consensus" goto test_contract_consensus
if /i "%~1"=="test-contract-lineage" goto test_contract_lineage
if /i "%~1"=="test-contract-state" goto test_contract_state
if /i "%~1"=="test-contract-snapshot" goto test_contract_snapshot
if /i "%~1"=="test-economy" goto test_economy
if /i "%~1"=="test-share" goto test_share
if /i "%~1"=="test-share-replay" goto test_share_replay
if /i "%~1"=="test-issuance" goto test_issuance
if /i "%~1"=="test-economic-state" goto test_economic_state
if /i "%~1"=="test-compensation-state" goto test_compensation_state
if /i "%~1"=="test-issuance-binding" goto test_issuance_binding
if /i "%~1"=="test-wallet" goto test_wallet
if /i "%~1"=="test-transfer" goto test_transfer
if /i "%~1"=="test-transfer-replay" goto test_transfer_replay
if /i "%~1"=="test-transfer-binding" goto test_transfer_binding
if /i "%~1"=="test-transfer-authorization" goto test_transfer_authorization
if /i "%~1"=="test-transfer-acceptance" goto test_transfer_acceptance
if /i "%~1"=="test-transfer-envelope" goto test_transfer_envelope
if /i "%~1"=="test-transfer-envelope-replay" goto test_transfer_envelope_replay
if /i "%~1"=="test-transfer-envelope-authorization" goto test_transfer_envelope_authorization
if /i "%~1"=="test-transfer-envelope-acceptance" goto test_transfer_envelope_acceptance
if /i "%~1"=="test-transfer-transaction" goto test_transfer_transaction
if /i "%~1"=="test-chain" goto test_chain

echo.
echo STN Chain Windows x64 build
echo Compiler: Microsoft cl.exe
echo Target:   %TARGET%
echo.

set "SOURCES= src\main.c  src\stn_address.c  src\stn_authority.c  src\stn_block.c  src\stn_chain.c  src\stn_economy.c  src\stn_compensation.c  src\stn_issuance.c  src\stn_economic_state.c  src\stn_compensation_state.c  src\stn_issuance_binding.c  src\stn_wallet.c  src\stn_transfer.c  src\stn_transfer_replay.c  src\stn_transfer_binding.c  src\stn_transfer_authorization.c  src\stn_transfer_acceptance.c  src\stn_transfer_envelope.c  src\stn_transfer_envelope_replay.c  src\stn_transfer_envelope_authorization.c  src\stn_transfer_envelope_acceptance.c  src\stn_contract.c  src\stn_contract_consensus.c  src\stn_contract_lineage.c  src\stn_contract_state.c  src\stn_contract_snapshot.c  src\stn_contract_transaction.c  src\stn_fork.c  src\stn_identity.c  src\stn_sentinel_intelligence.c  src\stn_lifecycle.c  src\stn_mining.c  src\stn_node_service.c  src\stn_peer.c  src\stn_pending.c  src\stn_pow.c  src\stn_record.c  src\stn_replay.c  src\stn_rpc.c  src\stn_share.c  src\stn_share_replay.c  src\stn_compensation_state.c  src\stn_storage.c  src\stn_transaction.c  src\stn_validation.c  src\crypto\ed25519_donna\ed25519_provider.c  platforms\windows\stn_sha256.c  platforms\windows\stn_storage_windows.c  platforms\windows\stn_peer_windows.c  platforms\windows\stn_app_windows.c"

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
    src\stn_authority.c ^
    src\stn_identity.c ^
    src\crypto\ed25519_donna\ed25519_provider.c ^
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

:test_contract_consensus
set "TEST_TARGET=%BUILD_DIR%\test-contract-consensus.exe"

echo.
echo STN Chain Contract majority consensus qualification test
echo Compiler: Microsoft cl.exe
echo Target:   %TEST_TARGET%
echo.

cl %CFLAGS% %INCLUDES% /DSTN_CONTRACT_CONSENSUS_TEST_MAIN ^
    tests\test_contract_consensus.c ^
    src\stn_contract_consensus.c ^
    src\stn_contract.c ^
    src\stn_address.c ^
    src\stn_authority.c ^
    src\stn_identity.c ^
    src\crypto\ed25519_donna\ed25519_provider.c ^
    platforms\windows\stn_sha256.c ^
    /Fo"%OBJ_DIR%\\" ^
    /Fe"%TEST_TARGET%" ^
    /link /INCREMENTAL:NO bcrypt.lib

if errorlevel 1 goto fail

echo.
echo Running Contract majority consensus qualification test...
"%TEST_TARGET%"
if errorlevel 1 goto test_fail

echo.
echo CONTRACT CONSENSUS TEST SUCCESSFUL
exit /b 0


:test_contract_lineage
set "TEST_TARGET=%BUILD_DIR%\test-contract-lineage.exe"

echo.
echo STN Chain Contract lineage qualification test
echo Compiler: Microsoft cl.exe
echo Target:   %TEST_TARGET%
echo.

cl %CFLAGS% %INCLUDES% /DSTN_CONTRACT_LINEAGE_TEST_MAIN ^
    tests\test_contract_lineage.c ^
    src\stn_contract_lineage.c ^
    src\stn_contract.c ^
    src\stn_address.c ^
    src\stn_authority.c ^
    src\stn_identity.c ^
    src\crypto\ed25519_donna\ed25519_provider.c ^
    platforms\windows\stn_sha256.c ^
    /Fo"%OBJ_DIR%\\" ^
    /Fe"%TEST_TARGET%" ^
    /link /INCREMENTAL:NO bcrypt.lib

if errorlevel 1 goto fail

echo.
echo Running Contract lineage qualification test...
"%TEST_TARGET%"
if errorlevel 1 goto test_fail

echo.
echo CONTRACT LINEAGE TEST SUCCESSFUL
exit /b 0


:test_contract_state
set "TEST_TARGET=%BUILD_DIR%\test-contract-state.exe"

echo.
echo STN Chain Contract accepted-state qualification test
echo Compiler: Microsoft cl.exe
echo Target:   %TEST_TARGET%
echo.

cl %CFLAGS% %INCLUDES% /DSTN_CONTRACT_STATE_TEST_MAIN ^
    tests\test_contract_state.c ^
    src\stn_contract_state.c ^
    src\stn_contract_consensus.c ^
    src\stn_contract_lineage.c ^
    src\stn_contract.c ^
    src\stn_address.c ^
    src\stn_authority.c ^
    src\stn_identity.c ^
    src\crypto\ed25519_donna\ed25519_provider.c ^
    platforms\windows\stn_sha256.c ^
    /Fo"%OBJ_DIR%\\" ^
    /Fe"%TEST_TARGET%" ^
    /link /INCREMENTAL:NO bcrypt.lib

if errorlevel 1 goto fail
echo.
echo Running Contract accepted-state qualification test...
"%TEST_TARGET%"
if errorlevel 1 goto test_fail
echo.
echo CONTRACT STATE TEST SUCCESSFUL
exit /b 0


:test_contract_snapshot
set "TEST_TARGET=%BUILD_DIR%\test-contract-snapshot.exe"
echo.
echo STN Chain Contract snapshot qualification test
echo Compiler: Microsoft cl.exe
echo Target:   %TEST_TARGET%
echo.
cl %CFLAGS% %INCLUDES% /DSTN_CONTRACT_SNAPSHOT_TEST_MAIN ^
    tests\test_contract_snapshot.c ^
    src\stn_chain.c ^
    src\stn_compensation_state.c ^
    src\stn_share.c ^
    src\stn_share_replay.c ^
    src\stn_block.c ^
    src\stn_transaction.c ^
    src\stn_record.c ^
    src\stn_validation.c ^
    src\stn_sentinel_intelligence.c ^
    src\stn_pow.c ^
    src\stn_lifecycle.c ^
    src\stn_replay.c ^
    src\stn_contract_transaction.c ^
    src\stn_contract_snapshot.c ^
    src\stn_contract_state.c ^
    src\stn_contract_consensus.c ^
    src\stn_contract_lineage.c ^
    src\stn_contract.c ^
    src\stn_address.c ^
    src\stn_authority.c ^
    src\stn_identity.c ^
    src\crypto\ed25519_donna\ed25519_provider.c ^
    platforms\windows\stn_sha256.c ^
    /Fo"%OBJ_DIR%\\" /Fe"%TEST_TARGET%" /link /INCREMENTAL:NO bcrypt.lib
if errorlevel 1 goto fail
echo.
echo Running Contract snapshot qualification test...
"%TEST_TARGET%"
if errorlevel 1 goto test_fail
echo.
echo CONTRACT SNAPSHOT TEST SUCCESSFUL
exit /b 0

:test_economy
set "TEST_TARGET=%BUILD_DIR%\test-economy.exe"
echo.
echo STN Chain Phase 19 economy qualification test
echo Compiler: Microsoft cl.exe
echo Target:   %TEST_TARGET%
echo.
cl %CFLAGS% %INCLUDES% /DSTN_ECONOMY_TEST_MAIN ^
    tests\test_economy.c ^
    src\stn_economy.c ^
    src\stn_pow.c ^
    src\stn_block.c ^
    src\stn_chain.c ^
    src\stn_compensation_state.c ^
    src\stn_share.c ^
    src\stn_share_replay.c ^
    src\stn_transaction.c ^
    src\stn_record.c ^
    src\stn_validation.c ^
    src\stn_sentinel_intelligence.c ^
    src\stn_lifecycle.c ^
    src\stn_replay.c ^
    src\stn_contract_transaction.c ^
    src\stn_contract_snapshot.c ^
    src\stn_contract_state.c ^
    src\stn_contract_consensus.c ^
    src\stn_contract_lineage.c ^
    src\stn_contract.c ^
    src\stn_address.c ^
    src\stn_authority.c ^
    src\stn_identity.c ^
    src\crypto\ed25519_donna\ed25519_provider.c ^
    platforms\windows\stn_sha256.c ^
    /Fo"%OBJ_DIR%\\" /Fe"%TEST_TARGET%" /link /INCREMENTAL:NO bcrypt.lib
if errorlevel 1 goto fail
echo.
echo Running Phase 19 economy qualification test...
"%TEST_TARGET%"
if errorlevel 1 goto test_fail
echo.
echo ECONOMY TEST SUCCESSFUL
exit /b 0


:test_share
set "TEST_TARGET=%BUILD_DIR%\test-share.exe"
echo.
echo STN Chain Phase 19 share evidence qualification test
echo Compiler: Microsoft cl.exe
echo Target:   %TEST_TARGET%
echo.
cl %CFLAGS% %INCLUDES% /DSTN_SHARE_TEST_MAIN ^
    tests\test_share.c ^
    src\stn_share.c ^
    src\stn_economy.c ^
    src\stn_pow.c ^
    src\stn_block.c ^
    src\stn_chain.c ^
    src\stn_compensation_state.c ^
    src\stn_share_replay.c ^
    src\stn_transaction.c ^
    src\stn_record.c ^
    src\stn_validation.c ^
    src\stn_sentinel_intelligence.c ^
    src\stn_lifecycle.c ^
    src\stn_replay.c ^
    src\stn_contract_transaction.c ^
    src\stn_contract_snapshot.c ^
    src\stn_contract_state.c ^
    src\stn_contract_consensus.c ^
    src\stn_contract_lineage.c ^
    src\stn_contract.c ^
    src\stn_address.c ^
    src\stn_authority.c ^
    src\stn_identity.c ^
    src\crypto\ed25519_donna\ed25519_provider.c ^
    platforms\windows\stn_sha256.c ^
    /Fo"%OBJ_DIR%\\" /Fe"%TEST_TARGET%" /link /INCREMENTAL:NO bcrypt.lib
if errorlevel 1 goto fail
echo.
echo Running Phase 19 share evidence qualification test...
"%TEST_TARGET%"
if errorlevel 1 goto test_fail
echo.
echo SHARE EVIDENCE TEST SUCCESSFUL
exit /b 0

:test_share_replay
set "TEST_TARGET=%BUILD_DIR%\test-share-replay.exe"
echo.
echo STN Chain Phase 19 share replay qualification test
echo Compiler: Microsoft cl.exe
echo Target:   %TEST_TARGET%
echo.
cl %CFLAGS% %INCLUDES% /DSTN_SHARE_REPLAY_TEST_MAIN ^
    tests\test_share_replay.c ^
    src\stn_share_replay.c ^
    src\crypto\ed25519_donna\ed25519_provider.c ^
    platforms\windows\stn_sha256.c ^
    /Fo"%OBJ_DIR%\\" /Fe"%TEST_TARGET%" /link /INCREMENTAL:NO
if errorlevel 1 goto fail
echo.
echo Running Phase 19 share replay qualification test...
"%TEST_TARGET%"
if errorlevel 1 goto test_fail
echo.
echo SHARE REPLAY TEST SUCCESSFUL
exit /b 0


:test_issuance
set "TEST_TARGET=%BUILD_DIR%\test-issuance.exe"
echo.
echo STN Chain Phase 19 issuance record qualification test
echo Compiler: Microsoft cl.exe
echo Target:   %TEST_TARGET%
echo.
cl %CFLAGS% %INCLUDES% /DSTN_ISSUANCE_TEST_MAIN ^
    tests\test_issuance.c ^
    src\stn_issuance.c ^
    src\stn_compensation.c ^
    /Fo"%OBJ_DIR%\\" /Fe"%TEST_TARGET%" /link /INCREMENTAL:NO
if errorlevel 1 goto fail
echo.
echo Running Phase 19 issuance record qualification test...
"%TEST_TARGET%"
if errorlevel 1 goto test_fail
echo.
echo ISSUANCE TEST SUCCESSFUL
exit /b 0


:test_economic_state
set "TEST_TARGET=%BUILD_DIR%\test-economic-state.exe"
echo.
echo STN Chain Phase 19 accepted economic state qualification test
echo Compiler: Microsoft cl.exe
echo Target:   %TEST_TARGET%
echo.
cl %CFLAGS% %INCLUDES% /DSTN_ECONOMIC_STATE_TEST_MAIN ^
    tests\test_economic_state.c ^
    src\stn_economic_state.c ^
    src\stn_transfer.c ^
    /Fo"%OBJ_DIR%\\" /Fe"%TEST_TARGET%" /link /INCREMENTAL:NO
if errorlevel 1 goto fail
echo.
echo Running Phase 19 accepted economic state qualification test...
"%TEST_TARGET%"
if errorlevel 1 goto test_fail
echo.
echo ECONOMIC STATE TEST SUCCESSFUL
exit /b 0


:test_compensation_state
set "TEST_TARGET=%BUILD_DIR%\test-compensation-state.exe"
echo.
echo STN Chain Phase 19 compensation mapping state qualification test
echo Compiler: Microsoft cl.exe
echo Target:   %TEST_TARGET%
echo.
cl %CFLAGS% %INCLUDES% /DSTN_COMPENSATION_STATE_TEST_MAIN ^
    tests\test_compensation_state.c ^
    src\stn_compensation_state.c ^
    /Fo"%OBJ_DIR%\\" /Fe"%TEST_TARGET%" /link /INCREMENTAL:NO
if errorlevel 1 goto fail
echo.
echo Running Phase 19 compensation mapping state qualification test...
"%TEST_TARGET%"
if errorlevel 1 goto test_fail
echo.
echo COMPENSATION STATE TEST SUCCESSFUL
exit /b 0


:test_issuance_binding
set "TEST_TARGET=%BUILD_DIR%\test-issuance-binding.exe"
echo.
echo STN Chain Phase 19 share issuance binding qualification test
echo Compiler: Microsoft cl.exe
echo Target:   %TEST_TARGET%
echo.
cl %CFLAGS% %INCLUDES% /DSTN_ISSUANCE_BINDING_TEST_MAIN ^
    tests\test_issuance_binding.c ^
    src\stn_issuance_binding.c ^
    src\stn_share.c ^
    src\stn_compensation_state.c ^
    src\stn_economy.c ^
    src\stn_chain.c ^
    src\stn_block.c ^
    src\stn_transaction.c ^
    src\stn_compensation.c ^
    src\stn_issuance.c ^
    src\stn_economic_state.c ^
    src\stn_pow.c ^
    src\stn_address.c ^
    src\stn_contract_lineage.c ^
    src\stn_contract_state.c ^
    src\stn_contract_consensus.c ^
    src\stn_sentinel_intelligence.c ^
    src\stn_contract_snapshot.c ^
    src\stn_contract_transaction.c ^
    src\stn_contract.c ^
    src\stn_lifecycle.c ^
    src\stn_authority.c ^
    src\stn_record.c ^
    src\stn_validation.c ^
    src\stn_identity.c ^
    src\stn_replay.c ^
    src\stn_share_replay.c ^
    src\crypto\ed25519_donna\ed25519_provider.c ^
    platforms\windows\stn_sha256.c ^
    /Fo"%OBJ_DIR%\\" /Fe"%TEST_TARGET%" /link /INCREMENTAL:NO bcrypt.lib
if errorlevel 1 goto fail
"%TEST_TARGET%"
if errorlevel 1 goto test_fail
echo.
echo ISSUANCE BINDING TEST SUCCESSFUL
exit /b 0


:test_wallet
set "TEST_TARGET=%BUILD_DIR%\test-wallet.exe"
echo.
echo STN Chain Phase 19 wallet primitive qualification test
echo Compiler: Microsoft cl.exe
echo Target:   %TEST_TARGET%
echo.
cl %CFLAGS% %INCLUDES% /DSTN_WALLET_TEST_MAIN ^
    tests\test_wallet.c ^
    src\stn_wallet.c ^
    src\stn_address.c ^
    platforms\windows\stn_sha256.c ^
    /Fo"%OBJ_DIR%\\" /Fe"%TEST_TARGET%" /link /INCREMENTAL:NO bcrypt.lib
if errorlevel 1 goto fail
"%TEST_TARGET%"
if errorlevel 1 goto test_fail
echo.
echo WALLET TEST SUCCESSFUL
exit /b 0


:test_transfer
set "TEST_TARGET=%BUILD_DIR%\test-transfer.exe"
echo.
echo STN Chain Phase 19 transfer primitive qualification test
echo Compiler: Microsoft cl.exe
echo Target:   %TEST_TARGET%
echo.
cl %CFLAGS% %INCLUDES% /DSTN_TRANSFER_TEST_MAIN ^
    tests\test_transfer.c ^
    src\stn_transfer.c ^
    /Fo"%OBJ_DIR%\\" /Fe"%TEST_TARGET%" /link /INCREMENTAL:NO
if errorlevel 1 goto fail
"%TEST_TARGET%"
if errorlevel 1 goto test_fail
echo.
echo TRANSFER TEST SUCCESSFUL
exit /b 0


:test_transfer_replay
set "TEST_TARGET=%BUILD_DIR%\test-transfer-replay.exe"
echo.
echo STN Chain Phase 19 transfer replay qualification test
echo Compiler: Microsoft cl.exe
echo Target:   %TEST_TARGET%
echo.
cl %CFLAGS% %INCLUDES% /DSTN_TRANSFER_REPLAY_TEST_MAIN ^
    tests\test_transfer_replay.c ^
    src\stn_transfer_replay.c ^
    src\stn_transfer.c ^
    /Fo"%OBJ_DIR%\\" /Fe"%TEST_TARGET%" /link /INCREMENTAL:NO
if errorlevel 1 goto fail
"%TEST_TARGET%"
if errorlevel 1 goto test_fail
echo.
echo TRANSFER REPLAY TEST SUCCESSFUL
exit /b 0


:test_transfer_binding
set "TEST_TARGET=%BUILD_DIR%\test-transfer-binding.exe"
echo.
echo STN Chain Phase 19 accepted transfer binding qualification test
echo Compiler: Microsoft cl.exe
echo Target:   %TEST_TARGET%
echo.
cl %CFLAGS% %INCLUDES% /DSTN_TRANSFER_BINDING_TEST_MAIN ^
    tests\test_transfer_binding.c ^
    src\stn_transfer_binding.c ^
    src\stn_transfer_replay.c ^
    src\stn_transfer.c ^
    src\stn_economic_state.c ^
    /Fo"%OBJ_DIR%\\" /Fe"%TEST_TARGET%" /link /INCREMENTAL:NO
if errorlevel 1 goto fail
"%TEST_TARGET%"
if errorlevel 1 goto test_fail
echo.
echo TRANSFER BINDING TEST SUCCESSFUL
exit /b 0


:test_transfer_authorization
set "TEST_TARGET=%BUILD_DIR%\test-transfer-authorization.exe"
echo.
echo STN Chain Phase 19 transfer authorization qualification test
echo Compiler: Microsoft cl.exe
echo Target:   %TEST_TARGET%
echo.
cl %CFLAGS% %INCLUDES% /DSTN_TRANSFER_AUTHORIZATION_TEST_MAIN ^
    tests\test_transfer_authorization.c ^
    src\stn_transfer_authorization.c ^
    src\stn_transfer.c ^
    src\stn_wallet.c ^
    src\stn_address.c ^
    src\stn_identity.c ^
    src\crypto\ed25519_donna\ed25519_provider.c ^
    platforms\windows\stn_sha256.c ^
    /Fo"%OBJ_DIR%\\" /Fe"%TEST_TARGET%" /link /INCREMENTAL:NO bcrypt.lib
if errorlevel 1 goto fail
"%TEST_TARGET%"
if errorlevel 1 goto test_fail
echo.
echo TRANSFER AUTHORIZATION TEST SUCCESSFUL
exit /b 0


:test_transfer_acceptance
set "TEST_TARGET=%BUILD_DIR%\test-transfer-acceptance.exe"
echo.
echo STN Chain Phase 19 authorized transfer acceptance qualification test
echo Compiler: Microsoft cl.exe
echo Target:   %TEST_TARGET%
echo.
cl %CFLAGS% %INCLUDES% /DSTN_TRANSFER_ACCEPTANCE_TEST_MAIN ^
    tests\test_transfer_acceptance.c ^
    src\stn_transfer_acceptance.c ^
    src\stn_transfer_authorization.c ^
    src\stn_transfer_binding.c ^
    src\stn_transfer_replay.c ^
    src\stn_transfer.c ^
    src\stn_economic_state.c ^
    src\stn_wallet.c ^
    src\stn_address.c ^
    src\stn_identity.c ^
    src\crypto\ed25519_donna\ed25519_provider.c ^
    platforms\windows\stn_sha256.c ^
    /Fo"%OBJ_DIR%\\" /Fe"%TEST_TARGET%" /link /INCREMENTAL:NO bcrypt.lib
if errorlevel 1 goto fail
"%TEST_TARGET%"
if errorlevel 1 goto test_fail
echo.
echo TRANSFER ACCEPTANCE TEST SUCCESSFUL
exit /b 0


:test_transfer_envelope
set "TEST_TARGET=%BUILD_DIR%\test-transfer-envelope.exe"
echo.
echo STN Chain Phase 19 canonical transfer envelope qualification test
echo Compiler: Microsoft cl.exe
echo Target:   %TEST_TARGET%
echo.
cl %CFLAGS% %INCLUDES% /DSTN_TRANSFER_ENVELOPE_TEST_MAIN ^
    tests\test_transfer_envelope.c ^
    src\stn_transfer_envelope.c ^
    src\stn_transfer.c ^
    /Fo"%OBJ_DIR%\\" /Fe"%TEST_TARGET%" /link /INCREMENTAL:NO
if errorlevel 1 goto fail
"%TEST_TARGET%"
if errorlevel 1 goto test_fail
echo.
echo TRANSFER ENVELOPE TEST SUCCESSFUL
exit /b 0


:test_transfer_envelope_replay
set "TEST_TARGET=%BUILD_DIR%\test-transfer-envelope-replay.exe"
echo.
echo STN Chain Phase 19 nonce-aware transfer replay qualification test
echo Compiler: Microsoft cl.exe
echo Target:   %TEST_TARGET%
echo.
cl %CFLAGS% %INCLUDES% /DSTN_TRANSFER_ENVELOPE_REPLAY_TEST_MAIN ^
    tests\test_transfer_envelope_replay.c ^
    src\stn_transfer_envelope_replay.c ^
    src\stn_replay.c ^
    src\stn_record.c ^
    src\stn_identity.c ^
    src\crypto\ed25519_donna\ed25519_provider.c ^
    /Fo"%OBJ_DIR%\\" /Fe"%TEST_TARGET%" /link /INCREMENTAL:NO bcrypt.lib
if errorlevel 1 goto fail
"%TEST_TARGET%"
if errorlevel 1 goto test_fail
echo.
echo TRANSFER ENVELOPE REPLAY TEST SUCCESSFUL
exit /b 0


:test_transfer_envelope_authorization
set "TEST_TARGET=%BUILD_DIR%\test-transfer-envelope-authorization.exe"
echo.
echo STN Chain Phase 19 nonce-bound transfer authorization qualification test
echo Compiler: Microsoft cl.exe
echo Target:   %TEST_TARGET%
echo.
cl %CFLAGS% %INCLUDES% /DSTN_TRANSFER_ENVELOPE_AUTHORIZATION_TEST_MAIN ^
    tests\test_transfer_envelope_authorization.c ^
    src\stn_transfer_envelope_authorization.c ^
    src\stn_transfer.c ^
    src\stn_wallet.c ^
    src\stn_address.c ^
    src\stn_identity.c ^
    src\crypto\ed25519_donna\ed25519_provider.c ^
    platforms\windows\stn_sha256.c ^
    /Fo"%OBJ_DIR%\\" /Fe"%TEST_TARGET%" /link /INCREMENTAL:NO bcrypt.lib
if errorlevel 1 goto fail
"%TEST_TARGET%"
if errorlevel 1 goto test_fail
echo.
echo TRANSFER ENVELOPE AUTHORIZATION TEST SUCCESSFUL
exit /b 0


:test_transfer_envelope_acceptance
set "TEST_TARGET=%BUILD_DIR%\test-transfer-envelope-acceptance.exe"
echo.
echo STN Chain Phase 19 nonce-aware transfer acceptance qualification test
echo Compiler: Microsoft cl.exe
echo Target:   %TEST_TARGET%
echo.
cl %CFLAGS% %INCLUDES% /DSTN_TRANSFER_ENVELOPE_ACCEPTANCE_TEST_MAIN ^
    tests\test_transfer_envelope_acceptance.c ^
    src\stn_transfer_envelope_acceptance.c ^
    src\stn_transfer_envelope_authorization.c ^
    src\stn_transfer_envelope_replay.c ^
    src\stn_transfer.c ^
    src\stn_economic_state.c ^
    src\stn_wallet.c ^
    src\stn_address.c ^
    src\stn_replay.c ^
    src\stn_record.c ^
    src\stn_identity.c ^
    src\crypto\ed25519_donna\ed25519_provider.c ^
    platforms\windows\stn_sha256.c ^
    /Fo"%OBJ_DIR%\\" /Fe"%TEST_TARGET%" /link /INCREMENTAL:NO bcrypt.lib
if errorlevel 1 goto fail
"%TEST_TARGET%"
if errorlevel 1 goto test_fail
echo.
echo TRANSFER ENVELOPE ACCEPTANCE TEST SUCCESSFUL
exit /b 0


:test_transfer_transaction
set "TEST_TARGET=%BUILD_DIR%\test-transfer-transaction.exe"
echo.
echo STN Chain Phase 19 transfer transaction qualification test
echo Compiler: Microsoft cl.exe
echo Target:   %TEST_TARGET%
echo.
cl %CFLAGS% %INCLUDES% /DSTN_TRANSFER_TRANSACTION_TEST_MAIN ^
    tests\test_transfer_transaction.c ^
    src\stn_transaction.c ^
    src\stn_transfer_envelope.c ^
    src\stn_transfer.c ^
    src\stn_record.c ^
    src\stn_contract_transaction.c ^
    src\stn_authority.c ^
    src\stn_address.c ^
    src\stn_block.c ^
    src\stn_economy.c ^
    src\stn_pow.c ^
    tests\stn_pow_block_id_stub.c ^
    src\stn_contract.c ^
    src\stn_contract_lineage.c ^
    src\stn_contract_state.c ^
    src\stn_contract_consensus.c ^
    src\stn_share.c ^
    src\stn_compensation.c ^
    src\stn_issuance.c ^
    src\stn_sentinel_intelligence.c ^
    src\stn_identity.c ^
    src\crypto\ed25519_donna\ed25519_provider.c ^
    platforms\windows\stn_sha256.c ^
    /Fo"%OBJ_DIR%\\" /Fe"%TEST_TARGET%" /link /INCREMENTAL:NO bcrypt.lib
if errorlevel 1 goto fail
"%TEST_TARGET%"
if errorlevel 1 goto test_fail
echo.
echo TRANSFER TRANSACTION TEST SUCCESSFUL
exit /b 0



:test_economic_persistence
set "TEST_TARGET=%BUILD_DIR%\\test-economic-persistence.exe"
echo.
echo STN Chain Phase 19 economic persistence qualification test
cl %CFLAGS% %INCLUDES% /DSTN_ECONOMIC_PERSISTENCE_TEST_MAIN ^
    tests\\test_economic_persistence.c src\\stn_economic_persistence.c src\\stn_economic_state.c src\\stn_transfer.c ^
    /Fo"%OBJ_DIR%\\\\" /Fe"%TEST_TARGET%" /link /INCREMENTAL:NO
if errorlevel 1 goto fail
"%TEST_TARGET%"
if errorlevel 1 goto test_fail
echo.
echo ECONOMIC PERSISTENCE TEST SUCCESSFUL
exit /b 0

:test_chain
set "TEST_TARGET=%BUILD_DIR%\\test-chain.exe"
echo.
echo STN Chain accepted-state qualification test
echo Compiler: Microsoft cl.exe
echo Target:   %TEST_TARGET%
echo.
cl %CFLAGS% /experimental:c11atomics %INCLUDES% /DSTN_LIFECYCLE_TEST /DSTN_CHAIN_TEST_MAIN ^
    tests\\test_chain.c ^
    src\\stn_chain.c src\\stn_transaction.c src\\stn_block.c src\\stn_pow.c src\\stn_economy.c src\\stn_record.c src\\stn_validation.c src\\stn_lifecycle.c src\\stn_authority.c ^
    src\\stn_contract.c src\\stn_contract_lineage.c src\\stn_contract_state.c src\\stn_contract_consensus.c src\\stn_contract_snapshot.c src\\stn_contract_transaction.c ^
    src\\stn_share.c src\\stn_share_replay.c src\\stn_compensation.c src\\stn_compensation_state.c src\\stn_issuance.c src\\stn_issuance_binding.c src\\stn_economic_state.c ^
    src\\stn_transfer.c src\\stn_transfer_envelope.c src\\stn_transfer_envelope_authorization.c src\\stn_transfer_envelope_replay.c src\\stn_transfer_envelope_acceptance.c ^
    src\\stn_wallet.c src\\stn_address.c src\\stn_replay.c src\\stn_identity.c src\\stn_sentinel_intelligence.c src\\crypto\\ed25519_donna\\ed25519_provider.c platforms\\windows\\stn_sha256.c ^
    /Fo"%OBJ_DIR%\\\\" /Fe"%TEST_TARGET%" /link /INCREMENTAL:NO bcrypt.lib
if errorlevel 1 goto fail
echo.
echo Running Chain accepted-state qualification test...
"%TEST_TARGET%"
if errorlevel 1 goto test_fail
echo.
echo CHAIN TEST SUCCESSFUL
exit /b 0

:test_fail
echo.
echo ISSUANCE BINDING TEST FAILED
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
