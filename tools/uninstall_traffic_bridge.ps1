#Requires -Version 7.2
[CmdletBinding()]param()
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(Get-Process TrafficMonitor,BatteryMonitor -ErrorAction SilentlyContinue){throw 'Exit TrafficMonitor and BatteryMonitor first'}
$backup=Join-Path $root 'evidence/traffic-bridge-install.json'
$state=Get-Content -Raw $backup | ConvertFrom-Json
if($state.PSObject.Properties['Version'] -and $state.Version -eq '0.3.0'){
    throw 'Rollback compact panel with set_compact_panel.ps1 before uninstalling; see evidence/compact-active-backup.txt'
}
$traffic=[IO.Path]::GetFullPath($state.TrafficDirectory)
$expectedDll=Join-Path $traffic 'plugins/VxeBattery.dll'
$expectedSkin=Join-Path $traffic 'skins/VXE Battery'
if([IO.Path]::GetFullPath($state.Dll) -ne $expectedDll -or [IO.Path]::GetFullPath($state.Skin) -ne $expectedSkin){throw 'Backup paths outside the exact bridge targets'}
if(Test-Path $expectedDll){
    if((Get-FileHash $expectedDll).Hash -ne $state.DllHash){throw 'Installed plugin changed; refusing deletion'}
    Remove-Item -LiteralPath $expectedDll
}
if(Test-Path $expectedSkin){
    if((Get-Item $expectedSkin).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Skin path is a reparse point; refusing recursive deletion'}
    if(Get-ChildItem $expectedSkin -Recurse -Force | Where-Object {$_.Attributes -band [IO.FileAttributes]::ReparsePoint}){throw 'Skin contains reparse points; refusing recursive deletion'}
    foreach($file in Get-ChildItem $expectedSkin -File -Recurse){
        $relative=[IO.Path]::GetRelativePath($expectedSkin,$file.FullName)
        $source=Join-Path $root "traffic-plugin/skin/$relative"
        if(!(Test-Path $source) -or (Get-FileHash $source).Hash -ne (Get-FileHash $file.FullName).Hash){throw 'Skin changed; refusing deletion'}
    }
    Remove-Item -LiteralPath $expectedSkin -Recurse
}
$iniBackup=Join-Path $root 'evidence/traffic-settings-before.json'
if(Test-Path $iniBackup){
    & python (Join-Path $PSScriptRoot 'traffic_config.py') $state.Config restore $iniBackup
    if($LASTEXITCODE){throw 'Restore TrafficMonitor settings failed'}
}
$key=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey('Software\BatteryMonitor\BatteryMonitor')
try {foreach($v in $state.RegistryValues){if($v.Exists){$key.SetValue($v.Name,$v.Value,[Microsoft.Win32.RegistryValueKind]::$($v.Kind))}else{$key.DeleteValue($v.Name,$false)}}}
finally {$key.Dispose()}
$stamp=Get-Date -Format yyyyMMdd-HHmmss
Move-Item -LiteralPath $backup -Destination "$backup.restored-$stamp"
if(Test-Path $iniBackup){Move-Item -LiteralPath $iniBackup -Destination "$iniBackup.restored-$stamp"}
Write-Host 'Bridge removed; only named settings restored. Original host and other plugins retained.'
