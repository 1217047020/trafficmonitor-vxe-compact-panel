#Requires -Version 7.2
[CmdletBinding()]
param([ValidateSet('x64','x86')][string]$Architecture='x64')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'MSVC Build Tools not found'}
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$out=Join-Path $root "build/beitong-preview-$Architecture"
New-Item -ItemType Directory -Force $out|Out-Null
$commands=@(
 "call `"$vc`" $Architecture >nul",
 "cl /nologo /std:c++17 /EHsc /utf-8 /W4 /O2 /MT `"$root/tests/beitong_protocol_test.cpp`" /Fe:beitong_protocol_test.exe",
 'if errorlevel 1 exit /b 1','beitong_protocol_test.exe','if errorlevel 1 exit /b 1',
 "cl /nologo /std:c++17 /EHsc /utf-8 /W4 /O2 /MT /LD /D_WIN32_WINNT=0x0602 `"$root/traffic-plugin/beitong_plugin.cpp`" /Fo:beitong_plugin.obj /link /DEF:`"$root/traffic-plugin/plugin.def`" /OUT:BeitongBattery.dll hid.lib setupapi.lib user32.lib",
 'if errorlevel 1 exit /b 1',
 "cl /nologo /std:c++17 /EHsc /utf-8 /W4 /O2 /MT `"$root/tests/beitong_plugin_test.cpp`" /Fe:beitong_plugin_test.exe",
 'if errorlevel 1 exit /b 1','beitong_plugin_test.exe BeitongBattery.dll','if errorlevel 1 exit /b 1'
)
$batch=Join-Path $out 'build.cmd'; $commands|Set-Content $batch -Encoding utf8
Push-Location $out
try { & $env:ComSpec /d /c "`"$batch`""; if($LASTEXITCODE){throw 'Beitong plugin build/tests failed'} }
finally { Pop-Location }
# Builds isolated candidate only; never installs, starts the worker, or changes host configuration.
Get-FileHash (Join-Path $out 'BeitongBattery.dll') -Algorithm SHA256
