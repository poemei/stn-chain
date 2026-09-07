param([Parameter(Mandatory=$true)][string]$Compiler)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$outputDirectory = Join-Path $repo 'build/platform-probes'
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$undefines = @('_WIN32','_WIN64','__linux__','__APPLE__','__MACH__','_M_IX86','_M_X64','_M_AMD64','__i386__','__x86_64__','_M_ARM','__arm__','_M_ARM64','__aarch64__','_M_ARM64EC') | ForEach-Object { '/U' + $_ }
$checks=0
function Invoke-Probe($name,$defines,$expectedError) {
    $object = Join-Path $outputDirectory ($name+'.obj')
    $log = & $Compiler /nologo /c /TC /std:c17 /W4 /WX @undefines @defines ('/Fo'+$object) (Join-Path $repo 'tests/platform_probe.c') 2>&1
    $exitCode=$LASTEXITCODE
    if($expectedError) {
        if($exitCode -eq 0 -or ($log -join "`n") -notmatch $expectedError){throw "Expected diagnostic $expectedError in ${name}: $log"}
    } elseif($exitCode -ne 0){throw "Unexpected failure in ${name}: $log"}
    $script:checks++
}
$systems=@(@('/D_WIN32','/DSTN_EXPECT_OS=1'),@('/D__linux__','/DSTN_EXPECT_OS=2'),@('/D__APPLE__','/D__MACH__','/DSTN_EXPECT_OS=3'))
$architectures=@(@('/D_M_IX86','/DSTN_EXPECT_ARCH=1'),@('/D_M_X64','/DSTN_EXPECT_ARCH=2'),@('/D_M_ARM','/DSTN_EXPECT_ARCH=3'),@('/D_M_ARM64','/DSTN_EXPECT_ARCH=4'))
for($os=0;$os -lt 3;$os++) {
    for($arch=0;$arch -lt 4;$arch++) {
        $definitions=@($systems[$os])+@($architectures[$arch])
        Invoke-Probe "detect-$os-$arch" $definitions $null
        $expected=if($os -eq 1){'STN_BACKEND_LINUX_NOT_IMPLEMENTED'}elseif($os -eq 2){'STN_BACKEND_MACOS_NOT_IMPLEMENTED'}elseif($arch -ne 1){'STN_BACKEND_WINDOWS_ARCH_NOT_QUALIFIED'}else{$null}
        Invoke-Probe "backend-$os-$arch" ($definitions+@('/DSTN_PROBE_BACKEND')) $expected
    }
}
Invoke-Probe 'unknown-os' @('/D_M_X64','/DSTN_EXPECT_OS=1','/DSTN_EXPECT_ARCH=2') 'STN_CONFIG_UNKNOWN_OR_AMBIGUOUS_OS'
Invoke-Probe 'unknown-arch' @('/D_WIN32','/DSTN_EXPECT_OS=1','/DSTN_EXPECT_ARCH=2') 'STN_CONFIG_UNKNOWN_OR_AMBIGUOUS_ARCH'
Invoke-Probe 'conflicting-os' @('/D_WIN32','/D__linux__','/D_M_X64','/DSTN_EXPECT_OS=1','/DSTN_EXPECT_ARCH=2') 'STN_CONFIG_UNKNOWN_OR_AMBIGUOUS_OS'
Invoke-Probe 'conflicting-arch' @('/D_WIN32','/D_M_X64','/D_M_ARM64','/DSTN_EXPECT_OS=1','/DSTN_EXPECT_ARCH=2') 'STN_CONFIG_UNKNOWN_OR_AMBIGUOUS_ARCH'
Invoke-Probe 'arm64ec' @('/D_WIN32','/D_M_ARM64EC','/D_M_X64','/DSTN_EXPECT_OS=1','/DSTN_EXPECT_ARCH=2') 'STN_CONFIG_UNSUPPORTED_ARM64EC_ABI'
# GCC/Clang spellings exercise the same centralized selection.
foreach($pair in @(@('__i386__',1),@('__x86_64__',2),@('__arm__',3),@('__aarch64__',4))) {
    Invoke-Probe ('portable-macro-'+$pair[0]) @('/D__linux__',('/D'+$pair[0]),'/DSTN_EXPECT_OS=2',('/DSTN_EXPECT_ARCH='+$pair[1])) $null
}
# Enforce dependency direction without compiling fake OS implementations.
$coreFiles=@(Get-ChildItem (Join-Path $repo 'src') -Filter 'stn_*.*')+@(Get-ChildItem (Join-Path $repo 'includes') -Filter '*.h')
foreach($file in $coreFiles) {
    $text=[IO.File]::ReadAllText($file.FullName)
    if($text -match '(?m)^\s*#\s*include\s*[<"](?:windows\.h|winsock\w*\.h|bcrypt\.h|sys/|unistd\.h|pthread\.h|.*stn_windows|.*stn_build_config|.*stn_backend)' -or
       $text -match '\b(?:_WIN32|_WIN64|_MSC_VER|_M_X64|_M_ARM64|__linux__|__APPLE__|__x86_64__|__aarch64__)\b') {
        throw "Platform dependency leaked into $($file.Name)"
    }
}
$checks++
Write-Output "Platform compile/boundary probes: $checks checks, 0 failures (simulated detection only)."
