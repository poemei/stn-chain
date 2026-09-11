# Copyright (c) 2026 STN-Labz. See docs/LICENSE.md.
# Real executable / TCP / CNG / NTFS integration. No mining search loop.
param([string]$Executable = "$PSScriptRoot\..\build\x64\Release\stn-chain.exe", [switch]$PendingRpcOnly, [switch]$LibraryOnly, [switch]$OutboundOnly, [switch]$DiscoveryOnly)
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
function Request($stream, [int]$method, [byte[]]$payload, [int]$version = 2) {
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
function StartNode([string]$data, [bool]$once = $true, [string]$genesis = '', [int]$rpcPort = 0, [string]$peer = '') {
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = [IO.Path]::GetFullPath($Executable)
    $info.Arguments = '--dev --once --rpc-port 0 --data "' + $data + '"'
    $info.Arguments = $info.Arguments.Replace('--rpc-port 0', '--rpc-port ' + $rpcPort)
    if ($genesis) { $info.Arguments = $info.Arguments.Replace('--dev', '--genesis "' + $genesis + '"') }
    if (!$once) { $info.Arguments = $info.Arguments.Replace('--once ', '') }
    if ($peer) { $info.Arguments += ' --peer ' + $peer }
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
function Transaction([int]$nonce) {
    $tx=[byte[]]::new(244)
    [Text.Encoding]::ASCII.GetBytes('STNT').CopyTo($tx,0); $tx[5]=1;$tx[7]=1;$tx[11]=232
    [Text.Encoding]::ASCII.GetBytes('STNR').CopyTo($tx,12)
    $fields=@{5=1;7=1;8=1;40=2;72=3;103=$nonce;111=10;115=52;117=1;125=5;126=1;128=1;129=97;131=1;132=98;134=1;135=99;136=1;168=4}
    foreach($key in $fields.Keys){$tx[12+$key]=$fields[$key]}
    return ,$tx
}
function Digest([string]$domain,[byte[]]$bytes) {
    return ,$sha.ComputeHash([byte[]]([Text.Encoding]::ASCII.GetBytes($domain)+[byte]0+$bytes))
}
function Hex([byte[]]$bytes) { return [BitConverter]::ToString($bytes).Replace('-','') }
function Solve([byte[]]$work,[bool]$valid) {
    $solved=[byte[]]$work.Clone(); $found=$false
    for([UInt64]$nonce=4294967296;$nonce -lt 4294968320;$nonce++) {
        (NumberBytes $nonce 8).CopyTo($solved,220)
        $digest=Digest 'STN-CHAIN:BLOCK:ID:1' $solved[68..235]
        # Exact unsigned big-endian comparison against Chain-issued target.
        $meets=([String]::CompareOrdinal((Hex $digest),(Hex $work[188..219])) -le 0)
        if($meets -eq $valid){$found=$true;break}
    }
    Check $found 'bounded Chain-target solution'
    Check ((Hex $solved[0..219]) -eq (Hex $work[0..219]) -and
        (Hex $solved[228..($work.Length-1)]) -eq (Hex $work[228..($work.Length-1)])) 'nonce-only mutation'
    Check ((ReadNumber $solved[220..227]) -ge 4294967296) '64-bit nonce exercised'
    return ,$solved
}
if ($LibraryOnly) { return }
$directory = Join-Path ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($Executable))) ('node-test-' + [Guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $directory
$data = Join-Path $directory 'chain.stns'
$genesisPath = Join-Path $directory 'genesis.block'
$script:processes = @(); $script:clients = @()
try {
    $node = StartNode $data
    Check ($node.Height -eq 0) 'genesis startup'
    $anchor = Request $node.Stream 2 (NumberBytes 0 8)
    Check ($anchor.Code -eq 0 -and $anchor.Payload.Length -eq 364) 'explicit normal-mode genesis'
    [IO.File]::WriteAllBytes($genesisPath, $anchor.Payload)
    if ($DiscoveryOnly) {
        $node.Client.Close();Check ($node.Process.WaitForExit(5000)) 'bootstrap stopped'
        $peerA=[Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0)
        $peerB=[Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0)
        $peerA.Start();$peerB.Start()
        function ServeDiscovery($remote,[int]$port,[bool]$malformed) {
            $remote.ReceiveTimeout=10000;$remote.SendTimeout=5000;$stream=$remote.GetStream()
            foreach($expected in @(1,2,3,7)) {
                $header=ReadExact $stream 12
                Check (([Text.Encoding]::ASCII.GetString($header,0,4)) -eq 'STNP' -and (ReadNumber $header[4..5]) -eq 2) 'discovery STNP v2'
                Check ((ReadNumber $header[6..7]) -eq $expected) 'establishment then one discovery request'
                $payload=ReadExact $stream ([int](ReadNumber $header[8..11]))
                if($expected -eq 1){$response=$payload;$response[67]=3;$type=1}
                elseif($expected -eq 2){$response=[byte[]]::new(84);$response[83]=1;$type=2}
                elseif($expected -eq 3){$response=[byte[]]((NumberBytes 0 4)+(NumberBytes 1 4)+$anchor.Payload[0..167]);$type=4}
                else {
                    Check ($payload.Length -eq 0) 'empty bounded discovery request'
                    $response=[byte[]]((NumberBytes 1 2)+[byte[]]@(127,0,0,1)+(NumberBytes $port 2));$type=8
                    if($malformed){$response[6]=0;$response[7]=0}
                }
                $frame=[byte[]]([Text.Encoding]::ASCII.GetBytes('STNP')+(NumberBytes 2 2)+(NumberBytes $type 2)+(NumberBytes $response.Length 4)+$response)
                $stream.Write($frame,0,$frame.Length)
            }
        }
        try {
            $before=[IO.File]::ReadAllBytes($data)
            $node=StartNode $data $false $genesisPath 0 ("127.0.0.1:"+$peerA.LocalEndpoint.Port)
            $accept=$peerA.AcceptTcpClientAsync();Check ($accept.Wait(10000)) 'configured A connected'
            $a=$accept.Result;$script:clients+=$a
            ServeDiscovery $a $peerB.LocalEndpoint.Port $false
            $r=Request $node.Stream 1 @();Check ($r.Code -eq 0 -and (ReadNumber $r.Payload[64..71]) -eq 0) 'discovery alone leaves accepted state unchanged'
            Check ((Hex ([IO.File]::ReadAllBytes($data))) -eq (Hex $before)) 'discovery leaves canonical storage unchanged'
            $a.Close()
            $accept=$peerB.AcceptTcpClientAsync();Check ($accept.Wait(15000)) 'discovered B selected after A loss'
            $b=$accept.Result;$script:clients+=$b
            ServeDiscovery $b $peerA.LocalEndpoint.Port $true
            $r=Request $node.Stream 1 @();Check ($r.Code -eq 0 -and (ReadNumber $r.Payload[64..71]) -eq 0) 'malformed discovery isolated from established RPC'
            $r=Request $node.Stream 0x1004 @();Check ($r.Code -eq 0 -and (ReadNumber $r.Payload[0..3]) -eq 0) 'malformed discovery leaves pending unchanged'
            Check ((Hex ([IO.File]::ReadAllBytes($data))) -eq (Hex $before)) 'malformed discovery leaves storage unchanged'
            Write-Output "Peer discovery executable: $script:checks checks, 0 failures."
        } finally {$peerA.Stop();$peerB.Stop()}
        return
    }    if ($OutboundOnly) {
        $node.Client.Close(); Check ($node.Process.WaitForExit(5000)) 'bootstrap stopped'
        $listener=[Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0);$listener.Start()
        try {
            $peerPort=$listener.LocalEndpoint.Port
            $node=StartNode $data $false $genesisPath 0 ("127.0.0.1:"+$peerPort)
            $accept=$listener.AcceptTcpClientAsync();Check ($accept.Wait(10000)) 'automatic outbound accepted'
            $remote=$accept.Result;$script:clients+=$remote;$remote.ReceiveTimeout=10000;$remote.SendTimeout=5000;$stream=$remote.GetStream()
            # Serve the existing canonical genesis as evidence; headers are reused
            # from local storage only after equality and ordinary revalidation.
            foreach($expected in @(1,2,3,2,3)) {
                $header=ReadExact $stream 12
                Check (([Text.Encoding]::ASCII.GetString($header,0,4)) -eq 'STNP' -and (ReadNumber $header[4..5]) -eq 2) 'STNP v2'
                Check ((ReadNumber $header[6..7]) -eq $expected) 'handshake once, then maintained session'
                $payload=ReadExact $stream ([int](ReadNumber $header[8..11]))
                if($expected -eq 1){$response=$payload;$type=1}
                elseif($expected -eq 2){$response=[byte[]]::new(84);$response[83]=1;$type=2}
                else {$response=[byte[]]((NumberBytes 0 4)+(NumberBytes 1 4)+$anchor.Payload[0..167]);$type=4}
                $frame=[byte[]]([Text.Encoding]::ASCII.GetBytes('STNP')+(NumberBytes 2 2)+(NumberBytes $type 2)+(NumberBytes $response.Length 4)+$response)
                $stream.Write($frame,0,$frame.Length)
            }
            $r=Request $node.Stream 1 @();Check ($r.Code -eq 0 -and (ReadNumber $r.Payload[64..71]) -eq 0) 'inbound RPC survives outbound validation'
            $remote.Close()
            $accept=$listener.AcceptTcpClientAsync();Check ($accept.Wait(15000)) 'automatic reconnect after loss'
            $remote=$accept.Result;$script:clients+=$remote;$remote.ReceiveTimeout=10000;$stream=$remote.GetStream()
            $header=ReadExact $stream 12;$null=ReadExact $stream ([int](ReadNumber $header[8..11]))
            # A partial malformed response must exhaust the bounded operation,
            # release dispatch exclusion and leave established RPC usable.
            $stream.WriteByte(0)
            Start-Sleep -Milliseconds 6200
            $r=Request $node.Stream 1 @();Check ($r.Code -eq 0 -and (ReadNumber $r.Payload[64..71]) -eq 0) 'partial outbound cannot alter state or strand RPC'
            $r=Request $node.Stream 0x1004 @();Check ($r.Code -eq 0 -and (ReadNumber $r.Payload[0..3]) -eq 0) 'pending unchanged'
            Write-Output "Automatic outbound executable: $script:checks checks, 0 failures."
        } finally { $listener.Stop() }
        return
    }    if ($PendingRpcOnly) {
        $node.Client.Close(); Check ($node.Process.WaitForExit(5000)) 'bootstrap stopped'
        $node = StartNode $data $false $genesisPath
        $submission = [byte[]]::new(244)
        [Text.Encoding]::ASCII.GetBytes('STNT').CopyTo($submission, 0)
        $submission[5]=1; $submission[7]=1; $submission[11]=232
        [Text.Encoding]::ASCII.GetBytes('STNR').CopyTo($submission, 12)
        $fields = @{5=1;7=1;8=1;40=2;72=3;103=1;111=10;115=52;117=1;125=5;126=1;128=1;129=97;131=1;132=98;134=1;135=99;136=1;168=4}
        foreach ($key in $fields.Keys) { $submission[12+$key] = $fields[$key] }
        $streams = @($node.Stream)
        for ($i=0; $i -lt 2; $i++) {
            $client = [Net.Sockets.TcpClient]::new('127.0.0.1', $node.Port)
            $client.ReceiveTimeout=5000; $client.SendTimeout=5000; $script:clients += $client
            $streams += $client.GetStream()
        }
        # Queue all clients before reading any reply; one has a bad STNC version.
        for ($i=0; $i -lt 3; $i++) {
            $version = if ($i -eq 0) { 1 } else { 2 }
            $frame = [byte[]]([Text.Encoding]::ASCII.GetBytes('STNC') + (NumberBytes $version 2) +
                (NumberBytes 1 2) + (NumberBytes 0x1005 2) + (NumberBytes 0 2) +
                (NumberBytes 123 8) + (NumberBytes $submission.Length 4) + $submission)
            $streams[$i].Write($frame,0,$frame.Length)
        }
        for ($i=0; $i -lt 3; $i++) {
            $header=ReadExact $streams[$i] 24; $code=ReadNumber $header[10..11]
            $payload=ReadExact $streams[$i] ([int](ReadNumber $header[20..23]))
            if ($i -eq 0) { Check ($code -eq 2 -and $payload.Length -eq 0) 'bad client isolated' }
            else { Check ($code -eq 0 -and $payload.Length -eq 36 -and $payload[3] -eq 7) 'real node fails closed without identity providers' }
            $r=Request $streams[$i] 0x1004 @()
            Check ($r.Code -eq 0 -and $r.Payload.Length -eq 16 -and
                (ReadNumber $r.Payload[0..3]) -eq 0 -and (ReadNumber $r.Payload[4..7]) -eq 128 -and
                (ReadNumber $r.Payload[8..11]) -eq 0 -and (ReadNumber $r.Payload[12..15]) -eq 262144) 'status and session preserved'
        }
        $r=Request $node.Stream 0x1005 ([byte[]]::new(65729))
        Check ($r.Code -eq 1 -and $r.Payload.Length -eq 0) 'oversized submission rejected'
        $r=Request $streams[1] 1 @()
        Check ($r.Code -eq 0 -and (ReadNumber $r.Payload[64..71]) -eq 0) 'other client and chain unchanged'
        Write-Output "Pending RPC executable/TCP: $script:checks checks, 0 failures."
        return
    }
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
    Check ($r.Code -eq 0 -and $r.Payload.Length -eq 80 -and $r.Payload[39] -eq 1) 'atomic solved work'
    $r = Request $node.Stream 0x2003 $work
    Check ($r.Code -eq 10) 'stale work'
    $r = Request $node.Stream 1 @()
    Check ($r.Code -eq 0 -and $r.Payload[71] -eq 1) 'updated info'
    $r = Request $node.Stream 1 @() 1
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
    $partial = [byte[]]([Text.Encoding]::ASCII.GetBytes('STNC') + (NumberBytes 2 2) +
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
    $node = StartNode $data $true $genesisPath
    Check ($node.Height -eq 70) 'normal startup without selected transaction'
    $r = Request $node.Stream 0x1004 @()
    Check ($r.Code -eq 0 -and $r.Payload.Length -eq 16 -and (ReadNumber $r.Payload[0..3]) -eq 0) 'empty memory-only pending'
    $r = Request $node.Stream 0x2002 @()
    Check ($r.Code -eq 5) 'no fixture fallback in normal mode'
    $record = [byte[]]::new(232)
    [Text.Encoding]::ASCII.GetBytes('STNR').CopyTo($record, 0)
    foreach ($pair in @(@(5,1),@(7,1),@(8,1),@(40,2),@(72,3),@(111,10),@(115,52),@(117,1),@(125,5),@(126,1),@(128,1),@(129,97),@(131,1),@(132,98),@(134,1),@(135,99),@(136,1),@(168,4))) { $record[$pair[0]] = $pair[1] }
    $r = Request $node.Stream 0x1001 $record
    Check ($r.Code -eq 0 -and $r.Payload.Length -eq 56 -and (ReadNumber $r.Payload[2..3]) -eq 4) 'missing authentication context fails closed'
    $r = Request $node.Stream 0x1004 @()
    Check ($r.Code -eq 0 -and (ReadNumber $r.Payload[0..3]) -eq 0) 'unavailable submission never enqueued'
    $node.Client.Dispose()
    Check ($node.Process.WaitForExit(10000) -and $node.Process.ExitCode -eq 0) 'normal-mode exit'
    Write-Output "Executable/TCP/NTFS: $script:checks checks, 0 failures."
} finally {
    foreach ($client in $script:clients) { $client.Dispose() }
    foreach ($p in $script:processes) { if (!$p.HasExited) { $p.Kill(); $p.WaitForExit() }; $p.Dispose() }
    foreach ($file in @($data, "$data.lock", "$data.stage", $genesisPath)) {
        if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file }
    }
    Remove-Item -LiteralPath $directory
}
