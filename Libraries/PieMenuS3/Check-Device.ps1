# Run after uploading. Sends each demo slice-count command and checks its reply.
[CmdletBinding()]
param([string]$Port = 'COM5')
$ErrorActionPreference = 'Stop'
$serialPort = [System.IO.Ports.SerialPort]::new($Port,115200)
$serialPort.ReadTimeout = 1000
try {
    $serialPort.Open()
    Start-Sleep -Milliseconds 750
    $serialPort.DiscardInBuffer()
    for ($slices=2; $slices -le 8; $slices++) {
        $serialPort.Write([string]$slices)
        Start-Sleep -Milliseconds 1000
        $reply = $serialPort.ReadExisting()
        Write-Host $reply.Trim()
        if ($reply -notmatch "$slices slices") { throw "Missing reply for $slices slices." }
    }
    $serialPort.Write('4') # Leave the four-slice page ready for touch.
    Start-Sleep -Milliseconds 500
    Write-Host $serialPort.ReadExisting().Trim()
    Write-Host 'Device smoke check passed: all seven pages rebuilt successfully.'
} finally {
    if ($serialPort.IsOpen) { $serialPort.Close() }
    $serialPort.Dispose()
}
