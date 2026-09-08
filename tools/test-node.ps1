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
function StartNode([string]$data, [bool]$once = $true) {
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = [IO.Path]::GetFullPath($Executable)
    $info.Arguments = '--dev --once --rpc-port 0 --data "' + $data + '"'
    if (!$once) { $info.Arguments = $info.Arguments.Replace('--once ', '') }
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
    return @{ Process = $p; Client = $client; Stream = $client.GetStream(); Height = $height; Port = $port }
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
    $node = StartNode $data $false
    $others = @()
    # More than the old 16-client ceiling, while the original stays connected.
    for ($i = 0; $i -lt 20; $i++) {
        $client = [Net.Sockets.TcpClient]::new('127.0.0.1', $node.Port)
        $client.ReceiveTimeout = 10000; $client.SendTimeout = 10000
        $script:clients += $client; $others += $client
        $r = Request $client.GetStream() 1 @()
        Check ($r.Code -eq 0) 'concurrent client serviced'
    }
    for ($i = 0; $i -lt 70; $i++) {
        $r = Request $node.Stream 1 @()
        Check ($r.Code -eq 0) 'session survives 64 requests'
    }
    # Independently precomputed real-SHA256 fixture solutions, no search loop.
    $nonces = @(0,0,0,2,1,3,3,6,2,0,0,0,0,1,0,2,2,1,0,0,0,0,0,5,0,1,1,3,0,0,3,0,0,4,2,2,1,0,1,0,0,1,0,0,1,4,0,0,3,1,0,0,0,1,1,1,5,0,0,0,2,3,0,0,0,0,0,1,0,1)
    for ($height = 2; $height -le 70; $height++) {
        $r = Request $node.Stream 0x2002 @()
        Check ($r.Code -eq 0 -and (ReadNumber $r.Payload[140..147]) -eq $height) 'next-height work'
        [byte[]]$solution = $r.Payload
        Check ((ReadNumber $solution[220..227]) -eq 0) 'fresh zero nonce'
        $solution[227] = $nonces[$height - 1]
        $r = Request $others[0].GetStream() 0x2003 $solution
        Check ($r.Code -eq 0 -and (ReadNumber $r.Payload[32..39]) -eq $height) 'real mined extension'
    }
    $r = Request $node.Stream 0x2003 $solution
    Check ($r.Code -eq 10) 'cross-client stale work'
    foreach ($client in $others) { $client.Dispose() }
    # Exercise reclamation while another session keeps making requests.
    for ($i = 0; $i -lt 20; $i++) {
        $client = [Net.Sockets.TcpClient]::new('127.0.0.1', $node.Port)
        $script:clients += $client; $client.ReceiveTimeout = 10000
        $r = Request $client.GetStream() 1 @(); Check ($r.Code -eq 0) 'connection churn'
        $client.Dispose()
    }
    # A frame header and one payload byte have arrived. Crossing the transport's
    # idle poll must retain that frame, while a second healthy session stays alive.
    $idle = [Net.Sockets.TcpClient]::new('127.0.0.1', $node.Port)
    $script:clients += $idle; $idle.ReceiveTimeout = 10000
    $partial = [byte[]]([Text.Encoding]::ASCII.GetBytes('STNC') + (NumberBytes 1 2) +
        (NumberBytes 1 2) + (NumberBytes 2 2) + (NumberBytes 0 2) +
        (NumberBytes 123 8) + (NumberBytes 8 4) + (NumberBytes 70 8))
    $node.Stream.Write($partial, 0, 25)
    Start-Sleep -Seconds 61
    $r = Request $idle.GetStream() 1 @(); Check ($r.Code -eq 0) 'idle session survives 60 seconds'
    $node.Stream.Write($partial, 25, 7)
    $h = ReadExact $node.Stream 24
    Check ((ReadNumber $h[10..11]) -eq 0 -and (ReadNumber $h[20..23]) -eq 364) 'partial frame retained across idle timeout'
    $block = ReadExact $node.Stream 364
    Check ((ReadNumber $block[72..79]) -eq 70) 'partial request correct block'
    $idle.Dispose()
    $node.Client.Dispose(); $node.Process.Kill(); $node.Process.WaitForExit()
    $node = StartNode $data
    Check ($node.Height -eq 70) 'restart beyond height 64'
    $r = Request $node.Stream 0x2002 @()
    Check ($r.Code -eq 0 -and (ReadNumber $r.Payload[140..147]) -eq 71) 'work after long-history restart'
    $node.Client.Dispose()
    Check ($node.Process.WaitForExit(10000) -and $node.Process.ExitCode -eq 0) 'long-history exit'
    Write-Output "Executable/TCP/NTFS: $script:checks checks, 0 failures."
} finally {
    foreach ($client in $script:clients) { $client.Dispose() }
    foreach ($p in $script:processes) { if (!$p.HasExited) { $p.Kill(); $p.WaitForExit() }; $p.Dispose() }
    foreach ($file in @($data, "$data.lock", "$data.stage")) {
        if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file }
    }
    Remove-Item -LiteralPath $directory
}
