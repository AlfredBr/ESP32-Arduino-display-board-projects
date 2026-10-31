# Builds and runs the desktop tests for the shared LevelMath.h with MSYS2 MinGW.
[CmdletBinding()]
param([string]$BuildPath = (Join-Path $env:TEMP 'level-host-build'))
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
New-Item -ItemType Directory -Force $BuildPath | Out-Null
$exe = Join-Path $BuildPath 'test_level.exe'
& C:/msys64/mingw64/bin/g++.exe -std=gnu++17 -Wall -Wextra -Werror -O1 -static -o $exe (Join-Path $PSScriptRoot 'tests\test_level.cpp')
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& $exe
exit $LASTEXITCODE
