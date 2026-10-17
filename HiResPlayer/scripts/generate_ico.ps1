Add-Type -AssemblyName System.Drawing

$srcPath = "d:\AudioPlayer\HiResPlayer\assets\app_icon.png"
$dstPng = "d:\AudioPlayer\HiResPlayer\assets\app_icon_transparent.png"
$dstIco = "d:\AudioPlayer\HiResPlayer\assets\app_icon.ico"

# Load source
$src = [System.Drawing.Bitmap]::FromFile($srcPath)

# Crop bounds: 174, 174, 676, 676
$cropRect = New-Object System.Drawing.Rectangle 174, 174, 676, 676
$targetSize = 512

# Create target bitmap
$cropped = New-Object System.Drawing.Bitmap $targetSize, $targetSize, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($cropped)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$g.Clear([System.Drawing.Color]::Transparent)

# Rounded rectangle clip path for the squircle rim
$pad = 6.0
$w = $targetSize - (2 * $pad)
$h = $targetSize - (2 * $pad)
$radius = 110.0
$path = New-Object System.Drawing.Drawing2D.GraphicsPath
$d = $radius * 2.0
$path.AddArc($pad, $pad, $d, $d, 180, 90)
$path.AddArc($pad + $w - $d, $pad, $d, $d, 270, 90)
$path.AddArc($pad + $w - $d, $pad + $h - $d, $d, $d, 0, 90)
$path.AddArc($pad, $pad + $h - $d, $d, $d, 90, 90)
$path.CloseFigure()

$g.SetClip($path)
$destRect = New-Object System.Drawing.Rectangle 0, 0, $targetSize, $targetSize
$g.DrawImage($src, $destRect, $cropRect.X, $cropRect.Y, $cropRect.Width, $cropRect.Height, [System.Drawing.GraphicsUnit]::Pixel)
$g.ResetClip()
$g.Dispose()
$src.Dispose()

$cropped.Save($dstPng, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Host "Cropped & masked PNG saved to $dstPng"

# Build multi-resolution ICO (256, 128, 64, 48, 32, 16)
$sizes = @(256, 128, 64, 48, 32, 16)
$pngBytesList = New-Object System.Collections.Generic.List[byte[]]

foreach ($s in $sizes) {
    $resized = New-Object System.Drawing.Bitmap $s, $s, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $gr = [System.Drawing.Graphics]::FromImage($resized)
    $gr.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $gr.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $gr.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $gr.Clear([System.Drawing.Color]::Transparent)
    $gr.DrawImage($cropped, 0, 0, $s, $s)
    $gr.Dispose()

    $ms = New-Object System.IO.MemoryStream
    $resized.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngBytesList.Add($ms.ToArray())
    $ms.Dispose()
    $resized.Dispose()
}

$cropped.Dispose()

# Write ICO binary
$fs = [System.IO.File]::Create($dstIco)
$bw = New-Object System.IO.BinaryWriter($fs)

$bw.Write([int16]0)              # Reserved
$bw.Write([int16]1)              # 1 = ICO
$bw.Write([int16]$sizes.Length)  # Image count

$offset = 6 + (16 * $sizes.Length)

for ($i = 0; $i -lt $sizes.Length; $i++) {
    $s = $sizes[$i]
    $w = if ($s -ge 256) { [byte]0 } else { [byte]$s }
    $h = if ($s -ge 256) { [byte]0 } else { [byte]$s }
    $bytes = $pngBytesList[$i]

    $bw.Write($w)                    # bWidth
    $bw.Write($h)                    # bHeight
    $bw.Write([byte]0)               # bColorCount
    $bw.Write([byte]0)               # bReserved
    $bw.Write([int16]1)              # wPlanes
    $bw.Write([int16]32)             # wBitCount
    $bw.Write([int32]$bytes.Length)  # dwBytesInRes
    $bw.Write([int32]$offset)        # dwImageOffset

    $offset += $bytes.Length
}

for ($i = 0; $i -lt $sizes.Length; $i++) {
    $bw.Write($pngBytesList[$i])
}

$bw.Flush()
$bw.Dispose()
$fs.Dispose()

Write-Host "Multi-resolution ICO successfully created at $dstIco"
