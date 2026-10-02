#Requires -Version 7.2
[CmdletBinding()]
param([ValidateSet('x64','x86')][string]$Architecture='x64')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if(!(Test-Path $vswhere)){throw 'vswhere not found'}
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'MSVC Build Tools not found'}
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$out=Join-Path $root "build/controller-probe-$Architecture"
New-Item -ItemType Directory -Force $out|Out-Null
$commands=@(
 "call `"$vc`" $Architecture >nul",
 "cl /nologo /std:c++17 /EHsc /utf-8 /W4 /O2 /MT `"$root/tests/controller_battery_model_test.cpp`" /Fe:controller_battery_model_test.exe",
 'if errorlevel 1 exit /b 1','controller_battery_model_test.exe','if errorlevel 1 exit /b 1',
 "cl /nologo /std:c++17 /EHsc /utf-8 /W4 /O2 /MT `"$root/tests/beitong_protocol_test.cpp`" /Fe:beitong_protocol_test.exe",
 'if errorlevel 1 exit /b 1','beitong_protocol_test.exe','if errorlevel 1 exit /b 1',
 "cl /nologo /std:c++17 /EHsc /utf-8 /W4 /O2 /MT `"$root/tools/beitong_native_probe.cpp`" /Fe:beitong_native_probe.exe /link setupapi.lib hid.lib",
 'if errorlevel 1 exit /b 1',
 "cl /nologo /std:c++17 /EHsc /utf-8 /W4 /O2 /MT /D_WIN32_WINNT=0x0A00 `"$root/tools/controller_probe.cpp`" /Fe:controller_probe.exe /link windowsapp.lib",
 'if errorlevel 1 exit /b 1'
)
$batch=Join-Path $out 'build.cmd'; $commands|Set-Content $batch -Encoding utf8
Push-Location $out
try { & $env:ComSpec /d /c "`"$batch`""; if($LASTEXITCODE){throw 'Controller probe build/tests failed'} }
finally { Pop-Location }
# Build runs only pure model tests, NEVER the live probe.
Get-FileHash (Join-Path $out 'controller_probe.exe') -Algorithm SHA256
