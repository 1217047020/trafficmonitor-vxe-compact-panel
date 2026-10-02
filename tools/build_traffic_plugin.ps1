#Requires -Version 7.2
[CmdletBinding()]
param([ValidateSet('x64','x86')][string]$Architecture='x64')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs= & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'MSVC Build Tools not found' }
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$out=Join-Path $root "build/traffic-compact-$Architecture"
New-Item -ItemType Directory -Force $out | Out-Null
$src=Join-Path $root 'traffic-plugin'
$commands=@(
    "call `"$vc`" $Architecture >nul",
    "cl /nologo /std:c++17 /EHsc /utf-8 /O2 /MT /LD /D_WIN32_WINNT=0x0602 `"$src/plugin.cpp`" /Fo:plugin.obj /link /DEF:`"$src/plugin.def`" /OUT:VxeBattery.dll hid.lib setupapi.lib gdi32.lib user32.lib ws2_32.lib bthprops.lib bcrypt.lib",
    'if errorlevel 1 exit /b 1',
    "cl /nologo /std:c++17 /EHsc /utf-8 /O2 /MT `"$src/native_test.cpp`" /Fe:native_test.exe",
    'if errorlevel 1 exit /b 1', 'native_test.exe',
    'if errorlevel 1 exit /b 1',
    "cl /nologo /std:c++17 /EHsc /utf-8 /O2 /MT /D_WIN32_WINNT=0x0602 `"$src/native_probe.cpp`" /Fe:native_probe.exe /link hid.lib setupapi.lib",
    'if errorlevel 1 exit /b 1',
    "cl /nologo /std:c++17 /EHsc /utf-8 /O2 /MT `"$src/bridge_probe.cpp`" /Fe:plugin_probe.exe /link advapi32.lib",
    'if errorlevel 1 exit /b 1',
    "cl /nologo /std:c++17 /EHsc /utf-8 /O2 /MT `"$src/compact_test.cpp`" /Fe:compact_test.exe /link gdi32.lib user32.lib",
    'if errorlevel 1 exit /b 1', 'compact_test.exe VxeBattery.dll',
    'if errorlevel 1 exit /b 1',
    "cl /nologo /std:c++17 /EHsc /utf-8 /O2 /MT /D_WIN32_WINNT=0x0602 `"$root/pudding-plugin/native_test.cpp`" /Fe:pudding_protocol_test.exe",
    'if errorlevel 1 exit /b 1', 'pudding_protocol_test.exe', 'if errorlevel 1 exit /b 1',
    "cl /nologo /std:c++17 /EHsc /utf-8 /O2 /MT /D_WIN32_WINNT=0x0602 `"$root/tests/pudding_case_test.cpp`" /Fe:pudding_case_test.exe /link ws2_32.lib bthprops.lib bcrypt.lib",
    'if errorlevel 1 exit /b 1', 'pudding_case_test.exe VxeBattery.dll', 'if errorlevel 1 exit /b 1',
    "cl /nologo /std:c++17 /EHsc /utf-8 /O2 /MT `"$src/compact_preview.cpp`" /Fe:compact_preview.exe /link gdi32.lib user32.lib gdiplus.lib",
    'if errorlevel 1 exit /b 1'
)
$batch=Join-Path $out 'build.cmd'
$commands | Set-Content -LiteralPath $batch -Encoding utf8
Push-Location $out
try { & $env:ComSpec /d /c "`"$batch`""; if($LASTEXITCODE){throw 'Build or tests failed'} }
finally { Pop-Location }
Get-FileHash (Join-Path $out 'VxeBattery.dll') -Algorithm SHA256
