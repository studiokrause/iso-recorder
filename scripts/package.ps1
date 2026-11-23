# Buduje program i sklada samodzielna paczke w dist\IsoRecorder\ (zastepuje poprzednia; kod wyjscia 2 = program
# dziala z poprzedniej paczki, nowa czeka w dist\IsoRecorder.new\).
# Paczka zawiera EXE, biblioteki Qt i caly GStreamer - uruchamia sie dwuklikiem, bez instalowania czegokolwiek.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$final = "$root\dist\IsoRecorder"
$dist = "$root\dist\IsoRecorder.new"     # paczka powstaje obok i dopiero gotowa zastepuje poprzednia

& "$PSScriptRoot\build.ps1"
if ($LASTEXITCODE) { throw 'Budowanie nie powiodlo sie.' }

$qt = Get-ChildItem C:\Qt -Directory | ForEach-Object { Get-ChildItem $_.FullName -Directory -Filter 'msvc*_64' } |
    Sort-Object FullName | Select-Object -Last 1 -ExpandProperty FullName
$gst = $env:GSTREAMER_1_0_ROOT_MSVC_X86_64
if (-not $gst) { $gst = [Environment]::GetEnvironmentVariable('GSTREAMER_1_0_ROOT_MSVC_X86_64', 'Machine') }
if (-not $gst) { $gst = [Environment]::GetEnvironmentVariable('GSTREAMER_1_0_ROOT_MSVC_X86_64', 'User') }
if (-not $gst) { $gst = "$env:LOCALAPPDATA\Programs\gstreamer\1.0\msvc_x86_64" }
$gst = $gst.TrimEnd('\')
if (-not (Test-Path "$gst\bin\gstreamer-1.0-0.dll")) { throw "Nie znaleziono GStreamera w $gst" }

if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
New-Item -ItemType Directory -Force $dist, "$dist\gstreamer-1.0", "$dist\gio\modules" | Out-Null

Copy-Item "$root\build\IsoRecorder.exe" $dist
& "$qt\bin\windeployqt.exe" --release --no-translations --no-system-d3d-compiler --no-opengl-sw --compiler-runtime "$dist\IsoRecorder.exe" | Out-Null
if ($LASTEXITCODE) { throw 'windeployqt nie powiodl sie.' }

Copy-Item "$gst\bin\*.dll" $dist
Copy-Item "$gst\lib\gstreamer-1.0\*.dll" "$dist\gstreamer-1.0"
$scanner = Get-ChildItem $gst -Recurse -Filter 'gst-plugin-scanner.exe' | Select-Object -First 1
if ($scanner) { Copy-Item $scanner.FullName $dist }
if (Test-Path "$gst\lib\gio\modules") { Copy-Item "$gst\lib\gio\modules\*.dll" "$dist\gio\modules" }

# Paczka ma zawierac tylko skladniki na licencjach LGPL / BSD / MIT. Wtyczki GPL z instalacji GStreamera nie sa programowi
# potrzebne (enkodery: NVENC, Quick Sync, AMF, Media Foundation, OpenH264), wiec nie trafiaja do paczki.
$gpl = 'gsta52dec.dll', 'gstdtsdec.dll', 'gstdvdread.dll', 'gstresindvd.dll', 'gstx264.dll', 'gstx265.dll'
foreach ($f in $gpl) { Remove-Item "$dist\gstreamer-1.0\$f" -ErrorAction SilentlyContinue }
foreach ($f in 'x264-*.dll', 'x265.dll', 'dca-*.dll', 'dvdread-*.dll', 'dvdnav-*.dll', 'a52-*.dll') { Remove-Item "$dist\$f" -ErrorAction SilentlyContinue }

# Zdalne sterowanie: dokumentacja i modul Bitfocus Companion (paczka powstaje poleceniem npm run package w companion\isorecorder)
New-Item -ItemType Directory -Force "$dist\docs", "$dist\companion" | Out-Null
Copy-Item "$root\docs\Remote-Control.md" "$dist\docs"
$module = Get-ChildItem "$root\companion\isorecorder\isorecorder-*.tgz" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
if ($module) { Copy-Item $module.FullName "$dist\companion" } else { Write-Warning 'Brak companion\isorecorder\isorecorder-*.tgz - uruchom npm run package w tym katalogu.' }

$version = (Select-String -Path "$root\CMakeLists.txt" -Pattern 'APP_VERSION_SHORT "([^"]+)"').Matches[0].Groups[1].Value
Set-Content "$dist\WERSJA.txt" "ISO Recorder $version`r`nZbudowano: $(Get-Date -Format 'yyyy-MM-dd HH:mm')" -Encoding utf8
$size = (Get-ChildItem $dist -Recurse -File | Measure-Object Length -Sum).Sum

# Podmiana poprzedniej paczki. Gdy program z niej wlasnie dziala, niczego nie ruszamy - nowa paczka zostaje obok.
$running = @(Get-Process IsoRecorder -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path.StartsWith($final, [StringComparison]::OrdinalIgnoreCase) })
if ($running.Count) {
    "Paczka: $dist  (wersja $version, $([math]::Round($size / 1MB)) MB)"
    "UWAGA: program dziala z $final - poprzednia paczka nie zostala podmieniona. Zamknij program i uruchom skrypt ponownie."
    exit 2
}
if (Test-Path $final) {
    # Czyscimy zawartosc, nie sam katalog - bywa otwarty w Eksploratorze i wtedy nie da sie go usunac.
    Get-ChildItem $final -Force | Remove-Item -Recurse -Force
} else {
    New-Item -ItemType Directory -Force $final | Out-Null
}
Get-ChildItem $dist -Force | Move-Item -Destination $final
Remove-Item $dist -Recurse -Force
"Paczka: $final  (wersja $version, $([math]::Round($size / 1MB)) MB)"
