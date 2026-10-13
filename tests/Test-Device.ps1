param([string]$Port='COM3')
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$root=Split-Path $PSScriptRoot -Parent
$out=Join-Path $root 'artifacts'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$serial=[System.IO.Ports.SerialPort]::new($Port,921600)
$serial.DtrEnable=$false;$serial.RtsEnable=$false;$serial.ReadTimeout=4000;$serial.WriteTimeout=1000
$serial.ReadBufferSize=131072
function Send([string]$Command){$serial.WriteLine($Command)}
function Read-Line([string]$Prefix){
    $deadline=[DateTime]::UtcNow.AddSeconds(5)
    while([DateTime]::UtcNow -lt $deadline){
        $line=$serial.ReadLine().Trim()
        if($line.StartsWith($Prefix)){return $line}
    }
    throw "Keine Antwort: $Prefix"
}
function State {
    Send 'STATE'
    $line=Read-Line 'STATE '
    Write-Output $line | Out-Host
    $values=@{}
    foreach($pair in ($line -split ' ' | Select-Object -Skip 1)){$parts=$pair.Split('=');if($parts.Length -eq 2){$values[$parts[0]]=$parts[1]}}
    return $values
}
function Hold([int]$Mask,[int]$Milliseconds){
    $until=[DateTime]::UtcNow.AddMilliseconds($Milliseconds)
    do{Send "K $Mask";Start-Sleep -Milliseconds 35}while([DateTime]::UtcNow -lt $until)
    Send 'K 0'
}
function Read-Bytes([int]$Count){
    $data=[byte[]]::new($Count);$offset=0
    while($offset -lt $Count){$offset+=$serial.Read($data,$offset,$Count-$offset)}
    return ,$data
}
function Snapshot([string]$Name){
    Send 'SNAP'
    $magic=[System.Text.Encoding]::ASCII.GetBytes('B32F');$matched=0
    while($matched -lt 4){$value=$serial.ReadByte();if($value -eq $magic[$matched]){$matched++}else{$matched=0}}
    $header=Read-Bytes 12
    $size=[BitConverter]::ToInt32($header,0)
    if($size -ne 38400){throw "Unerwartete Bildgroesse: $size"}
    $pixels=Read-Bytes $size
    $bmp=[System.Drawing.Bitmap]::new(160,120)
    for($y=0;$y -lt 120;$y++){for($x=0;$x -lt 160;$x++){
        $c=[BitConverter]::ToUInt16($pixels,($y*160+$x)*2)
        $red=[int]((($c -shr 11)-band 31)*255/31);$green=[int]((($c -shr 5)-band 63)*255/63);$blue=[int](($c-band 31)*255/31)
        $bmp.SetPixel($x,$y,[System.Drawing.Color]::FromArgb($red,$green,$blue))
    }}
    $path=Join-Path $out "$Name.png";$bmp.Save($path,[System.Drawing.Imaging.ImageFormat]::Png);$bmp.Dispose()
    Write-Host "Bild gespeichert: $path"
}
function Number([string]$Value){[double]::Parse($Value,[System.Globalization.CultureInfo]::InvariantCulture)}
try {
    $serial.Open();Send 'STREAM 0';Start-Sleep -Milliseconds 600;$serial.DiscardInBuffer()
    Send 'SELFTEST';$test=Read-Line 'SELFTEST';Write-Host $test
    if($test -ne 'SELFTEST failures=0'){throw 'Spielmechanik-Selbsttest fehlgeschlagen.'}
    Send 'MENU';Start-Sleep -Milliseconds 100;Snapshot 'title'
    Send 'START';Start-Sleep -Milliseconds 150;$first=State
    if($first.mode -ne 'PLAYING'){throw 'Start fehlgeschlagen.'}
    Snapshot 'first-room'
    Hold 1 300;Start-Sleep -Milliseconds 100;$moved=State
    if((Number $moved.y) -le (Number $first.y)+0.3){throw 'PC-Bewegung fehlgeschlagen.'}
    # An unrefreshed held key must expire after 250 ms.
    Send 'K 1';Start-Sleep -Milliseconds 450;$stopped=State
    Start-Sleep -Milliseconds 400;$still=State
    if([Math]::Abs((Number $still.y)-(Number $stopped.y)) -gt .04){throw 'Eingabe-Watchdog fehlgeschlagen.'}
    Send 'NEW';Hold 4 260;Hold 64 900;Snapshot 'gameplay';$combat=State
    if([int]$combat.kills -lt 1){throw 'Treffer-/Gegnertest fehlgeschlagen.'}
    Send 'MAP';Snapshot 'map'
    Send 'PAUSE';$paused=State;Hold 1 300;$after=State
    if($paused.mode -ne 'PAUSED' -or $paused.x -ne $after.x -or $paused.y -ne $after.y){throw 'Pause fehlgeschlagen.'}
    Write-Host 'PASS: Selbsttest, Start, PC-Bewegung, Eingabe-Watchdog, Kampf, Pause, Bilduebertragung.'
} finally {
    if($serial.IsOpen){Send 'K 0';Send 'STREAM 0';Send 'MENU';$serial.Close()}
    $serial.Dispose()
}
