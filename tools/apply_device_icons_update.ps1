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
$allowed=[IO.Path]::GetFullPath((Join-Path $root 'work/icon-update'))+[IO.Path]::DirectorySeparatorChar
if(!$backup.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Backup outside work/icon-update'}
$manifest=Join-Path $backup 'transaction.json'
$t=Get-Content -LiteralPath $manifest -Raw|ConvertFrom-Json
$statePath=Join-Path $root 'evidence/traffic-bridge-install.json'
$state=Get-Content -LiteralPath $statePath -Raw|ConvertFrom-Json
$oldStatePath=Join-Path $backup 'install-state-before.json'
$old=Get-Content -LiteralPath $oldStatePath -Raw|ConvertFrom-Json
$dll=Join-Path $old.TrafficDirectory 'plugins/VxeBattery.dll'
$config=Join-Path $old.TrafficDirectory 'config.ini'
if($t.Dll -ne $dll -or $t.Config -ne $config -or $state.Dll -ne $dll){throw 'Unexpected installation paths'}
$oldDll=Join-Path $backup 'VxeBattery-before.dll'
if((Get-FileHash -LiteralPath $oldDll).Hash -ne $t.OldDllHash){throw 'Backup DLL changed'}
if($Mode -eq 'apply'){
    if($t.Status -ne 'backed-up-running' -or $state.Version -ne $t.OldVersion -or $state.Mode -ne 'native'){throw 'Unexpected apply baseline'}
    $expected=$t.OldDllHash
    $source=Join-Path $root 'build/traffic-compact-x64/VxeBattery.dll'
    if((Get-FileHash -LiteralPath $source).Hash -ne $t.NewDllHash){throw 'Candidate DLL changed; revalidate'}
}else{
    if($t.Status -ne 'applied' -or $state.Version -ne '0.4.1-icons'){throw 'Unexpected rollback baseline'}
    $expected=$t.NewDllHash
    $source=$oldDll
}
if((Get-FileHash -LiteralPath $dll).Hash -ne $expected -or $state.DllHash -ne $expected){throw 'Installed DLL changed'}
Write-Host "Mode=$Mode; backup=$backup; configuration is not edited"
if(!$Apply){Write-Host 'Dry run only';return}
if(Get-Process -Name TrafficMonitor -ErrorAction SilentlyContinue){throw 'Exit TrafficMonitor normally before replacing DLL'}
$configHash=(Get-FileHash -LiteralPath $config).Hash
if($Mode -eq 'apply'){
    $configBackup=Join-Path $backup 'config-before.ini'
    if(Test-Path -LiteralPath $configBackup){throw 'Post-exit configuration backup already exists'}
    Copy-Item -LiteralPath $config -Destination $configBackup
}else{
    Copy-Item -LiteralPath $dll -Destination (Join-Path $backup 'VxeBattery-before-rollback.dll')
    Copy-Item -LiteralPath $statePath -Destination (Join-Path $backup 'install-state-before-rollback.json')
}
try{
    Copy-Item -LiteralPath $source -Destination $dll
    $targetHash=if($Mode -eq 'apply'){$t.NewDllHash}else{$t.OldDllHash}
    if((Get-FileHash -LiteralPath $dll).Hash -ne $targetHash){throw 'DLL replacement hash mismatch'}
    if((Get-FileHash -LiteralPath $config).Hash -ne $configHash){throw 'Configuration changed during replacement'}
    if($Mode -eq 'apply'){
        $state.DllHash=$t.NewDllHash;$state.Version='0.4.1-icons'
        $state.NativeSource='Windows native HID mouse/controller; M3/G5/E3 icon carousel; case provider pending'
        $state|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $statePath -Encoding utf8
        $t.Status='applied'
    }else{
        Copy-Item -LiteralPath $oldStatePath -Destination $statePath
        $t.Status='rolled-back'
    }
}catch{
    $restoreDll=if($Mode -eq 'apply'){$oldDll}else{Join-Path $backup 'VxeBattery-before-rollback.dll'}
    $restoreState=if($Mode -eq 'apply'){$oldStatePath}else{Join-Path $backup 'install-state-before-rollback.json'}
    Copy-Item -LiteralPath $restoreDll -Destination $dll
    Copy-Item -LiteralPath $restoreState -Destination $statePath
    $t.Status='failed-restored'
    $t|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $manifest -Encoding utf8
    throw
}
$t|Add-Member -NotePropertyName ConfigReplacementHash -NotePropertyValue $configHash -Force
$t|Add-Member -NotePropertyName AppliedAt -NotePropertyValue ([DateTimeOffset]::UtcNow.ToOffset([TimeSpan]::FromHours(8)).ToString('o')) -Force
$t|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $manifest -Encoding utf8
Write-Host "Completed $Mode; DLL verified; configuration hash unchanged"
