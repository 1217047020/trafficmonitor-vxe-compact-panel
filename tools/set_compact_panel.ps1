#Requires -Version 7.2
[CmdletBinding()]
param(
    [ValidateSet('enable','rollback')][string]$Mode='enable',
    [switch]$Apply,
    [switch]$VisualConfirmed,
    [string]$BackupDirectory=''
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$statePath=Join-Path $root 'evidence/traffic-bridge-install.json'
$state=Get-Content -Raw -LiteralPath $statePath | ConvertFrom-Json
$traffic=[IO.Path]::GetFullPath($state.TrafficDirectory)
$dll=[IO.Path]::GetFullPath($state.Dll)
$config=[IO.Path]::GetFullPath($state.Config)
if($dll -ne (Join-Path $traffic 'plugins/VxeBattery.dll') -or $config -ne (Join-Path $traffic 'config.ini')){
    throw 'Unexpected installation paths'
}
$configTool=Join-Path $PSScriptRoot 'compact_config.py'
$source=Join-Path $root 'build/traffic-compact-x64/VxeBattery.dll'
if($Mode -eq 'enable'){
    if(!(Test-Path -LiteralPath $source)){throw 'Build compact x64 DLL first'}
    if($state.Version -ne '0.2.0' -or $state.Mode -ne 'native'){throw 'Expected the validated native 0.2.0 baseline'}
    if((Get-FileHash -LiteralPath $dll).Hash -ne $state.DllHash){throw 'Installed DLL changed; refusing overwrite'}
    $changesJson= & python $configTool $config plan
    if($LASTEXITCODE){throw 'Config planning failed'}
    Write-Host "DLL: $dll"
    Write-Host "New source: $source"
    Write-Host "Only display settings change (no collection/font/history/skin/menu change):"
    $changesJson | Out-Host
    if(!$Apply){Write-Host 'Dry run only. No files changed. Confirm previews first, exit TrafficMonitor, then use -Apply -VisualConfirmed.';return}
    if(!$VisualConfirmed){throw 'Visual confirmation is required before applying'}
    if(Get-Process TrafficMonitor,BatteryMonitor -ErrorAction SilentlyContinue){throw 'Exit hosts before replacing DLL; do not terminate forcibly'}
    if(!$BackupDirectory){$BackupDirectory=Join-Path $root ('work/compact-switch/'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))}
    $backup=[IO.Path]::GetFullPath($BackupDirectory)
    $allowed=[IO.Path]::GetFullPath((Join-Path $root 'work/compact-switch'))+[IO.Path]::DirectorySeparatorChar
    if(!$backup.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Backup must be within workspace work/compact-switch'}
    if(Test-Path -LiteralPath $backup){throw 'Backup directory already exists'}
    New-Item -ItemType Directory -Path $backup | Out-Null
    Copy-Item -LiteralPath $dll -Destination (Join-Path $backup 'VxeBattery-0.2.dll')
    Copy-Item -LiteralPath $config -Destination (Join-Path $backup 'config-before.ini')
    Copy-Item -LiteralPath $statePath -Destination (Join-Path $backup 'install-state-before.json')
    $changesPath=Join-Path $backup 'changes.json'
    $changesJson | Set-Content -LiteralPath $changesPath -Encoding utf8
    $oldConfigHash=(Get-FileHash -LiteralPath $config).Hash
    $newDllHash=(Get-FileHash -LiteralPath $source).Hash
    $transaction=@{Version=1;Dll=$dll;Config=$config;OldDllHash=$state.DllHash;NewDllHash=$newDllHash;ConfigBeforeHash=$oldConfigHash;Status='backed-up'}
    $manifest=Join-Path $backup 'transaction.json'
    $transaction | ConvertTo-Json | Set-Content -LiteralPath $manifest -Encoding utf8
    try{
        Copy-Item -LiteralPath $source -Destination $dll
        if((Get-FileHash -LiteralPath $dll).Hash -ne $newDllHash){throw 'Installed DLL hash mismatch'}
        & python $configTool $config enable --changes $changesPath
        if($LASTEXITCODE){throw 'Config update failed'}
        $state.DllHash=$newDllHash;$state.Version='0.3.0'
        $state | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $statePath -Encoding utf8
        $transaction.Status='enabled'
        $transaction | ConvertTo-Json | Set-Content -LiteralPath $manifest -Encoding utf8
    }catch{
        # Host is stopped, transaction is synchronous: restore exact originals
        # only for a failed enable, not for a later normal rollback.
        Copy-Item -LiteralPath (Join-Path $backup 'VxeBattery-0.2.dll') -Destination $dll
        Copy-Item -LiteralPath (Join-Path $backup 'config-before.ini') -Destination $config
        Copy-Item -LiteralPath (Join-Path $backup 'install-state-before.json') -Destination $statePath
        $transaction.Status='failed-restored'
        $transaction | ConvertTo-Json | Set-Content -LiteralPath $manifest -Encoding utf8
        throw
    }
    Write-Host "Enabled (host not restarted). Backup: $backup"
    Write-Host "Start TrafficMonitor normally; validate data, off/on recovery, tooltip and menu. Rollback with -Mode rollback -BackupDirectory `"$backup`" -Apply."
}else{
    if(!$BackupDirectory){throw 'Provide the exact backup directory reported by enable'}
    $backup=[IO.Path]::GetFullPath($BackupDirectory)
    $allowed=[IO.Path]::GetFullPath((Join-Path $root 'work/compact-switch'))+[IO.Path]::DirectorySeparatorChar
    if(!$backup.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Backup is outside work/compact-switch'}
    $manifest=Join-Path $backup 'transaction.json'
    $transaction=Get-Content -Raw -LiteralPath $manifest | ConvertFrom-Json
    if($transaction.Status -ne 'enabled' -or $transaction.Dll -ne $dll -or $transaction.Config -ne $config){throw 'Unexpected rollback transaction'}
    if((Get-FileHash -LiteralPath $dll).Hash -ne $transaction.NewDllHash){throw 'Installed DLL changed; refusing rollback'}
    $oldDll=Join-Path $backup 'VxeBattery-0.2.dll'
    if((Get-FileHash -LiteralPath $oldDll).Hash -ne $transaction.OldDllHash){throw 'Backup DLL hash mismatch'}
    Write-Host "Restore DLL from: $oldDll; restore only the five display keys, preserve later unrelated settings."
    if(!$Apply){Write-Host 'Dry run only. No files changed.';return}
    if(Get-Process TrafficMonitor,BatteryMonitor -ErrorAction SilentlyContinue){throw 'Exit hosts before rollback'}
    # Python validates every key before replacing config. On conflict, do nothing.
    Copy-Item -LiteralPath $config -Destination (Join-Path $backup 'config-before-rollback.ini')
    Copy-Item -LiteralPath $dll -Destination (Join-Path $backup 'VxeBattery-before-rollback.dll')
    Copy-Item -LiteralPath $statePath -Destination (Join-Path $backup 'install-state-before-rollback.json')
    & python $configTool $config restore --changes (Join-Path $backup 'changes.json')
    if($LASTEXITCODE){throw 'Config conflict/update failed; DLL was not changed'}
    try{
        Copy-Item -LiteralPath $oldDll -Destination $dll
        if((Get-FileHash -LiteralPath $dll).Hash -ne $transaction.OldDllHash){throw 'Restored DLL hash mismatch'}
        Copy-Item -LiteralPath (Join-Path $backup 'install-state-before.json') -Destination $statePath
        $transaction.Status='rolled-back'
        $transaction | ConvertTo-Json | Set-Content -LiteralPath $manifest -Encoding utf8
    }catch{
        Copy-Item -LiteralPath (Join-Path $backup 'config-before-rollback.ini') -Destination $config
        Copy-Item -LiteralPath (Join-Path $backup 'VxeBattery-before-rollback.dll') -Destination $dll
        Copy-Item -LiteralPath (Join-Path $backup 'install-state-before-rollback.json') -Destination $statePath
        throw
    }
    Write-Host 'Rolled back to original 0.2.0 display; host not restarted. History and unrelated settings retained.'
}
