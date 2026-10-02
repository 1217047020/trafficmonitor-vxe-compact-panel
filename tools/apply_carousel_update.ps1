#Requires -Version 7.2
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BackupDirectory,
    [ValidateSet('apply','rollback')][string]$Mode='apply',
    [switch]$Apply
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$backup=[IO.Path]::GetFullPath($BackupDirectory)
$allowed=[IO.Path]::GetFullPath((Join-Path $root 'work/carousel-update'))+[IO.Path]::DirectorySeparatorChar
if(!$backup.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Backup outside work/carousel-update'}
$manifest=Join-Path $backup 'transaction.json'
$t=Get-Content -LiteralPath $manifest -Raw|ConvertFrom-Json
$statePath=Join-Path $root 'evidence/traffic-bridge-install.json'
$state=Get-Content -LiteralPath $statePath -Raw|ConvertFrom-Json
$oldStatePath=Join-Path $backup 'install-state-before.json'
$oldState=Get-Content -LiteralPath $oldStatePath -Raw|ConvertFrom-Json
$traffic=[IO.Path]::GetFullPath($oldState.TrafficDirectory)
$dll=Join-Path $traffic 'plugins/VxeBattery.dll'
$config=Join-Path $traffic 'config.ini'
if($t.Dll -ne $dll -or $t.Config -ne $config -or $state.Dll -ne $dll -or $oldState.Dll -ne $dll){throw 'Unexpected installation paths'}
$oldDll=Join-Path $backup 'VxeBattery-before.dll'
if((Get-FileHash -LiteralPath $oldDll).Hash -ne $t.OldDllHash){throw 'Original DLL backup hash mismatch'}
if($Mode -eq 'apply'){
    if($t.Status -ne 'backed-up-running' -or $state.Version -ne '0.3.0' -or $state.Mode -ne 'native'){throw 'Unexpected update baseline'}
    if((Get-FileHash -LiteralPath $dll).Hash -ne $t.OldDllHash -or $state.DllHash -ne $t.OldDllHash){throw 'Installed DLL baseline changed'}
    $source=Join-Path $root 'build/traffic-compact-x64/VxeBattery.dll'
    if((Get-FileHash -LiteralPath $source).Hash -ne $t.NewDllHash){throw 'Candidate changed after backup; revalidate first'}
}else{
    if($t.Status -ne 'applied' -or $state.Version -ne '0.4.0-preview'){throw 'Unexpected rollback baseline'}
    if((Get-FileHash -LiteralPath $dll).Hash -ne $t.NewDllHash -or $state.DllHash -ne $t.NewDllHash){throw 'Installed candidate changed; refusing rollback'}
    $source=$oldDll
}
Write-Host "Mode=$Mode; DLL=$dll; backup=$backup; no configuration keys will be edited"
if(!$Apply){Write-Host 'Dry run only';return}
if(Get-Process -Name TrafficMonitor -ErrorAction SilentlyContinue){throw 'Exit TrafficMonitor normally before replacing DLL'}
# Save final post-exit config, not just the earlier running snapshot. Never
# overwrite config on normal rollback: retain later unrelated user changes.
$configHash=(Get-FileHash -LiteralPath $config).Hash
if($Mode -eq 'apply'){
    $configBackup=Join-Path $backup 'config-before.ini'
    if(Test-Path -LiteralPath $configBackup){throw 'Post-exit configuration backup already exists'}
    Copy-Item -LiteralPath $config -Destination $configBackup
    try{
        Copy-Item -LiteralPath $source -Destination $dll
        if((Get-FileHash -LiteralPath $dll).Hash -ne $t.NewDllHash){throw 'Installed candidate hash mismatch'}
        if((Get-FileHash -LiteralPath $config).Hash -ne $configHash){throw 'Configuration changed during replacement'}
        $state.DllHash=$t.NewDllHash;$state.Version='0.4.0-preview'
        $state.NativeSource='Windows native HID mouse/controller; text carousel; case provider pending'
        $state|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $statePath -Encoding utf8
        $t.Status='applied'
    }catch{
        Copy-Item -LiteralPath $oldDll -Destination $dll
        Copy-Item -LiteralPath $oldStatePath -Destination $statePath
        $t.Status='failed-restored'
        $t|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $manifest -Encoding utf8
        throw
    }
}else{
    $currentDll=Join-Path $backup 'VxeBattery-before-rollback.dll'
    $currentState=Join-Path $backup 'install-state-before-rollback.json'
    if(Test-Path -LiteralPath $currentDll){throw 'Rollback capture already exists'}
    Copy-Item -LiteralPath $dll -Destination $currentDll
    Copy-Item -LiteralPath $statePath -Destination $currentState
    try{
        Copy-Item -LiteralPath $oldDll -Destination $dll
        if((Get-FileHash -LiteralPath $dll).Hash -ne $t.OldDllHash){throw 'Restored DLL hash mismatch'}
        if((Get-FileHash -LiteralPath $config).Hash -ne $configHash){throw 'Configuration changed during rollback'}
        Copy-Item -LiteralPath $oldStatePath -Destination $statePath
        $t.Status='rolled-back'
    }catch{
        Copy-Item -LiteralPath $currentDll -Destination $dll
        Copy-Item -LiteralPath $currentState -Destination $statePath
        throw
    }
}
$t|Add-Member -NotePropertyName ConfigReplacementHash -NotePropertyValue $configHash -Force
$t|Add-Member -NotePropertyName AppliedAt -NotePropertyValue ([DateTimeOffset]::UtcNow.ToOffset([TimeSpan]::FromHours(8)).ToString('o')) -Force
$t|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $manifest -Encoding utf8
Write-Host "Completed $Mode; host not restarted; configuration hash unchanged"
