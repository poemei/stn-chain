# Copyright (c) 2026 STN-Labz. See docs/LICENSE.md.
# Real executable / TCP / CNG / NTFS integration. No mining search loop.
param([string]$Executable = "$PSScriptRoot\..\build\x64\Release\stn-chain.exe")
$ErrorActionPreference = 'Stop'
$script:checks = 0
function Check($condition, $message) {
    $script:checks++
    if (!$condition) { throw "Node check failed: $message" }
}
function NumberBytes([UInt64]$value, [int]$width) {
    $b = [byte[]]::new($width)
    for ($i = $width - 1; $i -ge 0; $i--) { $b[$i] = [byte]($value -band 255); $value = $value -shr 8 }
    return ,$b
}
function ReadNumber([byte[]]$b) {
    [UInt64]$v = 0
    foreach ($x in $b) { $v = ($v -shl 8) -bor $x }
    return $v
}
function ReadExact($stream, [int]$n) {
    $b = [byte[]]::new($n); $offset = 0
    while ($offset -lt $n) {
        $r = $stream.Read($b, $offset, $n - $offset)
        if ($r -eq 0) { throw 'Unexpected RPC disconnect' }
        $offset += $r
    }
    return ,$b
}
function Request($stream, [int]$method, [byte[]]$payload, [int]$version = 1) {
    $frame = [byte[]]([Text.Encoding]::ASCII.GetBytes('STNC') + (NumberBytes $version 2) +
        (NumberBytes 1 2) + (NumberBytes $method 2) + (NumberBytes 0 2) +
        (NumberBytes 123 8) + (NumberBytes $payload.Length 4) + $payload)
    # Fragment the header deliberately; exact-read transport must reassemble it.
    $stream.Write($frame, 0, 7); $stream.Write($frame, 7, $frame.Length - 7)
    $header = ReadExact $stream 24
    Check ([Text.Encoding]::ASCII.GetString($header, 0, 4) -eq 'STNC') 'response magic'
    $n = ReadNumber $header[20..23]
    Check ($n -le 1051948) 'response bounded'
    return @{ Code = (ReadNumber $header[10..11]); Payload = (ReadExact $stream ([int]$n)) }
}
function StartNode([string]$data) {
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = [IO.Path]::GetFullPath($Executable)
    $info.Arguments = '--dev --once --rpc-port 0 --data "' + $data + '"'
    $info.UseShellExecute = $false; $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true; $info.RedirectStandardError = $true
    $p = [Diagnostics.Process]::Start($info)
    $script:processes += $p
    $line = $p.StandardOutput.ReadLineAsync()
    Check ($line.Wait(10000)) 'startup completed'
    Check ($line.Result -match 'RPC 127\.0\.0\.1:(\d+); height (\d+)') 'loopback startup'
    $port = [int]$Matches[1]; $height = [int]$Matches[2]
    $client = [Net.Sockets.TcpClient]::new('127.0.0.1', $port)
    $script:clients += $client
    $client.ReceiveTimeout = 5000; $client.SendTimeout = 5000
    return @{ Process = $p; Client = $client; Stream = $client.GetStream(); Height = $height }
}
$directory = Join-Path ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($Executable))) ('node-test-' + [Guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $directory
$data = Join-Path $directory 'chain.stns'
$script:processes = @(); $script:clients = @()
try {
    $node = StartNode $data
    Check ($node.Height -eq 0) 'genesis startup'
    $r = Request $node.Stream 0x2002 @()
    Check ($r.Code -eq 0 -and $r.Payload.Length -eq 432) 'template retrieval'
    [byte[]]$work = $r.Payload
    Check (([BitConverter]::ToString($work[32..63])).Replace('-', '') -eq '59F6D10B443DCF2BF0BB37F021B3C5644A66E71839BE8380D956CBF2CA4B76C7') 'independent work ID'
    $r = Request $node.Stream 0x2002 @()
    Check ([Convert]::ToBase64String($r.Payload) -eq [Convert]::ToBase64String($work)) 'repeatability'
    $r = Request $node.Stream 0x2000 @()
    Check ($r.Code -eq 0 -and $r.Payload[75] -eq 1) 'mining available'
    [byte[]]$bad = $work.Clone(); $bad[227] = 1
    $r = Request $node.Stream 0x2003 $bad
    Check ($r.Code -eq 7 -and $r.Payload.Length -eq 0) 'insufficient real PoW'
    $r = Request $node.Stream 0x2003 $work
    Check ($r.Code -eq 0 -and $r.Payload.Length -eq 72 -and $r.Payload[39] -eq 1) 'atomic solved work'
    $r = Request $node.Stream 0x2003 $work
    Check ($r.Code -eq 10) 'stale work'
    $r = Request $node.Stream 1 @()
    Check ($r.Code -eq 0 -and $r.Payload[71] -eq 1) 'updated info'
    $r = Request $node.Stream 1 @() 2
    Check ($r.Code -eq 2) 'unsupported version'
    $r = Request $node.Stream 0x3000 @()
    Check ($r.Code -eq 4) 'admin forbidden'
    $node.Client.Dispose()
    Check ($node.Process.WaitForExit(10000) -and $node.Process.ExitCode -eq 0) 'clean exit'
    $node = StartNode $data
    Check ($node.Height -eq 1) 'persisted restart'
    $r = Request $node.Stream 0x2003 $work
    Check ($r.Code -eq 10) 'stale across restart'
    $r = Request $node.Stream 0x2002 @()
    Check ($r.Code -eq 0 -and $r.Payload[147] -eq 2) 'next height template'
    $node.Client.Dispose()
    Check ($node.Process.WaitForExit(10000) -and $node.Process.ExitCode -eq 0) 'restart exit'
    Write-Output "Executable/TCP/NTFS: $script:checks checks, 0 failures."
} finally {
    foreach ($client in $script:clients) { $client.Dispose() }
    foreach ($p in $script:processes) { if (!$p.HasExited) { $p.Kill(); $p.WaitForExit() }; $p.Dispose() }
    foreach ($file in @($data, "$data.lock", "$data.stage")) {
        if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file }
    }
    Remove-Item -LiteralPath $directory
}
