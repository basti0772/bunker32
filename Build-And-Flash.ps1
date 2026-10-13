param([string]$Port='COM3',[switch]$BuildOnly)
$ErrorActionPreference='Stop'
$sketch=Join-Path $PSScriptRoot 'firmware\Bunker32'
$build=Join-Path $PSScriptRoot 'build'
$config=(Join-Path $sketch 'DisplayConfig.h').Replace('\','/')
$flags='-include "'+$config+'"'
& arduino-cli compile --fqbn esp32:esp32:esp32 --build-path $build --build-property "compiler.cpp.extra_flags=$flags" --build-property "compiler.c.extra_flags=$flags" $sketch
if($LASTEXITCODE -ne 0){throw 'Kompilieren fehlgeschlagen.'}
if(!$BuildOnly){
  & arduino-cli upload --port $Port --fqbn esp32:esp32:esp32 --upload-property upload.speed=115200 --input-dir $build $sketch
  if($LASTEXITCODE -ne 0){throw 'Flashen fehlgeschlagen.'}
}
