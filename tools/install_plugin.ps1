#Requires -Version 7.2
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'plugins\vxe-r1-nearlink.js'
$validation = Get-Content -Raw -LiteralPath (Join-Path $root 'evidence\validation-status.json') | ConvertFrom-Json
if (-not $validation.initialOfficialComparisonPassed -or -not $validation.mouseOffPassed -or
    (Get-Content -Raw -LiteralPath $source) -notmatch 'const PROTOCOL_VERIFIED = true;') {
    throw 'Hardware validation gate has not passed. Plugin not installed.'
}
if (Get-Process BatteryMonitor -ErrorAction SilentlyContinue) {
    throw 'Exit BatteryMonitor before installing/changing its settings.'
}
& (Join-Path $PSScriptRoot 'prepare_base.ps1')

# Save only values we change (not tokens or unrelated settings).
$base = 'Software\BatteryMonitor\BatteryMonitor'
$hash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData(
    [Text.Encoding]::UTF8.GetBytes('js:vxe/vxe-r1-nearlink:373B:10C9'))).Substring(0,16).ToLowerInvariant()
$values = @(
    @{ Sub=$base; Name='RefreshInterval'; Value=30000 },
    @{ Sub=$base; Name='StaleRetentionSec'; Value=0 },
    @{ Sub=$base; Name='RpcEnabled'; Value=0 },
    @{ Sub="$base\plugins"; Name='enabled'; Value=1 },
    @{ Sub="$base\plugins\disabled\vxe"; Name='vxe-r1-nearlink'; Value=0 },
    @{ Sub="$base\devices\$hash"; Name='LowBatteryAlert'; Value=0 },
    @{ Sub="$base\devices\$hash"; Name='KeepCachedForever'; Value=0 },
    @{ Sub="$base\devices\$hash"; Name='TrayVisible'; Value=1 }
)
$run = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('Software\Microsoft\Windows\CurrentVersion\Run')
try {
    if ($run -and $run.GetValue('BatteryMonitor')) {
        throw 'Existing BatteryMonitor autostart detected; disable it in the app before installation.'
    }
} finally { if ($run) { $run.Dispose() } }

$backup = Join-Path $root 'evidence\settings-before-install.json'
$destination = Join-Path $root 'runtime\plugins\vxe\vxe-r1-nearlink.js'
if (Test-Path -LiteralPath $backup) {
    throw 'Installation backup already exists; use uninstall_plugin.ps1 before reinstalling.'
}
if (Test-Path -LiteralPath $destination) {
    throw 'Destination plugin already exists; refusing to overwrite it.'
}
$saved = foreach ($item in $values) {
    $key = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($item.Sub)
    try {
        $exists = $key -and ($key.GetValueNames() -contains $item.Name)
        @{ Sub=$item.Sub; Name=$item.Name; Exists=[bool]$exists;
           Kind= $(if($exists){$key.GetValueKind($item.Name).ToString()}else{''});
           Value= $(if($exists){$key.GetValue($item.Name)}else{$null}) }
    } finally { if ($key) { $key.Dispose() } }
}
$saved | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $backup -Encoding utf8
try {
    foreach ($item in $values) {
        $key = [Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($item.Sub)
        try { $key.SetValue($item.Name, $item.Value, [Microsoft.Win32.RegistryValueKind]::DWord) }
        finally { $key.Dispose() }
    }
    New-Item -ItemType Directory -Force (Split-Path -Parent $destination) | Out-Null
    Copy-Item -LiteralPath $source -Destination $destination
} catch {
    & (Join-Path $PSScriptRoot 'uninstall_plugin.ps1')
    throw
}
Write-Host "Installed plugin with safe settings: $destination"
Write-Host 'Not launched automatically; start runtime\BatteryMonitor.exe for validation.'
