Set-StrictMode -Version Latest

function Write-Utf8File([string]$Path, [string]$Content) {
    [IO.File]::WriteAllText($Path, $Content, [Text.UTF8Encoding]::new($false))
}

function Get-BinarySha256([string]$Path) {
    $stream=[IO.File]::OpenRead($Path)
    $sha=[Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','').ToLowerInvariant() }
    finally { $sha.Dispose(); $stream.Dispose() }
}

function Get-FirmwareVersion([string]$Root) {
    $header = [IO.File]::ReadAllText((Join-Path $Root 'firmware/Bunker32/Version.h'))
    $match = [regex]::Match($header, '(?m)^#define BUNKER32_VERSION "(\d+\.\d+\.\d+)"\s*$')
    if (!$match.Success) { throw 'Version.h muss BUNKER32_VERSION im Format X.Y.Z enthalten.' }
    return $match.Groups[1].Value
}

function Read-BuildProperties([string]$Path) {
    $properties = @{}
    foreach ($line in [IO.File]::ReadAllLines($Path)) {
        $at = $line.IndexOf('=')
        if ($at -gt 0) { $properties[$line.Substring(0,$at)] = $line.Substring($at+1) }
    }
    return $properties
}

function Get-SourceHashes([string]$Root) {
    $files = @(Get-ChildItem (Join-Path $Root 'firmware/Bunker32') -File |
        Where-Object { $_.Extension -in @('.ino','.cpp','.h') } | Sort-Object Name)
    $files += Get-Item (Join-Path $Root 'tools/build-config.json')
    $result = [ordered]@{}
    foreach ($file in $files) {
        # Normalize checkout line endings, so Windows and CI produce the same source identity.
        $content = [IO.File]::ReadAllText($file.FullName).Replace("`r`n", "`n")
        $sha = [Security.Cryptography.SHA256]::Create()
        try { $hash = [BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($content))).Replace('-','').ToLowerInvariant() }
        finally { $sha.Dispose() }
        $relative = $file.FullName.Substring($Root.TrimEnd('\','/').Length+1).Replace('\','/')
        $result[$relative] = $hash
    }
    return $result
}

function Read-FlashLayout([string]$BuildDirectory) {
    $path = Join-Path $BuildDirectory 'flash_args'
    if (!(Test-Path -LiteralPath $path)) { throw 'Der Build hat keine flash_args erzeugt; Offsets werden nicht geraten.' }
    $settings = @{}
    $parts = @()
    foreach ($rawLine in [IO.File]::ReadAllLines($path)) {
        $line = $rawLine.Trim()
        if (!$line -or $line.StartsWith('#')) { continue }
        if ($line.StartsWith('--')) {
            $tokens = $line -split '\s+'
            if ($tokens.Count % 2) { throw "Ungueltige Flash-Optionen: $line" }
            for ($i=0; $i -lt $tokens.Count; $i+=2) {
                if ($tokens[$i] -notin @('--flash-mode','--flash-freq','--flash-size') -or $settings.ContainsKey($tokens[$i])) {
                    throw "Unerwartete oder doppelte Flash-Option: $($tokens[$i])"
                }
                $settings[$tokens[$i]] = $tokens[$i+1]
            }
            continue
        }
        $match = [regex]::Match($line, '^(0x[0-9a-fA-F]+|\d+)\s+(?:"([^"]+)"|(\S+))$')
        if (!$match.Success) { throw "Ungueltiger flash_args-Eintrag: $line" }
        $address = $match.Groups[1].Value
        $offset = if ($address.StartsWith('0x')) { [Convert]::ToInt64($address.Substring(2),16) } else { [long]$address }
        $name = $match.Groups[2].Value + $match.Groups[3].Value
        $full = [IO.Path]::GetFullPath((Join-Path $BuildDirectory $name))
        $prefix = [IO.Path]::GetFullPath($BuildDirectory).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
        if (!$full.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)) { throw 'BIN liegt ausserhalb des Build-Verzeichnisses.' }
        if (!(Test-Path -LiteralPath $full -PathType Leaf)) { throw "Build-Image fehlt: $name" }
        $length = (Get-Item -LiteralPath $full).Length
        if ($length -le 0) { throw "Leeres Build-Image: $name" }
        $parts += [pscustomobject]@{ name=$name; offset=$offset; size=$length; fullPath=$full }
    }
    foreach ($key in @('--flash-mode','--flash-freq','--flash-size')) {
        if (!$settings.ContainsKey($key) -or $settings[$key] -eq 'keep') { throw "Konkreter Wert fuer $key fehlt in flash_args." }
    }
    $sizeMatch = [regex]::Match($settings['--flash-size'], '^(\d+)MB$')
    if (!$sizeMatch.Success) { throw 'Unbekannte Flash-Groesse in flash_args.' }
    $flashBytes = [long]$sizeMatch.Groups[1].Value * 1MB
    $end = 0L
    foreach ($part in ($parts | Sort-Object offset)) {
        if ($part.offset -lt $end -or $part.offset+$part.size -gt $flashBytes) { throw 'Images ueberlappen oder ueberschreiten den Flash.' }
        $end = $part.offset+$part.size
    }
    if ($parts.Count -eq 0) { throw 'flash_args enthaelt keine Images.' }
    return [pscustomobject]@{ settings=$settings; parts=$parts; flashBytes=$flashBytes; imageEnd=$end }
}
