#Requires -Version 7.2
[CmdletBinding()]
param([string]$TrafficDirectory=(Join-Path ([Environment]::GetFolderPath('Desktop')) 'TrafficMonitor'))
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(Get-Process TrafficMonitor,BatteryMonitor -ErrorAction SilentlyContinue){throw 'Exit TrafficMonitor and BatteryMonitor first'}
$traffic=[IO.Path]::GetFullPath($TrafficDirectory)
$exe=Join-Path $traffic 'TrafficMonitor.exe'
if(!(Test-Path $exe)){throw 'TrafficMonitor.exe missing'}
$raw=[IO.File]::ReadAllBytes($exe)
$pe=[BitConverter]::ToInt32($raw,60)
if([BitConverter]::ToUInt16($raw,$pe+4) -ne 0x8664){throw 'This build requires x64 TrafficMonitor'}
$config=Join-Path $traffic 'config.ini'
$global=Get-Content -Raw (Join-Path $traffic 'global_cfg.ini')
if($global -notmatch 'portable_mode\s*=\s*true'){throw 'Only verified portable configuration supported'}
$backup=Join-Path $root 'evidence/traffic-bridge-install.json'
$iniBackup=Join-Path $root 'evidence/traffic-settings-before.json'
$dll=Join-Path $traffic 'plugins/VxeBattery.dll'
$skin=Join-Path $traffic 'skins/VXE Battery'
if((Test-Path $backup) -or (Test-Path $iniBackup) -or (Test-Path $dll) -or (Test-Path $skin)){throw 'Existing bridge/backup found; refusing overwrite'}
$key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('Software\BatteryMonitor\BatteryMonitor')
try {
    if($key.GetValue('RpcToken')){throw 'Existing RPC authentication found; keep it unchanged and configure bridge authentication separately'}
    $values=foreach($name in 'RpcEnabled','RpcHost'){
        $exists=$key.GetValueNames() -contains $name
        @{Name=$name;Exists=$exists;Kind=$(if($exists){$key.GetValueKind($name).ToString()}else{''});Value=$key.GetValue($name)}
    }
    $port=[int]$key.GetValue('RpcPort',19211)
    if($port -lt 1024 -or $port -gt 65535){throw 'Invalid configured RPC port'}
} finally {$key.Dispose()}
if((netstat -ano) -match ":$port\s+\S+\s+LISTENING\s+"){throw 'RPC port already occupied'}
$source=Join-Path $root 'build/traffic-x64/VxeBattery.dll'
if(!(Test-Path $source)){throw 'Build plugin first'}
$state=@{TrafficDirectory=$traffic;Config=$config;Dll=$dll;Skin=$skin;RegistryValues=@($values);Port=$port;DllHash=(Get-FileHash $source).Hash}
$state | ConvertTo-Json -Depth 6 | Set-Content $backup -Encoding utf8
try {
    & python (Join-Path $PSScriptRoot 'traffic_config.py') $config install $iniBackup
    if($LASTEXITCODE){throw 'INI edit failed'}
    Copy-Item $source $dll
    Copy-Item (Join-Path $root 'traffic-plugin/skin') $skin -Recurse
    $key=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey('Software\BatteryMonitor\BatteryMonitor')
    try {$key.SetValue('RpcHost','127.0.0.1',[Microsoft.Win32.RegistryValueKind]::String);$key.SetValue('RpcEnabled',1,[Microsoft.Win32.RegistryValueKind]::DWord)}
    finally {$key.Dispose()}
} catch {
    $failure=$_
    & (Join-Path $PSScriptRoot 'uninstall_traffic_bridge.ps1')
    throw $failure
}
Write-Host "Installed bridge. Loopback RPC port: $port. Existing traffic display preserved."
