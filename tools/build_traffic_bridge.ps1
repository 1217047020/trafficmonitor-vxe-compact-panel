#Requires -Version 7.2
[CmdletBinding()]
param([ValidateSet('x64','x86')][string]$Architecture='x64')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs= & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'MSVC Build Tools not found' }
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$out=Join-Path $root "build/traffic-$Architecture"
New-Item -ItemType Directory -Force $out | Out-Null
$src=Join-Path $root 'traffic-plugin'
$commands=@(
    "call `"$vc`" $Architecture >nul",
    "cl /nologo /std:c++17 /EHsc /utf-8 /O2 /MT /LD /D_WIN32_WINNT=0x0602 `"$src/bridge-plugin.cpp`" /Fo:plugin.obj /link /DEF:`"$src/plugin.def`" /OUT:VxeBattery.dll winhttp.lib advapi32.lib",
    'if errorlevel 1 exit /b 1',
    "cl /nologo /std:c++17 /EHsc /utf-8 /O2 /MT `"$src/state_test.cpp`" /Fe:state_test.exe",
    'if errorlevel 1 exit /b 1', 'state_test.exe',
    'if errorlevel 1 exit /b 1',
    "cl /nologo /std:c++17 /EHsc /utf-8 /O2 /MT `"$src/bridge_probe.cpp`" /Fe:bridge_probe.exe /link advapi32.lib"
)
$batch=Join-Path $out 'build.cmd'
$commands | Set-Content -LiteralPath $batch -Encoding utf8
Push-Location $out
try { & $env:ComSpec /d /c "`"$batch`""; if($LASTEXITCODE){throw 'Build or tests failed'} }
finally { Pop-Location }
Get-FileHash (Join-Path $out 'VxeBattery.dll') -Algorithm SHA256
