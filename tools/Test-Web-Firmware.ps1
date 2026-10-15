param([switch]$WithBuild,[string]$ProjectRoot=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Web-Firmware.Common.ps1')
$root=[IO.Path]::GetFullPath($ProjectRoot)
$docs=Join-Path $root 'docs'
$manifest=Get-Content -Raw (Join-Path $docs 'manifest.json') | ConvertFrom-Json
$info=Get-Content -Raw (Join-Path $docs 'firmware/build-info.json') | ConvertFrom-Json
$config=Get-Content -Raw (Join-Path $root 'tools/build-config.json') | ConvertFrom-Json
$version=Get-FirmwareVersion $root
if($manifest.name -ne 'Bunker 32' -or $manifest.version -ne $version -or $info.version -ne $version){throw 'Firmware-Versionen oder Projektname sind inkonsistent.'}
if($manifest.builds.Count -ne 1 -or $manifest.builds[0].chipFamily -ne 'ESP32' -or $info.chipFamily -ne 'ESP32'){throw 'Falsche Chipfamilie im Manifest.'}
if($info.fqbn -ne $config.fqbn -or $info.coreVersion -ne $config.platformVersion){throw 'Build-Konfiguration ist inkonsistent.'}
if($manifest.new_install_improv_wait_time -ne 0){throw 'Die Firmware unterstuetzt kein Improv; dessen Wartezeit muss 0 sein.'}
if($manifest.builds[0].parts.Count -ne 1){throw 'Erwartet wird genau ein zusammengefuehrtes Image.'}
$part=$manifest.builds[0].parts[0]
if($part.offset -ne 0 -or $part.offset -ne $info.mergedOffset){throw 'Falsche Basisadresse fuer die merged BIN.'}
# Reproduce project Pages URL resolution with ONLY docs/ exposed to the server.
$manifestUrl=[uri]'https://basti0772.github.io/bunker32/manifest.json'
$imageUrl=[uri]::new($manifestUrl,[string]$part.path)
if($imageUrl.Host -ne $manifestUrl.Host -or !$imageUrl.AbsolutePath.StartsWith('/bunker32/')){throw 'Firmware-Pfad verlaesst das GitHub-Pages-Projekt.'}
if($part.path -match '(^/|\.\.|:|\\)'){throw 'Firmware muss innerhalb von docs/ liegen.'}
$webBinary=Join-Path $docs $part.path
$repositoryBinary=Join-Path $root 'firmware/bunker32.bin'
foreach($path in @($webBinary,$repositoryBinary)){
    if(!(Test-Path -LiteralPath $path)){throw "Firmware-Datei fehlt: $path"}
    if((Get-Item $path).Length -ne $info.size -or $info.size -le 0){throw 'Firmware-Groesse stimmt nicht.'}
    if((Get-BinarySha256 $path) -ne $info.sha256){throw 'Firmware-Pruefsumme stimmt nicht.'}
}
$sourceHashes=Get-SourceHashes $root
if($sourceHashes.Count -ne @($info.sourceHashes.PSObject.Properties).Count){throw 'Die Quellcodedateien haben sich geaendert: Web-Firmware neu bauen.'}
foreach($entry in $sourceHashes.GetEnumerator()){
    $property=$info.sourceHashes.PSObject.Properties[$entry.Key]
    if(!$property -or $property.Value -ne $entry.Value){throw "Web-Firmware ist veraltet: $($entry.Key). tools/Build-Web-Firmware.ps1 ausfuehren."}
}
$bytes=[IO.File]::ReadAllBytes($webBinary)
if([Text.Encoding]::ASCII.GetString($bytes) -match '(?i)[a-z]:[\\/]+Users[\\/]+'){
    throw 'Die Firmware enthaelt einen lokalen Windows-Benutzerpfad; mit Prefix-Mapping neu bauen.'
}
$recorded=[IO.File]::ReadAllLines((Join-Path $docs 'firmware/flash_args'))
$imageLines=@($recorded | Where-Object { $_.Trim() -match '^(0x[0-9a-fA-F]+|\d+)\s+' })
if($imageLines.Count -ne $info.inputs.Count){throw 'Die Liste der Quell-Images ist unvollstaendig.'}
$flashOptions=($recorded | Where-Object {$_.Trim().StartsWith('--')}) -join ' '
foreach($option in @(@('mode',$info.flashMode),@('freq',$info.flashFrequency),@('size',$info.flashSize))){
    if($flashOptions -notmatch ('--flash-'+$option[0]+'\s+'+[regex]::Escape($option[1])+'(?:\s|$)')){throw 'Flash-Eigenschaften widersprechen flash_args.'}
}
$previousEnd=0L
foreach($imageEntry in ($info.inputs | Sort-Object offset)){
    $matching=@($imageLines | Where-Object {$_ -match ('\s+"?'+[regex]::Escape($imageEntry.file)+'"?\s*$')})
    if($matching.Count -ne 1){throw 'Image fehlt oder steht doppelt in flash_args.'}
    $address=($matching[0].Trim() -split '\s+')[0]
    $offset=if($address.StartsWith('0x')){[Convert]::ToInt64($address.Substring(2),16)}else{[long]$address}
    if($offset -ne $imageEntry.offset -or $offset -lt $previousEnd -or $imageEntry.size -le 0 -or $offset+$imageEntry.size -gt $bytes.Length){throw 'Image-Offset oder -Groesse ist ungueltig.'}
    for($i=$previousEnd;$i -lt $offset;$i++){if($bytes[$i] -ne 255){throw 'Die merged BIN hat keine gueltige FF-Auffuellung ab Adresse 0.'}}
    $sha=[Security.Cryptography.SHA256]::Create()
    try{$actual=[BitConverter]::ToString($sha.ComputeHash($bytes,[int]$offset,[int]$imageEntry.size)).Replace('-','').ToLowerInvariant()}
    finally{$sha.Dispose()}
    if($actual -ne $imageEntry.sha256){throw "Image liegt nicht am dokumentierten Flash-Offset: $($imageEntry.file)"}
    $previousEnd=$offset+$imageEntry.size
}
if($previousEnd -ne $bytes.Length){throw 'Unerwartete Daten nach dem letzten Image.'}
$partitions=@([IO.File]::ReadAllLines((Join-Path $docs 'firmware/partitions.csv')) | Where-Object {$_ -and !($_.Trim().StartsWith('#'))} |
    ForEach-Object {$columns=$_.Split(',');[pscustomobject]@{name=$columns[0].Trim();type=$columns[1].Trim();subtype=$columns[2].Trim();offset=[Convert]::ToInt64($columns[3].Trim().Substring(2),16);size=[Convert]::ToInt64($columns[4].Trim().Substring(2),16)}})
$application=@($info.inputs | Where-Object {$_.file.EndsWith('.ino.bin')})
$appPartition=@($partitions | Where-Object {$_.type -eq 'app' -and $_.subtype -eq 'ota_0'})
if($application.Count -ne 1 -or $appPartition.Count -ne 1 -or $application[0].offset -ne $appPartition[0].offset -or $application[0].size -gt $appPartition[0].size){throw 'Anwendung passt nicht zur erzeugten Partitionstabelle.'}
if($WithBuild){
    $build=Join-Path $root 'build'
    $actualLayout=Read-FlashLayout $build
    $properties=Read-BuildProperties (Join-Path $build 'build-properties.txt')
    if($properties['build.mcu'].ToUpperInvariant() -ne $manifest.builds[0].chipFamily){throw 'Chipfamilie stimmt nicht mit dem tatsaechlichen Build ueberein.'}
    if($actualLayout.parts.Count -ne $info.inputs.Count){throw 'Build-Image fehlt im Web-Paket.'}
    foreach($imageEntry in $actualLayout.parts){
        $entry=@($info.inputs | Where-Object {$_.file -eq $imageEntry.name -and $_.offset -eq $imageEntry.offset})
        if($entry.Count -ne 1 -or (Get-BinarySha256 $imageEntry.fullPath) -ne $entry[0].sha256){throw 'Web-Paket stimmt nicht mit den aktuellen Build-Images ueberein.'}
    }
}
$html=[IO.File]::ReadAllText((Join-Path $docs 'index.html'))
if($html -notmatch '<html lang="de"' -or $html -notmatch '<esp-web-install-button' -or $html -notmatch 'manifest="manifest.json"' -or $html -notmatch 'installer.js'){throw 'Installer-HTML ist unvollstaendig.'}
foreach($match in [regex]::Matches($html,'(?:src|href)="([^"#]+)"')){
    $reference=$match.Groups[1].Value
    if($reference -match '^(https?:|mailto:)'){continue}
    if(!(Test-Path -LiteralPath (Join-Path $docs $reference))){throw "Lokale HTML-Referenz fehlt: $reference"}
}
Write-Host "PASS: Version $version, ESP32, $($bytes.Length) Bytes, SHA256, Quellstand, Flash-Offsets und GitHub-Pages-Pfade."
Write-Host "Firmware-URL: $imageUrl"
