$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$out=Join-Path $root 'artifacts'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$exe=Join-Path $out 'Bunker32-Controller.exe'
if(Test-Path -LiteralPath $exe){Remove-Item -LiteralPath $exe}
Add-Type -TypeDefinition (Get-Content -Raw -Encoding UTF8 (Join-Path $PSScriptRoot 'Controller.cs')) -ReferencedAssemblies System.Windows.Forms,System.Drawing -OutputAssembly $exe -OutputType WindowsApplication
Write-Output $exe
