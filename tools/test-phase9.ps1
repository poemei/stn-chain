# Copyright (c) 2026 STN-Labz. See docs/LICENSE.md.
# TEST ONLY: real TCP/CNG/NTFS/runtime, scripted identity providers.
param([string]$BuildDirectory = "$PSScriptRoot\..\build\x64\Release")
. "$PSScriptRoot\test-node.ps1" -LibraryOnly
$production = Join-Path $BuildDirectory 'stn-chain.exe'
$fixture = Join-Path $BuildDirectory 'stn-chain-phase9-test.exe'
$Executable = $production
$script:processes=@(); $script:clients=@()
$directory=Join-Path ([IO.Path]::GetFullPath($BuildDirectory)) ('phase9-test-'+[Guid]::NewGuid().ToString('N'))
$null=New-Item -ItemType Directory -Path $directory
$data=Join-Path $directory 'chain.stns'; $genesisPath=Join-Path $directory 'genesis.block'
$eventName='Local\stn-phase9-'+[Guid]::NewGuid().ToString('N')
$stopEvent=[Threading.EventWaitHandle]::new($false,[Threading.EventResetMode]::ManualReset,$eventName)
$previousEvent=$env:STN_PHASE9_STOP_EVENT; $env:STN_PHASE9_STOP_EVENT=$eventName
$sha=[Security.Cryptography.SHA256]::Create()
function Submit($stream,[byte[]]$tx,[int]$expected) {
    $r=Request $stream 0x1005 $tx
    Check ($r.Code -eq 0 -and $r.Payload.Length -eq 36 -and $r.Payload[3] -eq $expected) "admission $expected"
    if($expected -le 1){Check ((Hex $r.Payload[4..35]) -eq (Hex (Digest 'STN-CHAIN:TX:ID:1' $tx))) 'canonical submission ID'}
    return $r
}
function Pending($stream,[int]$count) {
    $r=Request $stream 0x1004 @()
    Check ($r.Code -eq 0 -and $r.Payload.Length -eq 16 -and (ReadNumber $r.Payload[0..3]) -eq $count -and
        (ReadNumber $r.Payload[8..11]) -eq ($count*244) -and (ReadNumber $r.Payload[4..7]) -eq 128 -and
        (ReadNumber $r.Payload[12..15]) -eq 262144) 'pending count/bytes/capacities'
}
function Template($stream) {
    $r=Request $stream 0x2002 @(); Check ($r.Code -eq 0) 'template available'
    Check ((Hex $r.Payload[32..63]) -eq (Hex (Digest 'STN-CHAIN:WORK:ID:1' $r.Payload[68..($r.Payload.Length-1)]))) 'canonical work identity'
    return ,$r.Payload
}
function StopClean($node) {
    foreach($client in $script:clients){$client.Close()}
    $null=$stopEvent.Set()
    Check ($node.Process.WaitForExit(10000) -and $node.Process.ExitCode -eq 0) 'clean runtime shutdown'
    $null=$stopEvent.Reset()
}
try {
    $node=StartNode $data
    $anchor=Request $node.Stream 2 (NumberBytes 0 8)
    Check ($anchor.Code -eq 0 -and $anchor.Payload.Length -eq 364) 'genesis bootstrap'
    [IO.File]::WriteAllBytes($genesisPath,$anchor.Payload)
    $node.Client.Close();Check ($node.Process.WaitForExit(5000) -and $node.Process.ExitCode -eq 0) 'production bootstrap stopped'
    # Production remains unable to authenticate; its normal runtime is tested too.
    $node=StartNode $data $true $genesisPath
    $first=Transaction 1; $null=Submit $node.Stream $first 7; Pending $node.Stream 0
    $node.Client.Close();Check ($node.Process.WaitForExit(5000) -and $node.Process.ExitCode -eq 0) 'production fail-closed stopped'
    $Executable=$fixture; $node=StartNode $data $false $genesisPath
    $null=Submit $node.Stream $first 0; Pending $node.Stream 1
    $oldWork=Template $node.Stream
    $null=Submit $node.Stream $first 1
    $invalid=[byte[]]$first.Clone();$invalid[0]=0;$null=Submit $node.Stream $invalid 3
    $invalid=[byte[]]$first.Clone();$invalid[52]=9;$null=Submit $node.Stream $invalid 7
    Pending $node.Stream 1
    # Three simultaneous clients: two duplicate submissions and one status read.
    $streams=@();for($i=0;$i -lt 3;$i++) {
        $client=[Net.Sockets.TcpClient]::new('127.0.0.1',$node.Port);$client.ReceiveTimeout=5000
        $script:clients+=$client;$streams+=$client.GetStream()
        $method=if($i -eq 2){0x1004}else{0x1005};$payload=if($i -eq 2){[byte[]]@()}else{$first}
        $frame=[byte[]]([Text.Encoding]::ASCII.GetBytes('STNC')+(NumberBytes 2 2)+(NumberBytes 1 2)+
            (NumberBytes $method 2)+(NumberBytes 0 2)+(NumberBytes 7 8)+(NumberBytes $payload.Length 4)+$payload)
        $streams[$i].Write($frame,0,$frame.Length)
    }
    for($i=0;$i -lt 3;$i++) {
        $h=ReadExact $streams[$i] 24;$p=ReadExact $streams[$i] ([int](ReadNumber $h[20..23]))
        Check ((ReadNumber $h[10..11]) -eq 0) 'concurrent RPC response'
        if($i -eq 2){Check ($p.Length -eq 16 -and $p[3] -eq 1) 'concurrent status'}
        else{Check ($p.Length -eq 36 -and $p[3] -eq 1) 'concurrent duplicate'}
    }
    $ids=@();for($i=1;$i -le 17;$i++) {
        $tx=Transaction $i;$ids+=Hex (Digest 'STN-CHAIN:TX:ID:1' $tx)
        if($i -ne 1){$null=Submit $node.Stream $tx 0}
    }
    [Array]::Sort($ids,[StringComparer]::Ordinal)
    $r=Request $node.Stream 0x2003 $oldWork;Check ($r.Code -eq 10) 'changed-candidate stale work'
    $work=Template $node.Stream;$again=Template $node.Stream
    Check ((Hex $work) -eq (Hex $again)) 'stable candidate bytes';Pending $node.Stream 17
    Check ((ReadNumber $work[228..231]) -eq 16) 'existing count limit'
    $offset=236
    for($i=0;$i -lt 16;$i++) {
        $len=[int](ReadNumber $work[$offset..($offset+3)]);$offset+=4
        Check ($len -eq 244 -and (Hex (Digest 'STN-CHAIN:TX:ID:1' $work[$offset..($offset+$len-1)])) -eq $ids[$i]) 'candidate canonical order'
        $offset+=$len
    }
    Check ($offset -eq $work.Length) 'exact candidate bounds'
    $bad=Solve $work $false;$r=Request $node.Stream 0x2003 $bad;Check ($r.Code -eq 7) 'invalid PoW rejected';Pending $node.Stream 17
    $solved=Solve $work $true;$r=Request $node.Stream 0x2003 $solved
    Check ($r.Code -eq 0 -and (ReadNumber $r.Payload[32..39]) -eq 1) 'accepted and persisted multi-item block';Pending $node.Stream 1
    $remaining=Template $node.Stream
    Check ((Hex (Digest 'STN-CHAIN:TX:ID:1' $remaining[240..483])) -eq $ids[16]) 'only unrelated pending entry remains'
    $acceptedBlock=[byte[]]$solved[68..($solved.Length-1)]
    StopClean $node
    $node=StartNode $data $false $genesisPath;Check ($node.Height -eq 1) 'restored active height';Pending $node.Stream 0
    $r=Request $node.Stream 2 (NumberBytes 1 8);Check ($r.Code -eq 0 -and (Hex $r.Payload) -eq (Hex $acceptedBlock)) 'accepted content reconstructed'
    $included=[byte[]]$acceptedBlock[172..415];$null=Submit $node.Stream $included 5;Pending $node.Stream 0
    # One generated transaction at a time; no accumulated fixture set or soak.
    for($height=2;$height -le 65;$height++) {
        $tx=Transaction ($height+17);$null=Submit $node.Stream $tx 0
        $work=Template $node.Stream
        Check ((ReadNumber $work[140..147]) -eq $height) 'candidate builds from restored/current height'
        $r=Request $node.Stream 1 @();Check ((Hex $work[0..31]) -eq (Hex $r.Payload[72..103])) 'candidate parent is active tip'
        $solved=Solve $work $true;$r=Request $node.Stream 0x2003 $solved
        Check ($r.Code -eq 0 -and (ReadNumber $r.Payload[32..39]) -eq $height) 'continued accepted lifecycle';Pending $node.Stream 0
    }
    $last=[byte[]]$solved[68..($solved.Length-1)];StopClean $node
    $node=StartNode $data $false $genesisPath;Check ($node.Height -eq 65) 'restart beyond height 64'
    $r=Request $node.Stream 2 (NumberBytes 65 8);Check ($r.Code -eq 0 -and (Hex $r.Payload) -eq (Hex $last)) 'height 65 reconstructed'
    $null=Submit $node.Stream (Transaction 83) 0;$work=Template $node.Stream
    Check ((ReadNumber $work[140..147]) -eq 66) 'post-64 restart continued submission/work'
    StopClean $node
    Write-Output "Phase 9 executable lifecycle: $script:checks checks, 0 failures; height 65 restored, candidate 66; SCRIPTED identity providers."
} finally {
    foreach($client in $script:clients){$client.Dispose()}
    foreach($p in $script:processes){if(!$p.HasExited){$p.Kill();$p.WaitForExit()};$p.Dispose()}
    foreach($file in @($data,"$data.lock","$data.stage",$genesisPath)){if(Test-Path -LiteralPath $file){Remove-Item -LiteralPath $file}}
    Remove-Item -LiteralPath $directory
    $stopEvent.Dispose();$sha.Dispose();$env:STN_PHASE9_STOP_EVENT=$previousEvent
}
