# Download/extract the pinned original release; does not launch or install plugin.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$expected = '3a27989a6e09b3d00cbc0ed2ee901f3c16a7dc60f720e4586e2245137bac486d'
$url = 'https://github.com/JKWTCN/BatteryMonitor/releases/download/v1.4.260908/BatteryMonitor.zip'
$zip = Join-Path $root 'downloads\BatteryMonitor-v1.4.260908.zip'
New-Item -ItemType Directory -Force (Split-Path -Parent $zip) | Out-Null
if (-not (Test-Path -LiteralPath $zip)) { Invoke-WebRequest $url -OutFile $zip }
if ((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) {
    throw 'Original release SHA256 mismatch; do not execute this archive.'
}
$destination = Join-Path $root 'runtime'
if (Test-Path -LiteralPath $destination) {
    Write-Host "Verified original archive. Existing runtime left unchanged: $destination"
} else {
    Expand-Archive -LiteralPath $zip -DestinationPath $destination
    Write-Host "Verified original archive extracted: $destination"
}
