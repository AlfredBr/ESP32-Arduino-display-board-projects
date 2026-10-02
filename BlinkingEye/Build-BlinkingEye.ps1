<#
.SYNOPSIS
Compiles BlinkingEye and optionally uploads it to the ESP32-C3.
.EXAMPLE
.\Build-BlinkingEye.ps1
.EXAMPLE
.\Build-BlinkingEye.ps1 -Upload -Port COM3
.EXAMPLE
.\Build-BlinkingEye.ps1 -Upload -Port COM3 -DryRun
#>
[CmdletBinding()]
param(
    [string]$SketchPath = '',
    [string]$CliPath = 'C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe',
    [string]$ConfigFile = (Join-Path $env:USERPROFILE '.arduinoIDE\arduino-cli.yaml'),
    [string]$Fqbn = 'esp32:esp32:esp32c3:CDCOnBoot=cdc',
    [string]$Port = 'COM3',
    [switch]$Upload,
    [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
# Handle native exit codes explicitly, including in PowerShell 7 sessions
# where the caller enabled native-command error handling.
$PSNativeCommandUseErrorActionPreference = $false

try {
    if ([string]::IsNullOrWhiteSpace($SketchPath)) {
        $SketchPath = Split-Path -Parent $MyInvocation.MyCommand.Path
    }
    foreach ($requiredFile in @($CliPath, $ConfigFile)) {
        if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
            throw "Required file not found: $requiredFile"
        }
    }

    $sketchItem = Get-Item -LiteralPath $SketchPath
    if (-not $sketchItem.PSIsContainer) {
        if ($sketchItem.Extension -ne '.ino') {
            throw 'SketchPath must be a sketch folder or an .ino file.'
        }
        $sketchItem = $sketchItem.Directory
    }
    $resolvedSketch = $sketchItem.FullName
    $mainSketch = Join-Path $resolvedSketch ($sketchItem.Name + '.ino')
    if (-not (Test-Path -LiteralPath $mainSketch -PathType Leaf)) {
        throw "Arduino requires a matching folder and sketch name. Missing: $mainSketch"
    }
    if ($Upload -and [string]::IsNullOrWhiteSpace($Port)) {
        throw 'Supply an upload port, for example -Port COM3.'
    }

    # A single compile --upload invocation uploads only after compilation succeeds.
    # CDCOnBoot=cdc routes Serial output through the board's USB connection.
    $cliArguments = @(
        'compile',
        '--fqbn', $Fqbn,
        '--config-file', $ConfigFile
    )
    if ($Upload) {
        $cliArguments += @('--upload', '--port', $Port)
    }
    $cliArguments += $resolvedSketch

    # Display a copyable PowerShell command. Execute with an argument array,
    # never by evaluating the displayed string.
    $displayParts = @($CliPath) + $cliArguments
    $quotedParts = foreach ($part in $displayParts) {
        "'" + $part.Replace("'", "''") + "'"
    }
    Write-Host ('& ' + ($quotedParts -join ' '))
    if ($DryRun) {
        Write-Host 'Dry run: no compilation or upload performed.'
        exit 0
    }

    if ($Upload) {
        Write-Host "Compiling and uploading to $Port. Close Serial Monitor if the port is busy."
    } else {
        Write-Host 'Compiling only. Use -Upload to also flash the board.'
    }

    & $CliPath @cliArguments
    $cliExitCode = $LASTEXITCODE
    if ($cliExitCode -ne 0) {
        [Console]::Error.WriteLine("Arduino CLI failed (exit code $cliExitCode). See its output above.")
        exit $cliExitCode
    }

    if ($Upload) {
        Write-Host 'Compile and upload succeeded. The board should restart and run the sketch.'
    } else {
        Write-Host 'Compilation succeeded. Nothing was uploaded.'
    }
    exit 0
} catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}



