[CmdletBinding()]
param([switch]$Preview)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$temp = Join-Path ([System.IO.Path]::GetTempPath()) ('cube-tests-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $temp | Out-Null
try {
    $exe = Join-Path $temp 'CubeTests.exe'
    & g++ -std=c++17 -O2 -Wall -Wextra -Werror (Join-Path $PSScriptRoot 'tests\CubeTests.cpp') -o $exe
    if ($LASTEXITCODE -ne 0) { throw 'Native compilation failed.' }
    $bmp = Join-Path $temp 'preview.bmp'
    & $exe $bmp
    if ($LASTEXITCODE -ne 0) { throw 'Renderer tests failed.' }
    if ($Preview) {
        Add-Type -AssemblyName System.Drawing
        $image = [System.Drawing.Image]::FromFile($bmp)
        try { $image.Save((Join-Path $PSScriptRoot 'preview.png'), [System.Drawing.Imaging.ImageFormat]::Png) }
        finally { $image.Dispose() }
    }
} finally {
    Remove-Item -LiteralPath $temp -Recurse -Force
}
