[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (Get-Process BatteryMonitor -ErrorAction SilentlyContinue) {
    throw 'Exit BatteryMonitor first (closing its window may leave it in the tray).'
}
$destination = [IO.Path]::GetFullPath((Join-Path $root 'runtime\plugins\vxe\vxe-r1-nearlink.js'))
if (-not $destination.StartsWith([IO.Path]::GetFullPath($root) + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Plugin path escaped the project root.'
}
if (Test-Path -LiteralPath $destination) { Remove-Item -LiteralPath $destination }
$backup = Join-Path $root 'evidence\settings-before-install.json'
if (Test-Path -LiteralPath $backup) {
    $saved = Get-Content -Raw -LiteralPath $backup | ConvertFrom-Json
    foreach ($item in $saved) {
        if ($item.Sub -notmatch '^Software\\BatteryMonitor\\BatteryMonitor(\\|$)') {
            throw 'Invalid setting backup scope.'
        }
        $key = [Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($item.Sub)
        try {
            if ($item.Exists) {
                $kind = [Enum]::Parse([Microsoft.Win32.RegistryValueKind], $item.Kind)
                $key.SetValue($item.Name, $item.Value, $kind)
            } else { $key.DeleteValue($item.Name, $false) }
        } finally { $key.Dispose() }
    }
    $archived = Join-Path $root ('evidence\settings-restored-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.json')
    Move-Item -LiteralPath $backup -Destination $archived
}
Write-Host 'Plugin removed; backed-up changed settings restored. Upstream files/data left intact.'
