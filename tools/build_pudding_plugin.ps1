#Requires -Version 7.2
[CmdletBinding()]
param([ValidateSet('x64','x86')][string]$Architecture='x64')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'MSVC Build Tools not found'}
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$out=Join-Path $root "build/pudding-$Architecture"
New-Item -ItemType Directory -Force $out | Out-Null
$src=Join-Path $root 'pudding-plugin'
$commands=@(
    "call `"$vc`" $Architecture >nul",
    "cl /nologo /std:c++17 /EHsc /utf-8 /O2 /MT /W4 /WX /LD /D_WIN32_WINNT=0x0602 `"$src/plugin.cpp`" /Fo:plugin.obj /link /DEF:`"$src/plugin.def`" /OUT:PuddingBattery.dll ws2_32.lib bthprops.lib bcrypt.lib",
    'if errorlevel 1 exit /b 1',
    "cl /nologo /std:c++17 /EHsc /utf-8 /O2 /MT /W4 /WX `"$src/native_test.cpp`" /Fe:native_test.exe",
    'if errorlevel 1 exit /b 1', 'native_test.exe', 'if errorlevel 1 exit /b 1',
    "cl /nologo /std:c++17 /EHsc /utf-8 /O2 /MT /W4 /WX `"$src/plugin_test.cpp`" /Fe:plugin_test.exe",
    'if errorlevel 1 exit /b 1', 'plugin_test.exe PuddingBattery.dll', 'if errorlevel 1 exit /b 1',
    "cl /nologo /std:c++17 /EHsc /utf-8 /O2 /MT /W4 /WX /D_WIN32_WINNT=0x0602 `"$src/native_probe.cpp`" /Fe:native_probe.exe /link ws2_32.lib bthprops.lib bcrypt.lib",
    'if errorlevel 1 exit /b 1'
)
$batch=Join-Path $out 'build.cmd'
$commands | Set-Content -LiteralPath $batch -Encoding utf8
Push-Location $out
try {& $env:ComSpec /d /c "`"$batch`"";if($LASTEXITCODE){throw 'Pudding build or tests failed'}}
finally {Pop-Location}
Get-FileHash (Join-Path $out 'PuddingBattery.dll') -Algorithm SHA256
