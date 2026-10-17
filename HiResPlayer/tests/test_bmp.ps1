Add-Type -AssemblyName PresentationCore, WindowsBase
Add-Type -Path 'd:\AudioPlayer\HiResPlayer\src\HiResPlayer.Application\bin\Debug\net10.0-windows\HiResPlayer.Application.dll'
$meta = [HiResPlayer.Application.Services.LibraryScanner]::ReadFlacMetadata('D:\Songs\A.R. Rahman - Aao Balma (From Coke Studio @ MTV Se.flac')
Write-Host "Title: $($meta.Title)"
Write-Host "Artist: $($meta.Artist)"
Write-Host "Album: $($meta.Album)"
Write-Host "Art len: $($meta.ArtworkBytes.Length)"
$ms = New-Object System.IO.MemoryStream(,$meta.ArtworkBytes)
$bmp = New-Object System.Windows.Media.Imaging.BitmapImage
$bmp.BeginInit()
$bmp.CacheOption = [System.Windows.Media.Imaging.BitmapCacheOption]::OnLoad
$bmp.DecodePixelWidth = 512
$bmp.StreamSource = $ms
$bmp.EndInit()
$bmp.Freeze()
Write-Host "Success! PixelWidth: $($bmp.PixelWidth) PixelHeight: $($bmp.PixelHeight)"
