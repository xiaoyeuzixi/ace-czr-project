[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$sourceRoot = Split-Path -Parent $PSScriptRoot
$desktopRoot = Split-Path -Parent (Split-Path -Parent $sourceRoot)
$packageRoot = Join-Path $desktopRoot "04_NoACE一键启动程序"
$bin = Join-Path $packageRoot "bin"
$vcvars = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
$stubOut = Join-Path $bin "combined_stub.dll"
$launcherOut = Join-Path $bin "NoAceUnityLauncher.exe"
$command = 'call "{0}" && cd /d "{1}" && cl /nologo /O2 /EHsc /utf-8 combined_stub.cpp /link /DEF:combined_stub.def /MACHINE:X64 /OUT:"{2}" && cl /nologo /O2 /EHsc /utf-8 NoAceUnityLauncher.cpp /link /SUBSYSTEM:WINDOWS /MACHINE:X64 user32.lib /OUT:"{3}"' -f $vcvars,$PSScriptRoot,$stubOut,$launcherOut
cmd.exe /d /s /c $command
