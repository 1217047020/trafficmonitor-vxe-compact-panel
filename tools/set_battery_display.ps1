#Requires -Version 7.2
[CmdletBinding()]
param([ValidateSet('taskbar','desktop')][string]$Mode='taskbar')
$ErrorActionPreference='Stop'
if(Get-Process TrafficMonitor -ErrorAction SilentlyContinue){throw 'Exit TrafficMonitor before switching display mode'}
$root=Split-Path $PSScriptRoot -Parent
$state=Get-Content -Raw (Join-Path $root 'evidence/traffic-bridge-install.json') | ConvertFrom-Json
if($state.PSObject.Properties['Version'] -and $state.Version -eq '0.3.0'){
    throw 'Rollback compact panel before using legacy display switching; see evidence/compact-active-backup.txt'
}
& python (Join-Path $PSScriptRoot 'traffic_config.py') $state.Config $Mode (Join-Path $root 'evidence/traffic-settings-before.json')
if($LASTEXITCODE){throw 'Switch failed'}
Start-Process (Join-Path $state.TrafficDirectory 'TrafficMonitor.exe') -WorkingDirectory $state.TrafficDirectory -WindowStyle Hidden
