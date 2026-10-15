param([switch]$Incremental)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Web-Firmware.Common.ps1')
$root=Split-Path $PSScriptRoot -Parent
$build=Join-Path $root 'build'
& (Join-Path $root 'Build-And-Flash.ps1') -BuildOnly -Clean:(!$Incremental)
$properties=Read-BuildProperties (Join-Path $build 'build-properties.txt')
$layout=Read-FlashLayout $build
if($properties['build.mcu'] -ne 'esp32'){throw 'Dieser Installer ist nur fuer den vorhandenen klassischen ESP32 freigegeben.'}
foreach($pair in @(@('--flash-mode','build.flash_mode'),@('--flash-freq','build.img_freq'),@('--flash-size','build.flash_size'))){
    if($layout.settings[$pair[0]] -ne $properties[$pair[1]]){throw "Flash-Parameter stimmen nicht mit dem Build ueberein: $($pair[0])"}
}
$esptool=Join-Path $properties['tools.esptool_py.path'] $properties['tools.esptool_py.cmd']
if(!(Test-Path -LiteralPath $esptool)){throw 'Zum Build gehoerendes esptool fehlt.'}
$version=Get-FirmwareVersion $root
$output=Join-Path $build 'bunker32-web.bin'
# The base address is explicitly selected, not inferred from a board example.
# Every source offset and flash parameter comes from this build's response file.
Push-Location $build
try {
    & $esptool --chip $properties['build.mcu'] merge-bin --format raw --target-offset 0 -o $output '@flash_args'
    if($LASTEXITCODE -ne 0){throw 'esptool merge-bin fehlgeschlagen.'}
} finally { Pop-Location }
$bytes=[IO.File]::ReadAllBytes($output)
if($bytes.Length -ne $layout.imageEnd){throw 'Merged BIN hat nicht die erwartete Laenge ab Adresse 0.'}
$inputMetadata=@()
foreach($part in $layout.parts){
    $source=[IO.File]::ReadAllBytes($part.fullPath)
    for($i=0;$i -lt $source.Length;$i++){
        if($bytes[$part.offset+$i] -ne $source[$i]){throw "Merged BIN weicht am Build-Offset von $($part.name) ab."}
    }
    $inputMetadata+=[ordered]@{file=$part.name;offset=$part.offset;size=$part.size;sha256=(Get-BinarySha256 $part.fullPath)}
}
$settings=Get-Content -Raw (Join-Path $PSScriptRoot 'build-config.json') | ConvertFrom-Json
$metadata=[ordered]@{
    name='Bunker 32';version=$version;chipFamily=$properties['build.mcu'].ToUpperInvariant();fqbn=$properties['build.fqbn']
    coreVersion=$properties['version'];library="$($settings.library) $($settings.libraryVersion)"
    flashMode=$layout.settings['--flash-mode'];flashFrequency=$layout.settings['--flash-freq'];flashSize=$layout.settings['--flash-size']
    partitionScheme=$properties['build.partitions'];mergedOffset=0;size=$bytes.Length
    sha256=(Get-BinarySha256 $output);offsetSource='Arduino build/flash_args'
    inputs=$inputMetadata;sourceHashes=(Get-SourceHashes $root)
}
$manifest=[ordered]@{
    name='Bunker 32';version=$version;new_install_prompt_erase=$false;new_install_improv_wait_time=0
    builds=@([ordered]@{chipFamily=$metadata.chipFamily;parts=@([ordered]@{path='firmware/bunker32.bin';offset=$metadata.mergedOffset})})
}
$docs=Join-Path $root 'docs'
$webFiles=Join-Path $docs 'firmware'
New-Item -ItemType Directory -Force -Path $webFiles | Out-Null
Copy-Item -LiteralPath $output -Destination (Join-Path $root 'firmware/bunker32.bin')
Copy-Item -LiteralPath $output -Destination (Join-Path $webFiles 'bunker32.bin')
Write-Utf8File (Join-Path $docs 'manifest.json') (($manifest | ConvertTo-Json -Depth 8)+"`n")
Write-Utf8File (Join-Path $webFiles 'build-info.json') (($metadata | ConvertTo-Json -Depth 8)+"`n")
Copy-Item -LiteralPath (Join-Path $build 'flash_args') -Destination (Join-Path $webFiles 'flash_args')
Copy-Item -LiteralPath (Join-Path $build 'partitions.csv') -Destination (Join-Path $webFiles 'partitions.csv')
& (Join-Path $PSScriptRoot 'Test-Web-Firmware.ps1') -WithBuild
Write-Host "Web-Firmware: firmware/bunker32.bin ($($bytes.Length) Bytes), Version $version, Flash-Basis 0x0000"
