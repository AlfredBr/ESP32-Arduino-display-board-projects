[CmdletBinding()]
param([switch]$Preview)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$temp = Join-Path ([System.IO.Path]::GetTempPath()) ('cube-tests-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $temp | Out-Null
try {
    $exe = Join-Path $temp 'CubeTests.exe'
    $bmp = Join-Path $temp 'preview.bmp'
    foreach ($hiddenLines in @(0, 1)) {
        & g++ -std=c++17 -O2 -Wall -Wextra -Werror "-DHIDDEN_LINE_REMOVAL=$hiddenLines" (Join-Path $PSScriptRoot 'tests\CubeTests.cpp') -o $exe
        if ($LASTEXITCODE -ne 0) { throw 'Native compilation failed.' }
        & $exe $bmp
        if ($LASTEXITCODE -ne 0) { throw 'Renderer tests failed.' }
        Write-Host "Hidden-line removal=$hiddenLines passed."
    }
    if ($Preview) {
        Add-Type -AssemblyName System.Drawing
        $image = [System.Drawing.Image]::FromFile($bmp)
        try { $image.Save((Join-Path $PSScriptRoot 'preview.png'), [System.Drawing.Imaging.ImageFormat]::Png) }
        finally { $image.Dispose() }
    }
} finally {
    Remove-Item -LiteralPath $temp -Recurse -Force
}
