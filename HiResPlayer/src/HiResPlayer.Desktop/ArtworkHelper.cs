using System;
using System.Collections.Concurrent;
using System.IO;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using HiResPlayer.Application.Services;

namespace HiResPlayer.Desktop
{
    /// <summary>
    /// Thread-safe artwork loader and decoder for FLAC embedded picture blocks and folder artwork.
    /// Freezes BitmapImage objects for maximum performance, multi-thread safety, and zero UI stutters.
    /// </summary>
    public static class ArtworkHelper
    {
        private static readonly ConcurrentDictionary<string, ImageSource?> _thumbCache = new(StringComparer.OrdinalIgnoreCase);
        private static readonly ConcurrentDictionary<string, ImageSource?> _fullCache = new(StringComparer.OrdinalIgnoreCase);

        /// <summary>
        /// Retrieves or decodes a 96px thumbnail for list rows and transport previews.
        /// </summary>
        public static ImageSource? GetThumbnail(string filePath, byte[]? artworkBytes = null)
        {
            if (string.IsNullOrEmpty(filePath)) return null;

            if (_thumbCache.TryGetValue(filePath, out var cached) && cached != null)
            {
                return cached;
            }

            byte[]? bytes = artworkBytes ?? TryExtractOrFindArtwork(filePath);
            if (bytes == null || bytes.Length == 0) return null;
            var decoded = DecodeImage(bytes, 96);
            if (decoded != null) _thumbCache[filePath] = decoded;
            return decoded;
        }

        /// <summary>
        /// Retrieves or decodes a 512px high-resolution artwork for the Now Playing view.
        /// </summary>
        public static ImageSource? GetFullArtwork(string filePath, byte[]? artworkBytes = null)
        {
            if (string.IsNullOrEmpty(filePath)) return null;

            if (_fullCache.TryGetValue(filePath, out var cached) && cached != null)
            {
                return cached;
            }

            byte[]? bytes = artworkBytes ?? TryExtractOrFindArtwork(filePath);
            if (bytes == null || bytes.Length == 0) return null;
            var decoded = DecodeImage(bytes, 512);
            if (decoded != null) _fullCache[filePath] = decoded;
            return decoded;
        }

        public static void CacheArtwork(string filePath, byte[]? artworkBytes)
        {
            if (string.IsNullOrEmpty(filePath) || artworkBytes == null || artworkBytes.Length == 0) return;

            var thumb = DecodeImage(artworkBytes, 96);
            if (thumb != null) _thumbCache[filePath] = thumb;

            var full = DecodeImage(artworkBytes, 512);
            if (full != null) _fullCache[filePath] = full;
        }

        public static byte[]? TryExtractOrFindArtwork(string filePath)
        {
            try
            {
                if (!File.Exists(filePath)) return null;

                // 1. Check folder artwork first (fastest)
                var folderArt = LibraryScanner.TryLoadFolderArtwork(filePath);
                if (folderArt != null && folderArt.Length > 0) return folderArt;

                // 2. Read FLAC picture metadata
                var meta = LibraryScanner.ReadFlacMetadata(filePath);
                return meta.ArtworkBytes;
            }
            catch
            {
                return null;
            }
        }

        private static ImageSource? DecodeImage(byte[] data, int decodeWidth)
        {
            try
            {
                using var ms = new MemoryStream(data);
                var bmp = new BitmapImage();
                bmp.BeginInit();
                bmp.CacheOption = BitmapCacheOption.OnLoad;
                bmp.CreateOptions = BitmapCreateOptions.IgnoreImageCache;
                if (decodeWidth > 0)
                {
                    bmp.DecodePixelWidth = decodeWidth;
                }
                bmp.StreamSource = ms;
                bmp.EndInit();
                bmp.Freeze(); // Critical for cross-thread access and rendering performance
                return bmp;
            }
            catch
            {
                return null;
            }
        }
    }
}
