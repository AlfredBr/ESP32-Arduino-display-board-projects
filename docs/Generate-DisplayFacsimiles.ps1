[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$root = Split-Path $PSScriptRoot -Parent
$destination = Join-Path $PSScriptRoot 'images'
New-Item -ItemType Directory -Force -Path $destination | Out-Null

# Compose desktop renderer output into schematic display outlines.
# The frame represents the screen outline, not a particular board enclosure.
foreach ($project in @('Matrix', 'Fireworks', 'Breitling', 'Hyperspace', 'BlinkingEye', 'PieMenu', 'Hamster', 'wireframe_cube')) {
    $preview = if ($project -eq 'PieMenu') { 'PieMenu\previews\preview-4-pie.png' } else { "$project\preview.png" }
    $source = [System.Drawing.Image]::FromFile((Join-Path $root $preview))
    $bitmap = [System.Drawing.Bitmap]::new(480, 560)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $path = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $font = [System.Drawing.Font]::new('Segoe UI', 19)
    $small = [System.Drawing.Font]::new('Segoe UI', 10)
    $frameBrush = [System.Drawing.SolidBrush]::new([System.Drawing.ColorTranslator]::FromHtml('#263342'))
    $textBrush = [System.Drawing.SolidBrush]::new([System.Drawing.ColorTranslator]::FromHtml('#dce5ef'))
    $format = [System.Drawing.StringFormat]::new()
    try {
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
        $graphics.Clear([System.Drawing.ColorTranslator]::FromHtml('#101720'))
        if ($project -eq 'wireframe_cube') {
            # Native AMOLED aspect ratio, with the 288-square animation centered.
            $graphics.FillRectangle($frameBrush, 74, 26, 332, 404)
            $graphics.FillRectangle([System.Drawing.Brushes]::Black, 80, 32, 320, 392)
            $graphics.DrawImage($source, [System.Drawing.Rectangle]::new(114, 102, 252, 252))
            $graphics.DrawString('Cube', $small, [System.Drawing.Brushes]::White, 222, 382)
        } else {
        $graphics.FillEllipse($frameBrush, 38, 26, 404, 404)
        $graphics.FillEllipse([System.Drawing.Brushes]::Black, 48, 36, 384, 384)
        $path.AddEllipse(48, 36, 384, 384)
        $graphics.SetClip($path)
        $graphics.DrawImage($source, [System.Drawing.Rectangle]::new(48, 36, 384, 384))
        $graphics.ResetClip()
        }
        $format.Alignment = [System.Drawing.StringAlignment]::Center
        $title = switch ($project) {
            'BlinkingEye' { 'Blinking Eye' }
            'PieMenu' { 'Pie Menu' }
            'wireframe_cube' { 'Wireframe Shapes' }
            default { $project }
        }
        $hardware = if ($project -eq 'wireframe_cube') { '368 x 448 | ESP32-S3 / AMOLED' } else { '240 x 240 | ESP32-C3 / GC9A01' }
        $graphics.DrawString($title, $font, $textBrush, [System.Drawing.RectangleF]::new(0, 452, 480, 40), $format)
        $graphics.DrawString($hardware, $small, $textBrush, [System.Drawing.RectangleF]::new(0, 500, 480, 24), $format)
        $graphics.DrawString('Display facsimile - desktop-rendered frame', $small, $textBrush, [System.Drawing.RectangleF]::new(0, 525, 480, 24), $format)
        $bitmap.Save((Join-Path $destination ($project.ToLowerInvariant() + '-display.png')), [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $format.Dispose(); $textBrush.Dispose(); $frameBrush.Dispose()
        $small.Dispose(); $font.Dispose(); $path.Dispose()
        $graphics.Dispose(); $bitmap.Dispose(); $source.Dispose()
    }
}
