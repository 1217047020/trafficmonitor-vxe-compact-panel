#Requires -Version 7.2
[CmdletBinding()]param()
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(Get-Process BatteryMonitor,TrafficMonitor -ErrorAction SilentlyContinue){throw 'Exit both hosts before migration'}
$statePath=Join-Path $root 'evidence/traffic-bridge-install.json'
$state=Get-Content -Raw $statePath | ConvertFrom-Json
if($state.PSObject.Properties['Mode'] -and $state.Mode -eq 'native'){throw 'Native plugin already installed'}
$dll=[IO.Path]::GetFullPath($state.Dll)
$expected=Join-Path ([IO.Path]::GetFullPath($state.TrafficDirectory)) 'plugins/VxeBattery.dll'
if($dll -ne $expected){throw 'Unexpected installation path'}
if((Get-FileHash $dll).Hash -ne $state.DllHash){throw 'Existing bridge changed; refusing overwrite'}
$source=Join-Path $root 'build/traffic-native-x64/VxeBattery.dll'
if(!(Test-Path $source)){throw 'Build native plugin first'}
$backupDll=Join-Path $root 'downloads/VxeBattery-bridge-0.1.dll'
$backupState=Join-Path $root 'evidence/traffic-bridge-before-native.json'
if((Test-Path $backupDll) -or (Test-Path $backupState)){throw 'Migration backups already exist'}
Copy-Item -LiteralPath $dll -Destination $backupDll
Copy-Item -LiteralPath $statePath -Destination $backupState
# Restore exactly the RPC values changed by the bridge install. Do not touch
# autostart, battery history, other BatteryMonitor settings or TrafficMonitor INI.
$key=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey('Software\BatteryMonitor\BatteryMonitor')
$rpcBefore=foreach($name in 'RpcEnabled','RpcHost'){
    $exists=$key.GetValueNames() -contains $name
    @{Name=$name;Exists=$exists;Kind=$(if($exists){$key.GetValueKind($name).ToString()}else{''});Value=$key.GetValue($name)}
}
try {
    foreach($v in $state.RegistryValues){if($v.Exists){$key.SetValue($v.Name,$v.Value,[Microsoft.Win32.RegistryValueKind]::$($v.Kind))}else{$key.DeleteValue($v.Name,$false)}}
    Copy-Item -LiteralPath $source -Destination $dll
    $state.DllHash=(Get-FileHash $dll).Hash
    $state|Add-Member -NotePropertyName Mode -NotePropertyValue 'native' -Force
    $state|Add-Member -NotePropertyName Version -NotePropertyValue '0.2.0' -Force
    $state|Add-Member -NotePropertyName NativeSource -NotePropertyValue 'Windows HID read/write; no BatteryMonitor RPC' -Force
    $state|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $statePath -Encoding utf8
} catch {
    Copy-Item -LiteralPath $backupDll -Destination $dll
    Copy-Item -LiteralPath $backupState -Destination $statePath
    foreach($v in $rpcBefore){if($v.Exists){$key.SetValue($v.Name,$v.Value,[Microsoft.Win32.RegistryValueKind]::$($v.Kind))}else{$key.DeleteValue($v.Name,$false)}}
    throw
} finally {$key.Dispose()}
Write-Host 'Native plugin installed. Start TrafficMonitor only. BatteryMonitor is no longer required.'
