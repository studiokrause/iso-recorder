# Test sterowania przez OSC (UDP): program z 4 kanalami testowymi, polecenia wysylane prawdziwymi pakietami UDP,
# stan odczytywany z informacji zwrotnej (odpowiedz na adres nadawcy).
#   A. samokontrola programu (kodek OSC, mapa MIDI, polecenia)  B. rec / pause / split / stop kanalu
#   C. zaznaczanie: select, select/all, select/invert, select/none  D. rec / pause / stop zaznaczonych
#   E. widok 2x2 / 3x3 / 4x4 / list w informacji zwrotnej  F. /isorec/status odsyla pelny stan
#   G. pakiet #bundle  H. nieznana sciezka nic nie psuje
# Uzycie:  tests\osc_test.ps1 [-Exe sciezka] [-Port 18001]
param([string]$Exe = '', [int]$Port = 18001)
$ErrorActionPreference = 'Continue'
$root = Split-Path $PSScriptRoot
if (-not $Exe) { $Exe = "$root\build\IsoRecorder.exe" }
$gstBin = "$env:LOCALAPPDATA\Programs\gstreamer\1.0\msvc_x86_64\bin"
$saved = $env:PATH
$errors = 0
function Check([string]$what, [bool]$ok, [string]$got = '') { if ($ok) { "ok    $what  $got" } else { "BLAD  $what  $got"; $script:errors++ } }

function Pad([System.Collections.Generic.List[byte]]$b) { while ($b.Count % 4) { $b.Add(0) } }
function OscStr([System.Collections.Generic.List[byte]]$b, [string]$s) { $b.AddRange([Text.Encoding]::UTF8.GetBytes($s)); $b.Add(0); Pad $b }
function OscMsg([string]$path, $arg = $null) {
    $b = New-Object 'System.Collections.Generic.List[byte]'
    OscStr $b $path
    if ($null -eq $arg) { OscStr $b ',' }
    elseif ($arg -is [int]) { OscStr $b ',i'; $b.AddRange([byte[]]([BitConverter]::GetBytes([int]$arg)[3..0])) }
    else { OscStr $b ',s'; OscStr $b ([string]$arg) }
    , $b.ToArray()
}
function OscDecode([byte[]]$d) {      # zwraca @{path; value}
    $i = 0
    $end = [Array]::IndexOf($d, [byte]0, $i); $path = [Text.Encoding]::UTF8.GetString($d, 0, $end); $i = ($end + 4) -band -4
    $end = [Array]::IndexOf($d, [byte]0, $i); $tags = [Text.Encoding]::UTF8.GetString($d, $i, $end - $i); $i = ($end + 4) -band -4
    $v = $null
    if ($tags -eq ',i') { $v = [int](([int]$d[$i] -shl 24) -bor ($d[$i + 1] -shl 16) -bor ($d[$i + 2] -shl 8) -bor $d[$i + 3]) }
    elseif ($tags -eq ',s') { $e = [Array]::IndexOf($d, [byte]0, $i); $v = [Text.Encoding]::UTF8.GetString($d, $i, $e - $i) }
    elseif ($tags -eq ',f') { $v = [BitConverter]::ToSingle(@($d[$i + 3], $d[$i + 2], $d[$i + 1], $d[$i]), 0) }
    @{ path = $path; value = $v }
}

# ---- A. samokontrola
$env:PATH = "C:\Qt\6.10.3\msvc2022_64\bin;$gstBin;$saved"
$env:ISOREC_PROFILE = 'osctest'
$out = "$env:TEMP\osc_self.txt"
$p = Start-Process -FilePath $Exe -ArgumentList '--demo', '4', '--remote-selftest' -RedirectStandardOutput $out -PassThru -Wait
Check 'A: samokontrola programu (kodek OSC, mapa MIDI, polecenia)' ($p.ExitCode -eq 0) ((Get-Content $out | Select-String 'BLAD') -join '; ')

# ---- uruchomienie programu z OSC
$rec = "$env:TEMP\osc_rec"
Remove-Item $rec -Recurse -Force -ErrorAction SilentlyContinue
$app = Start-Process -FilePath $Exe -ArgumentList '--demo', '4', '--osc-port', $Port, '--out', "`"$rec`"", '--no-splash' -PassThru
$udp = New-Object Net.Sockets.UdpClient
$udp.Client.ReceiveTimeout = 200
$state = @{}
function Pump { try { while ($true) { $ep = New-Object Net.IPEndPoint ([Net.IPAddress]::Any, 0); $d = $udp.Receive([ref]$ep); $m = OscDecode $d; $script:state[$m.path] = $m.value } } catch {} }
function Send($path, $arg = $null) { $b = OscMsg $path $arg; [void]$udp.Send($b, $b.Length, '127.0.0.1', $Port) }
function WaitFor($path, $want, [int]$sec = 8) {
    $t = [Diagnostics.Stopwatch]::StartNew()
    while ($t.Elapsed.TotalSeconds -lt $sec) { Pump; if ("$($state[$path])" -eq "$want") { return $true } }
    $false
}
try {
    Start-Sleep -Seconds 4
    Send '/isorec/status'
    Check 'F: /isorec/status zwraca pelny stan (wersja i liczba kanalow)' ((WaitFor '/isorec/channels' 4) -and $state['/isorec/version'] -eq '3.5') "wersja=$($state['/isorec/version']) kanalow=$($state['/isorec/channels'])"
    Check 'F: nazwa i stan kanalu 1' ($state['/isorec/ch/1/name'] -like 'TEST*' -and $state['/isorec/ch/1/state'] -eq 'PREVIEW') "$($state['/isorec/ch/1/name']) / $($state['/isorec/ch/1/state'])"

    # B. kanal
    Send '/isorec/ch/1/rec'
    Check 'B: ch/1/rec -> REC' (WaitFor '/isorec/ch/1/state' 'REC') $state['/isorec/ch/1/state']
    Check 'B: tylko kanal 1 nagrywa' ($state['/isorec/ch/2/state'] -eq 'PREVIEW' -and $state['/isorec/rec/count'] -eq 1)
    Start-Sleep -Seconds 2; Pump
    Check 'B: licznik czasu kanalu 1 idzie' ($state['/isorec/ch/1/time'] -match '^00:00:0[1-9]$') $state['/isorec/ch/1/time']
    Send '/isorec/ch/1/pause'
    Check 'B: ch/1/pause -> PAUSED' (WaitFor '/isorec/ch/1/state' 'PAUSED') $state['/isorec/ch/1/state']
    Send '/isorec/ch/1/pause'
    Check 'B: ponowne pause wznawia' (WaitFor '/isorec/ch/1/state' 'REC') $state['/isorec/ch/1/state']
    Send '/isorec/ch/1/split'
    Start-Sleep -Seconds 3
    Send '/isorec/ch/1/stop'
    Check 'B: ch/1/stop -> PREVIEW' (WaitFor '/isorec/ch/1/state' 'PREVIEW' 12) $state['/isorec/ch/1/state']
    $files = @(Get-ChildItem $rec -Recurse -Filter *.mkv -ErrorAction SilentlyContinue)
    Check 'B: split dal dwa pliki kanalu 1' ($files.Count -eq 2) "plikow=$($files.Count)"

    # C. zaznaczanie
    Send '/isorec/select/none'
    Check 'C: select/none' (WaitFor '/isorec/selected/count' 0)
    Send '/isorec/ch/3/select'
    Check 'C: ch/3/select zaznacza' ((WaitFor '/isorec/ch/3/selected' 1) -and $state['/isorec/selected/count'] -eq 1)
    Send '/isorec/ch/3/select' 0
    Start-Sleep -Milliseconds 800; Pump
    Check 'C: argument 0 (zwolnienie przycisku) jest ignorowany' ($state['/isorec/ch/3/selected'] -eq 1)
    Send '/isorec/ch/3/select'
    Check 'C: ch/3/select ponownie odznacza' (WaitFor '/isorec/ch/3/selected' 0)
    Send '/isorec/select/all'
    Check 'C: select/all' (WaitFor '/isorec/selected/count' 4)
    Send '/isorec/ch/2/select/off'
    Send '/isorec/select/invert'
    Check 'C: select/invert (po wylaczeniu kanalu 2 zostaje tylko on)' ((WaitFor '/isorec/selected/count' 1) -and $state['/isorec/ch/2/selected'] -eq 1)

    # D. zaznaczone
    Send '/isorec/select/all'
    WaitFor '/isorec/selected/count' 4 | Out-Null
    Send '/isorec/selected/rec'
    Check 'D: selected/rec - wszystkie 4 nagrywaja' (WaitFor '/isorec/rec/count' 4 10) "rec=$($state['/isorec/rec/count'])"
    Send '/isorec/selected/pause'
    Check 'D: selected/pause - wszystkie w pauzie' ((WaitFor '/isorec/ch/4/state' 'PAUSED') -and $state['/isorec/ch/1/state'] -eq 'PAUSED')
    Send '/isorec/selected/pause'
    WaitFor '/isorec/ch/4/state' 'REC' | Out-Null
    Send '/isorec/ch/2/select/off'
    Send '/isorec/selected/stop'
    Check 'D: selected/stop zatrzymuje zaznaczone, ale nie kanal 2' ((WaitFor '/isorec/ch/1/state' 'PREVIEW' 12) -and $state['/isorec/ch/2/state'] -in 'REC') "ch2=$($state['/isorec/ch/2/state'])"
    Send '/isorec/ch/2/stop'
    WaitFor '/isorec/rec/count' 0 12 | Out-Null

    # E. widok
    foreach ($v in '2x2', '3x3', '4x4', 'list') {
        Send "/isorec/view/$v"
        Check "E: view/$v w informacji zwrotnej" (WaitFor '/isorec/view' $v) $state['/isorec/view']
    }
    Send '/isorec/view' 'list'
    Send '/isorec/view' 3
    Check 'E: /isorec/view z argumentem liczbowym' (WaitFor '/isorec/view' '3x3')

    # G. bundle
    $a = OscMsg '/isorec/select/none'; $b2 = OscMsg '/isorec/ch/2/select'
    $bun = New-Object 'System.Collections.Generic.List[byte]'
    $bun.AddRange([Text.Encoding]::ASCII.GetBytes('#bundle')); $bun.Add(0); $bun.AddRange([byte[]](0, 0, 0, 0, 0, 0, 0, 1))
    foreach ($e in $a, $b2) { $bun.AddRange([byte[]](0, 0, 0, [byte]$e.Length)); $bun.AddRange($e) }
    $arr = $bun.ToArray(); [void]$udp.Send($arr, $arr.Length, '127.0.0.1', $Port)
    Check 'G: pakiet #bundle (none + ch/2/select) wykonany' ((WaitFor '/isorec/ch/2/selected' 1) -and $state['/isorec/selected/count'] -eq 1)

    # H. smieci
    Send '/isorec/ch/99/rec'; Send '/isorec/nonsens'; Send '/inne/ch/1/rec'
    [void]$udp.Send([byte[]](1, 2, 3), 3, '127.0.0.1', $Port)
    Start-Sleep -Milliseconds 800; Pump
    Send '/isorec/status'
    Check 'H: nieznane sciezki i smieci nic nie psuja' ((WaitFor '/isorec/channels' 4) -and -not $app.HasExited -and $state['/isorec/ch/1/state'] -eq 'PREVIEW')
} catch { Check "wyjatek w ttescie" $false "$_" } finally {
    if (-not $app.HasExited) { $app.Kill() }
    $udp.Close()
    $env:PATH = $saved; $env:ISOREC_PROFILE = $null
}
"Bledy: $errors"
exit $errors
