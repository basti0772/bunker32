param([string]$Port='',[switch]$BuildOnly,[switch]$Clean)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'tools/Web-Firmware.Common.ps1')
$settings=Get-Content -Raw (Join-Path $PSScriptRoot 'tools/build-config.json') | ConvertFrom-Json
$sketch=Join-Path $PSScriptRoot 'firmware\Bunker32'
$build=Join-Path $PSScriptRoot 'build'
$header=(Join-Path $sketch 'DisplayConfig.h').Replace('\','/')
$flags='-include "'+$header+'"'
# Keep __FILE__ / assertion strings and debug paths free of local user names.
# Arduino on Windows can pass either slash style, including doubled backslashes.
$pathMappings=@(
  @{From=[Environment]::GetFolderPath('UserProfile');To='/builder'},
  @{From=$PSScriptRoot;To='/project'}
)
foreach($mapping in $pathMappings){
  if([string]::IsNullOrWhiteSpace($mapping.From)){continue}
  $variants=@($mapping.From.Replace('\','/'),$mapping.From,$mapping.From.Replace('\','\\')) | Select-Object -Unique
  foreach($prefix in $variants){$flags+=' "-ffile-prefix-map='+$prefix+'='+$mapping.To+'"'}
}
# Windows PowerShell 5.1 strips embedded quotes in native arguments unless escaped.
$nativeMode=Get-Variable PSNativeCommandArgumentPassing -ValueOnly -ErrorAction SilentlyContinue
if($PSVersionTable.PSVersion.Major -lt 7 -or $nativeMode -eq 'Legacy'){$flags=$flags.Replace('"','\"')}
$compileArgs=@('compile','--fqbn',$settings.fqbn,'--build-path',$build,
    '--build-property',"compiler.cpp.extra_flags=$flags",'--build-property',"compiler.c.extra_flags=$flags",$sketch)
if($Clean){$compileArgs+='--clean'}
& arduino-cli @compileArgs
if($LASTEXITCODE -ne 0){throw "Kompilieren fehlgeschlagen (Exit $LASTEXITCODE)."}
$properties=& arduino-cli @compileArgs --show-properties=expanded
if($LASTEXITCODE -ne 0){throw 'Build-Eigenschaften konnten nicht gelesen werden.'}
Write-Utf8File (Join-Path $build 'build-properties.txt') (($properties -join "`n")+"`n")
$expanded=Read-BuildProperties (Join-Path $build 'build-properties.txt')
if($expanded['version'] -ne $settings.platformVersion){throw "Erwartet wird ESP32 Core $($settings.platformVersion), installiert ist $($expanded['version'])."}
$displayLibraryFound=$false
foreach($directory in (Get-Content -Raw (Join-Path $build 'libraries.cache') | ConvertFrom-Json)){
  $libraryFile=Join-Path $directory 'library.properties'
  if(!(Test-Path -LiteralPath $libraryFile)){continue}
  $library=Read-BuildProperties $libraryFile
  if($library['name'] -ne $settings.library){continue}
  $displayLibraryFound=$true
  if($library['version'] -ne $settings.libraryVersion){throw "Erwartet wird $($settings.library) $($settings.libraryVersion), verwendet wurde $($library['version'])."}
}
if(!$displayLibraryFound){throw 'Die verwendete Display-Bibliothek konnte nicht verifiziert werden.'}
if(!$BuildOnly){
  if([string]::IsNullOrWhiteSpace($Port)){throw 'Zum Flashen -Port angeben; -BuildOnly benoetigt keinen Port.'}
  & arduino-cli upload --port $Port --fqbn $settings.fqbn --upload-property upload.speed=115200 --input-dir $build $sketch
  if($LASTEXITCODE -ne 0){throw 'Flashen fehlgeschlagen.'}
}
