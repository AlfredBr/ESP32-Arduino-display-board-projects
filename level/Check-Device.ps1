# Run after uploading. Resets the board, checks that the IMU answered, and
# prints a second of streamed readings. Lay the board flat for a sane result.
[CmdletBinding()]
param([string]$Port = 'COM5')
$ErrorActionPreference = 'Stop'
$serialPort = [System.IO.Ports.SerialPort]::new($Port,115200)
$serialPort.ReadTimeout = 1000
try {
    $serialPort.Open()
    # Pulse DTR/RTS so the board restarts and prints its start-up line.
    $serialPort.DtrEnable = $false; $serialPort.RtsEnable = $true
    Start-Sleep -Milliseconds 100
    $serialPort.RtsEnable = $false
    Start-Sleep -Milliseconds 2500
    $banner = $serialPort.ReadExisting()
    Write-Host $banner.Trim()
    $serialPort.Write('s')
    Start-Sleep -Milliseconds 1100
    $serialPort.Write('s')
    $readings = $serialPort.ReadExisting()
    Write-Host $readings.Trim()
    if ($readings -notmatch 'acc ') { throw 'No accelerometer readings streamed.' }
    if ($banner -and $banner -notmatch 'IMU detected') { throw 'The board reported no IMU.' }
    Write-Host 'Device check passed: accelerometer readings are streaming.'
} finally {
    if ($serialPort.IsOpen) { $serialPort.Close() }
    $serialPort.Dispose()
}
