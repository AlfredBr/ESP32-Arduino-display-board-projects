[CmdletBinding()]
param([string]$BuildPath = (Join-Path $env:TEMP 'PieMenuS3-host-build'))
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$testSource = Join-Path $PSScriptRoot 'tests'
& cmake -S $testSource -B $BuildPath -G 'MinGW Makefiles' -DCMAKE_C_COMPILER=C:/msys64/mingw64/bin/gcc.exe -DCMAKE_CXX_COMPILER=C:/msys64/mingw64/bin/g++.exe -DCMAKE_BUILD_TYPE=Debug
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& cmake --build $BuildPath --parallel 8
if ($LASTEXITCODE) { exit $LASTEXITCODE }
Push-Location $BuildPath
try {
    & ctest --output-on-failure
    $testExit = $LASTEXITCODE
} finally { Pop-Location }
exit $testExit
