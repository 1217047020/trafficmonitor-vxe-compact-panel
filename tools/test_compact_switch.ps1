#Requires -Version 7.2
# Fixture-only test of the unchanged production enable/rollback script.
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$fixture=Join-Path $root ('work/compact-installer-tests/'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Force $fixture | Out-Null
foreach($dir in 'tools','evidence','build/traffic-compact-x64','host/plugins','work/compact-switch'){
    New-Item -ItemType Directory -Force (Join-Path $fixture $dir) | Out-Null
}
foreach($file in 'set_compact_panel.ps1','compact_config.py','traffic_config.py'){
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination (Join-Path $fixture "tools/$file")
}
$hostDir=Join-Path $fixture 'host'
$config=Join-Path $hostDir 'config.ini'
$dll=Join-Path $hostDir 'plugins/VxeBattery.dll'
# No interaction with the real host: synthetic configuration and an inert
# old-DLL byte fixture test the copy/hash/rollback transaction, never loading it.
Copy-Item -LiteralPath (Join-Path $root 'tests/fixtures/traffic-config.ini') -Destination $config
[IO.File]::WriteAllBytes($dll,[Text.Encoding]::UTF8.GetBytes('fixture-native-v0.2-not-executable'))
Copy-Item -LiteralPath (Join-Path $root 'build/traffic-compact-x64/VxeBattery.dll') -Destination (Join-Path $fixture 'build/traffic-compact-x64/VxeBattery.dll')
$metadata=@{TrafficDirectory=$hostDir;Config=$config;Dll=$dll;DllHash=(Get-FileHash $dll).Hash;Mode='native';Version='0.2.0'}
$state=Join-Path $fixture 'evidence/traffic-bridge-install.json'
$metadata | ConvertTo-Json | Set-Content -LiteralPath $state -Encoding utf8
$beforeConfig=(Get-FileHash $config).Hash;$beforeDll=(Get-FileHash $dll).Hash
$script=Join-Path $fixture 'tools/set_compact_panel.ps1'
$backup=Join-Path $fixture 'work/compact-switch/test'
function Assert($condition,$message){if(!$condition){throw "FAILED: $message"}}
& $script
Assert ((Get-FileHash $config).Hash -eq $beforeConfig) 'dry run changed config'
Assert ((Get-FileHash $dll).Hash -eq $beforeDll) 'dry run changed DLL'
$refused=$false
try{& $script -Apply}catch{$refused=$true}
Assert $refused 'missing visual confirmation was accepted'
# The fixture has no host process. Shadow only process lookup in this test shell;
# this does not affect the running TrafficMonitor or modify production script.
function Get-Process { param($Name,$ErrorAction) return $null }
try{
    & $script -Apply -VisualConfirmed -BackupDirectory $backup
    Assert ((Get-Content -Raw $state | ConvertFrom-Json).Version -eq '0.3.0') 'metadata enable failed'
    Assert ((Get-FileHash $dll).Hash -ne $beforeDll) 'DLL enable failed'
    & $script -Mode rollback -BackupDirectory $backup -Apply
    Assert ((Get-FileHash $dll).Hash -eq $beforeDll) 'DLL round trip failed'
    Assert ((Get-FileHash $config).Hash -eq $beforeConfig) 'config byte-exact round trip failed'
    Assert ((Get-Content -Raw $state | ConvertFrom-Json).Version -eq '0.2.0') 'metadata rollback failed'
}finally{Remove-Item Function:\Get-Process}
Write-Host 'Fixture enable/rollback passed; real installed DLL/config/host untouched.'
Write-Host "Fixture retained: $fixture"
