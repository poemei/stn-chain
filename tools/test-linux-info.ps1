# [AI:GPT-6 | 2026-09-28 14:52:23 UTC]
# Windows-only helper/codec regression. Does not qualify the Linux sockets.
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repo 'build\linux-info'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$runtime = [IO.File]::ReadAllText((Join-Path $repo 'platforms\linux\stn_app_linux.c'))
$first = $runtime.IndexOf('#define APP_INFO_PAYLOAD_SIZE 184u')
$last = $runtime.IndexOf('typedef struct rpc_client {', $first)
if ($first -lt 0 -or $last -lt $first) { throw 'INFO implementation boundaries missing' }
[IO.File]::WriteAllText((Join-Path $out 'stn_linux_info_under_test.inc'), $runtime.Substring($first, $last - $first))
# The codec needs this standalone cursor decoder, but linking the full Chain
# object would pull unrelated consensus code into this helper-only host test.
$chain = [IO.File]::ReadAllText((Join-Path $repo 'src\stn_chain.c'))
$first = $chain.IndexOf('stn_cursor_result stn_chain_cursor_decode(')
$last = $chain.IndexOf('stn_cursor_result stn_chain_cursor_validate(', $first)
if ($first -lt 0 -or $last -lt $first) { throw 'Cursor decoder boundaries missing' }
[IO.File]::AppendAllText((Join-Path $out 'stn_linux_info_under_test.inc'), $chain.Substring($first, $last - $first))
$first = $chain.IndexOf('stn_data_status stn_chain_block_id(')
$last = $chain.IndexOf('/* State metadata has consistency checks', $first)
if ($first -lt 0 -or $last -lt $first) { throw 'Block identifier boundaries missing' }
[IO.File]::AppendAllText((Join-Path $out 'stn_linux_info_under_test.inc'), $chain.Substring($first, $last - $first))
$sources = @('tests/test_linux_info.c','src/stn_rpc.c','src/stn_address.c',
    'src/stn_block.c','src/stn_record.c','src/stn_transaction.c',
    'src/stn_sentinel_intelligence.c','src/stn_pow.c','src/stn_contract_transaction.c',
    'src/stn_contract.c','src/stn_share.c','src/stn_compensation.c','src/stn_issuance.c',
    'src/stn_transfer_envelope.c','src/stn_transfer.c','src/stn_authority.c',
    'src/stn_contract_consensus.c','src/stn_identity.c','src/stn_economy.c',
    'src/crypto/ed25519_donna/ed25519_provider.c','platforms/windows/stn_sha256.c')
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'MSVC toolchain unavailable' }
$lines = @(
    '@echo off',
    ('call "' + $vs + '\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64'),
    'if errorlevel 1 exit /b 1',
    ('cd /d "' + $repo + '"'),
    ('cl /nologo /std:c17 /W4 /WX /O2 /Gy /MT /TC /D_CRT_SECURE_NO_WARNINGS /Iincludes /Isrc /Isrc/crypto/ed25519_donna /Ibuild/linux-info /Fobuild/linux-info/ /Febuild/linux-info/test-linux-info.exe ' + ($sources -join ' ') + ' /link /OPT:REF bcrypt.lib'),
    'if errorlevel 1 exit /b 1',
    'build\linux-info\test-linux-info.exe',
    'exit /b %errorlevel%'
)
[IO.File]::WriteAllLines((Join-Path $out 'run.cmd'), $lines, [Text.Encoding]::ASCII)
& cmd /c (Join-Path $out 'run.cmd')
if ($LASTEXITCODE -ne 0) { throw "INFO regression failed: $LASTEXITCODE" }
# [End AI:GPT-6]
