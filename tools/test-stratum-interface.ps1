# Actual current Stratum executable and its linked production STNC client.
param([string]$StratumDirectory='C:\poes_projects\stn-stratum',[switch]$JobMappingOnly,[switch]$MinerResultOnly,[switch]$FailureStateOnly,[switch]$FullLifecycleOnly)
. "$PSScriptRoot\test-node.ps1" -LibraryOnly
$script:processes=@();$script:clients=@()
$directory=Join-Path ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($Executable))) ('stratum-interface-'+[Guid]::NewGuid().ToString('N'))
$null=New-Item -ItemType Directory -Path $directory
$data=Join-Path $directory 'chain.stns';$genesisPath=Join-Path $directory 'genesis.block'
$log=Join-Path $directory 'logs\stn-stratum.log'
function StartStratumProcess([string]$name,[string]$arguments='') {
    $info=[Diagnostics.ProcessStartInfo]::new();$info.FileName=Join-Path $StratumDirectory "build\$name"
    $info.Arguments=$arguments;$info.WorkingDirectory=$directory;$info.UseShellExecute=$false;$info.CreateNoWindow=$true
    $info.RedirectStandardInput=$true;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
    $p=[Diagnostics.Process]::Start($info);$script:processes+=$p;return $p
}
function DriverResult {
    $line=$driver.StandardOutput.ReadLineAsync();Check ($line.Wait(15000)) 'Stratum client response deadline'
    Check ($line.Result -match '^RESULT (\d+) (\d+) ([0-9A-F]*)$') 'Stratum driver result'
    $code=[int]$Matches[1];$length=[int]$Matches[2];$hex=$Matches[3];$payload=[byte[]]::new($length)
    Check ($hex.Length -eq 2*$length) 'Stratum interpreted response length'
    for($i=0;$i -lt $length;$i++){$payload[$i]=[Convert]::ToByte($hex.Substring(2*$i,2),16)}
    return @{Code=$code;Payload=$payload}
}
function ClientCall([string]$command) {$driver.StandardInput.WriteLine($command);$driver.StandardInput.Flush();return DriverResult}
function WaitLog([string]$pattern,[int]$atLeast=1) {
    $deadline=[DateTime]::UtcNow.AddSeconds(15);$found=$false
    do {
        if($null -eq $script:serverReadTask){$script:serverReadTask=$server.StandardOutput.ReadLineAsync()}
        if($script:serverReadTask.Wait(100)){
            $script:serverOutput += $script:serverReadTask.Result + "`n"
            $script:serverReadTask=$null
        }
        $found=([regex]::Matches($script:serverOutput,$pattern)).Count -ge $atLeast
    } while(!$found -and [DateTime]::UtcNow -lt $deadline)
    Check $found "actual Stratum output: $pattern"
}
function Observer {
    $client=[Net.Sockets.TcpClient]::new('127.0.0.1',18475)
    $client.ReceiveTimeout=5000;$script:clients+=$client;return $client
}
function Job($observer,[byte[]]$template) {
    $stream=$observer.GetStream();$h=ReadExact $stream 84
    Check ([Text.Encoding]::ASCII.GetString($h,0,4) -eq 'STNM' -and $h[4] -eq 1 -and $h[5] -eq 1 -and $h[6] -eq 0 -and $h[7] -eq 0) 'job framing/version/reserved bytes'
    $n=[int](ReadNumber $h[72..75]);Check ($n -eq $template.Length-68) 'exact job block length'
    $block=ReadExact $stream $n
    Check ([Convert]::ToBase64String($block) -eq [Convert]::ToBase64String($template[68..($template.Length-1)])) 'all candidate bytes unchanged'
    Check ([Convert]::ToBase64String($h[8..39]) -eq [Convert]::ToBase64String($template[32..63])) 'job ID is Chain work ID'
    Check ([Convert]::ToBase64String($h[40..71]) -eq [Convert]::ToBase64String($template[188..219])) 'exact 256-bit target'
    Check ([Convert]::ToBase64String($h[76..83]) -eq [Convert]::ToBase64String($block[152..159]) -and (ReadNumber $h[76..83]) -eq 0) '64-bit initial nonce preserved'
    Check ([Convert]::ToBase64String($block[40..71]) -eq [Convert]::ToBase64String($template[0..31])) 'stale base preserved in candidate'
    Check ((ReadNumber $block[4..5]) -eq 3 -and (ReadNumber $block[72..79]) -eq (ReadNumber $template[140..147]) -and $block[8] -eq 1) 'candidate version height network preserved'
    return ,([byte[]]($h+$block))
}
function MinerResult($observer,[byte[]]$jobId,[UInt64]$nonce,[int]$expected,[int]$malformed=0) {
    $frame=[byte[]]::new(48);[Text.Encoding]::ASCII.GetBytes('STNM').CopyTo($frame,0)
    $frame[4]=1;$frame[5]=2;$jobId.CopyTo($frame,8);(NumberBytes $nonce 8).CopyTo($frame,40)
    if($malformed -eq 1){$frame[0]=0};if($malformed -eq 2){$frame[6]=1}
    $stream=$observer.GetStream();$stream.Write($frame,0,13);$stream.Write($frame,13,35)
    $r=ReadExact $stream 12
    Check ([Text.Encoding]::ASCII.GetString($r,0,4) -eq 'STNM' -and $r[4] -eq 1 -and $r[5] -eq 3) 'miner result response framing'
    $code=ReadNumber $r[8..11]
    if($expected -lt 0){Check ($code -eq 2 -or $code -eq 3) 'outage result is stale/provider, never accepted';return $code}
    Check ($code -eq $expected) "miner result classification $expected"
}
try {
    # Never interrupt another service to free these production-configured ports.
    foreach($port in @(18473,18475)){
        $probe=[Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,$port)
        try{$probe.Start();Check $true 'configured port available'}finally{$probe.Stop()}
    }
    $node=StartNode $data $false '' 18473
    $r=Request $node.Stream 2 (NumberBytes 0 8);Check ($r.Code -eq 0) 'genesis fixture obtained'
    [IO.File]::WriteAllBytes($genesisPath,$r.Payload)
    if($JobMappingOnly -or $MinerResultOnly -or $FailureStateOnly -or $FullLifecycleOnly){
        $node.Client.Close();$node.Process.Kill();$node.Process.WaitForExit()
        $previousEvent=$env:STN_PHASE9_STOP_EVENT
        $eventName='Local\stn-job-proof-'+[Guid]::NewGuid().ToString('N')
        $stopEvent=[Threading.EventWaitHandle]::new($false,[Threading.EventResetMode]::ManualReset,$eventName)
        $env:STN_PHASE9_STOP_EVENT=$eventName
        $Executable=Join-Path ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($Executable))) 'stn-chain-phase9-test.exe'
        $node=StartNode $data $false $genesisPath 18473
        $server=StartStratumProcess 'stn-stratum.exe'
        WaitLog 'connection still unavailable code=5'
        $a=Observer;$b=Observer
        Start-Sleep -Milliseconds 300
        Check (!$a.GetStream().DataAvailable -and !$b.GetStream().DataAvailable) 'no placeholder jobs with unavailable Chain work'
        if($FullLifecycleOnly){
            $r=Request $node.Stream 1 @();Check ($r.Code -eq 0 -and (ReadNumber $r.Payload[64..71]) -eq 0) 'initial accepted height'
            $initialTip=$r.Payload[72..103]
            $r=Request $node.Stream 0x2002 @();Check ($r.Code -eq 5) 'initial authoritative work unavailable'
        }
        $r=Request $node.Stream 0x1005 (Transaction 1);Check ($r.Code -eq 0 -and $r.Payload[3] -eq 0) 'first canonical content admitted'
        $r=Request $node.Stream 0x2002 @();Check ($r.Code -eq 0) 'authoritative template A'
        $templateA=$r.Payload;$jobA=Job $a $templateA;$jobA2=Job $b $templateA
        Check ([Convert]::ToBase64String($jobA) -eq [Convert]::ToBase64String($jobA2)) 'identical job across two sessions'
        if($FullLifecycleOnly){
            $sha=[Security.Cryptography.SHA256]::Create()
            Check ((Hex $templateA[0..31]) -eq (Hex $initialTip) -and (ReadNumber $templateA[140..147]) -eq 1) 'first work derives from accepted genesis tip'
            $bad=Solve $templateA $false
            MinerResult $a $jobA[8..39] (ReadNumber $bad[220..227]) 1
            WaitLog 'Chain result=7 response=0'
            $r=Request $node.Stream 1 @();Check ($r.Code -eq 0 -and (ReadNumber $r.Payload[64..71]) -eq 0 -and (Hex $r.Payload[72..103]) -eq (Hex $initialTip)) 'forwarded invalid evidence cannot change accepted state'
            $r=Request $node.Stream 0x2002 @();Check ($r.Code -eq 0 -and (Hex $r.Payload) -eq (Hex $templateA) -and !$b.GetStream().DataAvailable) 'rejected session result preserves other session current work'
            $solved=Solve $templateA $true;$nonce=ReadNumber $solved[220..227]
            MinerResult $b $jobA2[8..39] $nonce 0
            WaitLog 'Chain result=0 response=72'
            $acceptedBlock=$solved[68..($solved.Length-1)]
            $r=Request $node.Stream 2 (NumberBytes 1 8)
            Check ($r.Code -eq 0 -and (Hex $r.Payload) -eq (Hex $acceptedBlock)) 'accepted block exactly original candidate plus returned nonce'
            Check ((ReadNumber $r.Payload[152..159]) -eq $nonce -and $nonce -ge 4294967296 -and (Hex $r.Payload[120..151]) -eq (Hex $jobA[40..71])) 'accepted full-width nonce and full target preserved'
            $r=Request $node.Stream 1 @();Check ($r.Code -eq 0 -and (ReadNumber $r.Payload[64..71]) -eq 1) 'Chain alone reports acceptance'
            $acceptedInfo=$r.Payload
            Check ((Hex $acceptedInfo[72..103]) -eq (Hex (Digest 'STN-CHAIN:BLOCK:ID:1' $acceptedBlock[0..167]))) 'accepted tip is canonical solved header identity'
            $r=Request $node.Stream 0x1004 @();Check ($r.Code -eq 0 -and (ReadNumber $r.Payload[0..3]) -eq 0) 'accepted pending content cleaned'
            MinerResult $a $jobA[8..39] $nonce 2
            $r=Request $node.Stream 0x2002 @();Check ($r.Code -eq 5) 'accepted job no longer eligible'
            # Exactly one lifecycle interruption: stop both, recover Chain alone,
            # then start a fresh coordinator with no retained session/work state.
            $server.Kill();$server.WaitForExit();$a.Close();$b.Close()
            $node.Client.Close();$null=$stopEvent.Set()
            Check ($node.Process.WaitForExit(10000) -and $node.Process.ExitCode -eq 0) 'accepted Chain clean persistence shutdown'
            $null=$stopEvent.Reset();$node=StartNode $data $false $genesisPath 18473
            Check ($server.HasExited -and $node.Height -eq 1) 'accepted state recovered while coordinator absent'
            $r=Request $node.Stream 1 @();Check ($r.Code -eq 0 -and (Hex $r.Payload) -eq (Hex $acceptedInfo)) 'complete INFO state reconstructed independently and exactly'
            $r=Request $node.Stream 2 (NumberBytes 1 8);Check ($r.Code -eq 0 -and (Hex $r.Payload) -eq (Hex $acceptedBlock)) 'persisted accepted bytes recovered exactly without Stratum'
            $r=Request $node.Stream 0x2002 @();Check ($r.Code -eq 5) 'recovery does not revive accepted work'
            $script:serverReadTask=$null;$script:serverOutput=''
            $server=StartStratumProcess 'stn-stratum.exe';WaitLog 'connection still unavailable code=5'
            $c=Observer;$d=Observer;Start-Sleep -Milliseconds 300
            Check (!$c.GetStream().DataAvailable -and !$d.GetStream().DataAvailable) 'fresh coordinator sends no placeholder or cached current job'
            MinerResult $c $jobA[8..39] $nonce 2
            $r=Request $node.Stream 0x1005 (Transaction 2);Check ($r.Code -eq 0 -and $r.Payload[3] -eq 0) 'new eligible content after recovery'
            $r=Request $node.Stream 0x2002 @();Check ($r.Code -eq 0) 'new authoritative work retrieved after recovery'
            $templateB=$r.Payload;$jobC=Job $c $templateB;$jobD=Job $d $templateB
            Check ((Hex $templateB[0..31]) -eq (Hex $acceptedInfo[72..103]) -and (ReadNumber $templateB[140..147]) -eq 2) 'new work extends recovered accepted tip at height two'
            Check ((Hex $jobC) -eq (Hex $jobD) -and (Hex $jobC[8..39]) -ne (Hex $jobA[8..39])) 'fresh work identity deterministic across recovered sessions'
            MinerResult $c $jobA[8..39] $nonce 2
            $r=Request $node.Stream 0x2002 @();Check ($r.Code -eq 0 -and (Hex $r.Payload) -eq (Hex $templateB) -and !$d.GetStream().DataAvailable) 'stale session cannot replace other session authoritative work'
            $r=Request $node.Stream 1 @();Check ($r.Code -eq 0 -and (Hex $r.Payload[64..135]) -eq (Hex $acceptedInfo[64..135])) 'recovery and stale results preserve height tip and cumulative work'
            $node.Client.Close();$null=$stopEvent.Set()
            Check ($node.Process.WaitForExit(10000) -and $node.Process.ExitCode -eq 0) 'final lifecycle teardown'
            Write-Output "Full Chain/Stratum lifecycle: $script:checks checks, 0 failures; exact persistence, independent recovery, one interruption cycle."
            return
        }
        if($FailureStateOnly){
            # Disconnect with a prefix buffered in only session A.
            $partial=[byte[]]::new(48);[Text.Encoding]::ASCII.GetBytes('STNM').CopyTo($partial,0)
            $partial[4]=1;$partial[5]=2;$jobA[8..39].CopyTo($partial,8)
            $a.GetStream().Write($partial,0,17);$a.Close()
            $r=Request $node.Stream 1 @();Check ($r.Code -eq 0 -and (ReadNumber $r.Payload[64..71]) -eq 0) 'partial miner frame is not an accepted result'
            $r=Request $node.Stream 0x2002 @();Check ((Hex $r.Payload) -eq (Hex $templateA)) 'one interrupted session preserves shared candidate'
            $c=Observer;$jobC=Job $c $templateA
            Check ((Hex $jobC) -eq (Hex $jobA)) 'session reconnect has exact authoritative identity'
            # Kill only this test Chain, then exercise a real result boundary.
            $node.Client.Close();$node.Process.Kill();$node.Process.WaitForExit()
            $outageCode=MinerResult $c $jobC[8..39] 0 -1
            if($outageCode -eq 3){WaitLog 'Chain result=11 response=0'}
            else{WaitLog 'connection unavailable code=11'}
            $d=Observer;Start-Sleep -Milliseconds 400
            Check (!$d.GetStream().DataAvailable -and !$b.GetStream().DataAvailable) 'known Chain failure does not advertise cached or fabricated jobs'
            $node=StartNode $data $false $genesisPath 18473
            $r=Request $node.Stream 1 @();Check ($r.Code -eq 0 -and (ReadNumber $r.Payload[64..71]) -eq 0) 'Chain reconnect restores actual INFO'
            $r=Request $node.Stream 0x2002 @();Check ($r.Code -eq 5) 'restarted Chain has no volatile pending work'
            Start-Sleep -Milliseconds 1300
            Check (!$d.GetStream().DataAvailable -and !$b.GetStream().DataAvailable) 'unavailable across reconnect holds no current job'
            MinerResult $c $jobA[8..39] 0 2
            $r=Request $node.Stream 0x1005 (Transaction 1);Check ($r.Code -eq 0 -and $r.Payload[3] -eq 0) 'fresh admission restores prior eligible content'
            $r=Request $node.Stream 0x2002 @();Check ($r.Code -eq 0 -and (Hex $r.Payload) -eq (Hex $templateA)) 'unchanged authoritative inputs restore exact template'
            $recoveredB=Job $b $r.Payload;$recoveredC=Job $c $r.Payload;$recoveredD=Job $d $r.Payload
            Check ((Hex $recoveredB) -eq (Hex $jobA) -and (Hex $recoveredC) -eq (Hex $recoveredD)) 'recovery does not regenerate transport-based identity'
            $c.Close()
            $r=Request $node.Stream 0x1005 (Transaction 2);Check ($r.Code -eq 0 -and $r.Payload[3] -eq 0) 'Chain work changes during miner disconnect'
            $r=Request $node.Stream 0x2002 @();Check ($r.Code -eq 0) 'replacement from current Chain'
            $templateB=$r.Payload;$jobB=Job $b $templateB;$jobD=Job $d $templateB
            $e=Observer;$jobE=Job $e $templateB
            Check ((Hex $jobB) -eq (Hex $jobD) -and (Hex $jobE) -eq (Hex $jobB) -and (Hex $jobB[8..39]) -ne (Hex $jobA[8..39])) 'reconnected miner receives changed job, other sessions remain consistent'
            MinerResult $e $jobA[8..39] 0 2
            # Restart actual Stratum while Chain and its current work remain.
            $server.Kill();$server.WaitForExit()
            $one=[byte[]]::new(1);$closed=$false
            try{$closed=$b.GetStream().Read($one,0,1) -eq 0}catch{
                $cause=$_.Exception.GetBaseException()
                if($cause -is [Net.Sockets.SocketException] -and $cause.SocketErrorCode -eq [Net.Sockets.SocketError]::ConnectionReset){$closed=$true}else{throw}
            }
            Check $closed 'Stratum loss closes old session path'
            $script:serverReadTask=$null;$script:serverOutput=''
            $server=StartStratumProcess 'stn-stratum.exe';WaitLog 'CHAIN RPC connected'
            $f=Observer;$jobF=Job $f $templateB
            Check ((Hex $jobF) -eq (Hex $jobB)) 'Stratum restart fetches exact current Chain work'
            MinerResult $f $jobA[8..39] 0 2
            $r=Request $node.Stream 1 @();Check ($r.Code -eq 0 -and (ReadNumber $r.Payload[64..71]) -eq 0) 'interruptions and stale results cause no false acceptance'
            $r=Request $node.Stream 0x1004 @();Check ($r.Code -eq 0 -and (ReadNumber $r.Payload[0..3]) -eq 2 -and (ReadNumber $r.Payload[8..11]) -eq 488) 'pending state preserved throughout failure proof'
            $node.Client.Close();$null=$stopEvent.Set();Check ($node.Process.WaitForExit(10000) -and $node.Process.ExitCode -eq 0) 'final Chain clean stop'
            Write-Output "Reconnect/failure states: $script:checks checks, 0 failures; outage result=$outageCode."
            return
        }
        if($MinerResultOnly){
            $sha=[Security.Cryptography.SHA256]::Create()
            $bad=Solve $templateA $false
            MinerResult $a $jobA[8..39] (ReadNumber $bad[220..227]) 1
            WaitLog 'Chain result=7 response=0'
            $unknown=[byte[]]$jobA[8..39];$unknown[0]=$unknown[0] -bxor 1
            MinerResult $a $unknown 0 2
            MinerResult $b $jobA[8..39] 0 4 1
            MinerResult $b $jobA[8..39] 0 4 2
            $r=Request $node.Stream 1 @();Check ($r.Code -eq 0 -and (ReadNumber $r.Payload[64..71]) -eq 0) 'invalid, unknown and malformed results do not activate blocks'
            $r=Request $node.Stream 0x2002 @();Check ((Hex $r.Payload) -eq (Hex $templateA)) 'session A results cannot mutate session B candidate'
            $r=Request $node.Stream 0x1005 (Transaction 2);Check ($r.Code -eq 0 -and $r.Payload[3] -eq 0) 'authoritative work replacement'
            $r=Request $node.Stream 0x2002 @();Check ($r.Code -eq 0) 'template B issued'
            $templateB=$r.Payload;$jobB=Job $a $templateB;$jobB2=Job $b $templateB
            Check ((Hex $jobB) -eq (Hex $jobB2) -and (Hex $jobA[8..39]) -ne (Hex $jobB[8..39])) 'current work identity changes consistently'
            MinerResult $a $jobA[8..39] 0 2
            $a.Close();$c=Observer;$jobC=Job $c $templateB
            Check ((Hex $jobC) -eq (Hex $jobB)) 'reconnected session receives only current authoritative job'
            MinerResult $c $jobA[8..39] 0 2
            $solved=Solve $templateB $true;$nonce=ReadNumber $solved[220..227]
            MinerResult $c $jobC[8..39] $nonce 0
            WaitLog 'Chain result=0 response=72'
            $r=Request $node.Stream 2 (NumberBytes 1 8)
            Check ($r.Code -eq 0 -and (Hex $r.Payload) -eq (Hex $solved[68..($solved.Length-1)])) 'persisted Chain block equals exact job plus permitted nonce'
            Check ((ReadNumber $r.Payload[152..159]) -eq $nonce -and $nonce -ge 4294967296) 'full-width nonce round trip and big-endian preservation'
            Check ((Hex $r.Payload[120..151]) -eq (Hex $jobB[40..71])) 'full target unchanged through accepted return path'
            $r=Request $node.Stream 0x1004 @();Check ($r.Code -eq 0 -and (ReadNumber $r.Payload[0..3]) -eq 0 -and (ReadNumber $r.Payload[8..11]) -eq 0) 'accepted two-item cleanup only after Chain success'
            MinerResult $b $jobB2[8..39] $nonce 2
            $r=Request $node.Stream 0x2002 @();Check ($r.Code -eq 5) 'no eligible work after acceptance'
            $node.Client.Close();$null=$stopEvent.Set()
            Check ($node.Process.WaitForExit(10000) -and $node.Process.ExitCode -eq 0) 'accepted Chain clean stop'
            $null=$stopEvent.Reset();$node=StartNode $data $false $genesisPath 18473
            Check ($node.Height -eq 1) 'accepted result survives Chain reconnect'
            $d=Observer;Start-Sleep -Milliseconds 400
            Check (!$d.GetStream().DataAvailable) 'no cached job made current on reconnect'
            MinerResult $d $jobA[8..39] 0 2
            $node.Client.Close();$null=$stopEvent.Set()
            Check ($node.Process.WaitForExit(10000) -and $node.Process.ExitCode -eq 0) 'final result-path clean stop'
            Write-Output "Miner result/Stratum/Chain: $script:checks checks, 0 failures; actual STNM -> STNC -> Chain acceptance."
            return
        }
        $r=Request $node.Stream 0x2002 @();Check ([Convert]::ToBase64String($templateA) -eq [Convert]::ToBase64String($r.Payload)) 'unchanged Chain template'
        $c=Observer;$jobA3=Job $c $templateA
        Check ([Convert]::ToBase64String($jobA) -eq [Convert]::ToBase64String($jobA3)) 'repeat mapping independent of connection order'
        $r=Request $node.Stream 0x1005 (Transaction 2);Check ($r.Code -eq 0 -and $r.Payload[3] -eq 0) 'pending change without mining or shares'
        $r=Request $node.Stream 0x2002 @();Check ($r.Code -eq 0) 'authoritative template B'
        $templateB=$r.Payload;$jobB=Job $a $templateB;$jobB2=Job $b $templateB;$jobB3=Job $c $templateB
        Check ([Convert]::ToBase64String($jobB[8..39]) -ne [Convert]::ToBase64String($jobA[8..39])) 'replacement has distinct Chain work identity'
        Check ([Convert]::ToBase64String($jobB) -eq [Convert]::ToBase64String($jobB2) -and
            [Convert]::ToBase64String($jobB) -eq [Convert]::ToBase64String($jobB3)) 'replacement consistent across sessions'
        $r=Request $node.Stream 0x1004 @();Check ($r.Code -eq 0 -and (ReadNumber $r.Payload[0..3]) -eq 2 -and (ReadNumber $r.Payload[8..11]) -eq 488) 'job mapping does not consume pending'
        $node.Client.Close();$null=$stopEvent.Set()
        Check ($node.Process.WaitForExit(10000) -and $node.Process.ExitCode -eq 0) 'test Chain clean shutdown'
        $null=$stopEvent.Reset();$node=StartNode $data $false $genesisPath 18473
        WaitLog 'connection unavailable code=5'
        $d=Observer;Start-Sleep -Milliseconds 400
        Check (!$d.GetStream().DataAvailable -and !$a.GetStream().DataAvailable -and !$b.GetStream().DataAvailable -and !$c.GetStream().DataAvailable) 'unavailable does not advertise cached job to old or new sessions'
        $r=Request $node.Stream 0x2002 @();Check ($r.Code -eq 5) 'Chain confirms unavailable after volatile pending reset'
        $r=Request $node.Stream 1 @();Check ($r.Code -eq 0 -and (ReadNumber $r.Payload[64..71]) -eq 0) 'no mining, shares, or block acceptance performed'
        $node.Client.Close();$null=$stopEvent.Set();Check ($node.Process.WaitForExit(10000) -and $node.Process.ExitCode -eq 0) 'final clean shutdown'
        Write-Output "Deterministic Stratum job mapping: $script:checks checks, 0 failures; observation-only sessions, no shares."
        return
    }
    $server=StartStratumProcess 'stn-stratum.exe'
    WaitLog 'CHAIN RPC connected'
    $driver=StartStratumProcess 'test-chain-interface.exe' '18473'
    $r=ClientCall 'info';Check ($r.Code -eq 0 -and $r.Payload.Length -eq 176 -and (ReadNumber $r.Payload[64..71]) -eq 0) 'actual client INFO height/endian'
    $r=ClientCall 'template';Check ($r.Code -eq 0 -and $r.Payload.Length -eq 432) 'actual client mining template'
    $work=$r.Payload;$r=Request $node.Stream 0x2002 @()
    Check ([Convert]::ToBase64String($work) -eq [Convert]::ToBase64String($r.Payload)) 'same complete candidate/work identity'
    Check ((ReadNumber $work[220..227]) -eq 0 -and $work[188] -eq 127 -and (ReadNumber $work[140..147]) -eq 1) 'published target, height and nonce offsets'
    $r=ClientCall 'unknown';Check ($r.Code -eq 3) 'unsupported opcode clear METHOD'
    $r=ClientCall 'submit 1';Check ($r.Code -eq 7 -and $r.Payload.Length -eq 0) 'Chain rejects invalid solved work'
    $r=ClientCall 'submit 0';Check ($r.Code -eq 0 -and $r.Payload.Length -eq 72 -and (ReadNumber $r.Payload[32..39]) -eq 1) 'Chain accepts deterministic fixture via actual Stratum client'
    $r=ClientCall 'submit 0';Check ($r.Code -eq 10) 'actual Stratum maps STALE'
    $driver.StandardInput.WriteLine('info');$driver.StandardInput.Flush()
    $r=Request $node.Stream 1 @();Check ($r.Code -eq 0) 'additional STNC client responsive with Stratum connected'
    $r=DriverResult;Check ($r.Code -eq 0 -and (ReadNumber $r.Payload[64..71]) -eq 1) 'simultaneous Stratum INFO'
    $node.Client.Close();$node.Process.Kill();$node.Process.WaitForExit()
    $r=ClientCall 'info';Check ($r.Code -eq 11) 'configured endpoint outage returns transport failure'
    WaitLog 'connection unavailable code=11'
    $node=StartNode $data $false '' 18473
    $r=ClientCall 'info';Check ($r.Code -eq 0 -and (ReadNumber $r.Payload[64..71]) -eq 1) 'same Stratum client reconnects to restored endpoint'
    WaitLog 'CHAIN RPC connected' 2
    $r=ClientCall 'template';Check ($r.Code -eq 0) 'session resumes template retrieval'
    $node.Client.Close();$node.Process.Kill();$node.Process.WaitForExit()
    $node=StartNode $data $false $genesisPath 18473
    $r=ClientCall 'template';Check ($r.Code -eq 5 -and $r.Payload.Length -eq 0) 'actual client distinguishes no-current-work UNAVAILABLE'
    WaitLog 'connection unavailable code=5'
    $r=ClientCall 'info';Check ($r.Code -eq 0) 'INFO still works without mining content'
    # A lost mutation reply must not trigger blind replay. This bounded fault
    # fixture exercises the actual Stratum transport, not Chain acceptance.
    $infoPayload=$r.Payload;$server.Kill();$server.WaitForExit()
    $node.Client.Close();$node.Process.Kill();$node.Process.WaitForExit()
    $faultListener=[Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,18473)
    $faultListener.Start()
    try {
        $driver.StandardInput.WriteLine('info');$driver.StandardInput.Flush()
        $accept=$faultListener.AcceptTcpClientAsync();Check ($accept.Wait(5000)) 'fault fixture connected'
        $faultClient=$accept.Result;$faultClient.ReceiveTimeout=5000;$script:clients+=$faultClient
        $stream=$faultClient.GetStream();$header=ReadExact $stream 24
        $header[7]=2;(NumberBytes 176 4).CopyTo($header,20)
        $stream.Write($header,0,24);$stream.Write($infoPayload,0,176)
        $r=DriverResult;Check ($r.Code -eq 0) 'reconnected read before mutation fault'
        $driver.StandardInput.WriteLine('submit 0');$driver.StandardInput.Flush()
        $header=ReadExact $stream 24;$null=ReadExact $stream ([int](ReadNumber $header[20..23]))
        Check ((ReadNumber $header[8..9]) -eq 0x2003) 'one mutation received'
        $faultClient.Close();$r=DriverResult
        Check ($r.Code -eq 11 -and !$faultListener.Pending()) 'lost mutation response not retried'
    } finally {$faultListener.Stop()}
    $driver.StandardInput.WriteLine('quit');$driver.StandardInput.Flush()
    Check ($driver.WaitForExit(5000) -and $driver.ExitCode -eq 0) 'Stratum client clean close'
    Write-Output "Chain/actual Stratum STNC: $script:checks checks, 0 failures."
} finally {
    foreach($client in $script:clients){$client.Dispose()}
    foreach($p in $script:processes){if(!$p.HasExited){$p.Kill();$p.WaitForExit()};$p.Dispose()}
    foreach($file in @($data,"$data.lock","$data.stage",$genesisPath,$log)){if(Test-Path -LiteralPath $file){Remove-Item -LiteralPath $file}}
    $logs=Join-Path $directory 'logs';if(Test-Path -LiteralPath $logs){Remove-Item -LiteralPath $logs}
    Remove-Item -LiteralPath $directory
    if($null -ne $stopEvent){$stopEvent.Dispose();$env:STN_PHASE9_STOP_EVENT=$previousEvent}
    if($null -ne $sha){$sha.Dispose()}
}
