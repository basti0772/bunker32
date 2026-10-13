param([string]$Port='COM3')
$ErrorActionPreference='Stop'
$backup=Join-Path $PSScriptRoot 'backups\before-bunker32-safe.bin'
if(!(Test-Path -LiteralPath $backup) -or (Get-Item -LiteralPath $backup).Length -ne 4194304){throw 'Keine vollstaendige 4-MB-Sicherung vorhanden.'}
$hashFile=$backup+'.sha256'
if(!(Test-Path -LiteralPath $hashFile)){throw 'Pruefsummendatei fehlt.'}
if((Get-FileHash -LiteralPath $backup -Algorithm SHA256).Hash -ne (Get-Content -LiteralPath $hashFile).Trim()){throw 'Pruefsumme der Sicherung stimmt nicht.'}
$tool=Get-ChildItem (Join-Path $env:LOCALAPPDATA 'Arduino15\packages\esp32\tools\esptool_py') -Filter esptool.exe -Recurse | Sort-Object FullName -Descending | Select-Object -First 1
if(!$tool){throw 'esptool.exe nicht gefunden.'}
& $tool.FullName --chip esp32 --port $Port --baud 115200 write-flash 0 $backup
if($LASTEXITCODE -ne 0){throw 'Wiederherstellen fehlgeschlagen.'}
