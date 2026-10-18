using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace HiResPlayer.Application.Services
{
    public record TrackMetadata(
        string FilePath,
        string Title,
        string Artist,
        string Album,
        string AlbumArtist,
        uint TrackNumber,
        uint SampleRate,
        uint BitsPerSample,
        uint Channels,
        ulong TotalSamples,
        TimeSpan Duration,
        byte[]? ArtworkBytes = null,
        string Genre = "Unknown Genre",
        string Composer = ""
    );

    public class LibraryScanner
    {
        public event Action<int, int, string>? ProgressChanged;

        public async Task<List<TrackMetadata>> ScanFolderAsync(
            string folderPath,
            CancellationToken cancellationToken = default)
        {
            var results = new List<TrackMetadata>();
            if (!Directory.Exists(folderPath)) return results;

            await Task.Run(() =>
            {
                var flacFiles = new List<string>();
                EnumerateFilesResilient(folderPath, "*.flac", flacFiles, cancellationToken);
                int total = flacFiles.Count;
                int current = 0;

                foreach (var file in flacFiles)
                {
                    if (cancellationToken.IsCancellationRequested) break;

                    try
                    {
                        var meta = ReadFlacBasicMetadata(file);
                        results.Add(meta);
                    }
                    catch
                    {
                        // Safely skip corrupted files without aborting whole scan
                    }

                    current++;
                    ProgressChanged?.Invoke(current, total, file);
                }
            }, cancellationToken);

            return results;
        }

        private static void EnumerateFilesResilient(
            string root,
            string searchPattern,
            List<string> results,
            CancellationToken cancellationToken)
        {
            if (cancellationToken.IsCancellationRequested) return;

            try
            {
                var files = Directory.GetFiles(root, searchPattern);
                results.AddRange(files);
            }
            catch (UnauthorizedAccessException) { /* Skip inaccessible directory */ }
            catch (DirectoryNotFoundException) { /* Skip missing directory */ }
            catch (PathTooLongException) { /* Skip overlong path */ }
            catch (IOException) { /* Skip IO errors */ }

            try
            {
                var subdirs = Directory.GetDirectories(root);
                foreach (var subdir in subdirs)
                {
                    if (cancellationToken.IsCancellationRequested) break;
                    try
                    {
                        var dirInfo = new DirectoryInfo(subdir);
                        if ((dirInfo.Attributes & FileAttributes.ReparsePoint) != 0) continue;
                    }
                    catch { }

                    EnumerateFilesResilient(subdir, searchPattern, results, cancellationToken);
                }
            }
            catch (UnauthorizedAccessException) { }
            catch (DirectoryNotFoundException) { }
            catch (PathTooLongException) { }
            catch (IOException) { }
        }

        private static TrackMetadata ReadFlacBasicMetadata(string filePath)
        {
            return ReadFlacMetadata(filePath);
        }

        public static TrackMetadata ReadFlacMetadata(string filePath)
        {
            string title = Path.GetFileNameWithoutExtension(filePath);
            string artist = "Unknown Artist";
            string album = "Unknown Album";
            string albumArtist = "Unknown Artist";
            string genre = "Unknown Genre";
            string composer = "";
            uint trackNumber = 1;

            uint sampleRate = 44100;
            uint bitsPerSample = 16;
            uint channels = 2;
            ulong totalSamples = 0;
            byte[]? artwork = null;

            try
            {
                using var fs = new FileStream(filePath, FileMode.Open, FileAccess.Read, FileShare.Read);
                byte[] magic = new byte[4];
                if (fs.Read(magic, 0, 4) == 4 &&
                    magic[0] == 0x66 && magic[1] == 0x4C && magic[2] == 0x61 && magic[3] == 0x43) // "fLaC"
                {
                    bool isLast = false;
                    byte[] header = new byte[4];

                    while (!isLast && fs.Position < fs.Length)
                    {
                        if (fs.Read(header, 0, 4) < 4) break;

                        isLast = (header[0] & 0x80) != 0;
                        int blockType = header[0] & 0x7F;
                        int blockLength = (header[1] << 16) | (header[2] << 8) | header[3];

                        if (blockLength <= 0 || fs.Position + blockLength > fs.Length) break;

                        switch (blockType)
                        {
                            case 0: // STREAMINFO (34 bytes)
                            {
                                byte[] info = new byte[blockLength];
                                fs.ReadExactly(info, 0, blockLength);
                                if (blockLength >= 18)
                                {
                                    sampleRate = ((uint)info[10] << 12) | ((uint)info[11] << 4) | ((uint)info[12] >> 4);
                                    channels = (((uint)info[12] >> 1) & 0x07) + 1;
                                    bitsPerSample = ((((uint)info[12] & 0x01) << 4) | ((uint)info[13] >> 4)) + 1;
                                    totalSamples = (((ulong)info[13] & 0x0F) << 32) |
                                                   ((ulong)info[14] << 24) |
                                                   ((ulong)info[15] << 16) |
                                                   ((ulong)info[16] << 8) |
                                                   (ulong)info[17];
                                }
                                break;
                            }
                            case 4: // VORBIS_COMMENT
                            {
                                byte[] commentData = new byte[blockLength];
                                fs.ReadExactly(commentData, 0, blockLength);
                                ParseVorbisComment(commentData, ref title, ref artist, ref album, ref albumArtist, ref genre, ref composer, ref trackNumber);
                                break;
                            }
                            case 6: // PICTURE
                            {
                                if (blockLength > 32)
                                {
                                    byte[] picData = new byte[blockLength];
                                    fs.ReadExactly(picData, 0, blockLength);
                                    var (picType, picBytes) = ParsePictureBlock(picData);
                                    if (picBytes != null)
                                    {
                                        if (artwork == null || picType == 3) // 3 = Cover Front
                                        {
                                            artwork = picBytes;
                                        }
                                    }
                                }
                                else
                                {
                                    fs.Seek(blockLength, SeekOrigin.Current);
                                }
                                break;
                            }
                            default:
                                fs.Seek(blockLength, SeekOrigin.Current);
                                break;
                        }
                    }
                }
            }
            catch
            {
                // Fallback to basic metadata
            }

            // Smart Fallback for Artist / Title / Album if Vorbis comments were missing or default
            if (string.IsNullOrWhiteSpace(artist) || artist.Equals("Unknown Artist", StringComparison.OrdinalIgnoreCase))
            {
                string fileName = Path.GetFileNameWithoutExtension(filePath);
                var parts = fileName.Split(new[] { " - " }, StringSplitOptions.RemoveEmptyEntries);
                if (parts.Length >= 2)
                {
                    string p0 = parts[0].Trim().TrimStart('0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.', ' ');
                    if (string.IsNullOrEmpty(p0) && parts.Length >= 3)
                    {
                        artist = parts[1].Trim();
                        if (title == fileName || string.IsNullOrWhiteSpace(title)) title = parts[2].Trim();
                    }
                    else
                    {
                        artist = parts[0].Trim();
                        if (title == fileName || string.IsNullOrWhiteSpace(title)) title = parts[1].Trim();
                    }
                }
                else
                {
                    var dir = Path.GetDirectoryName(filePath);
                    if (!string.IsNullOrEmpty(dir))
                    {
                        var parent = Path.GetFileName(dir);
                        var grandParent = Path.GetFileName(Path.GetDirectoryName(dir));
                        if (!string.IsNullOrEmpty(grandParent) &&
                            !grandParent.Equals("Music", StringComparison.OrdinalIgnoreCase) &&
                            !grandParent.Contains(":") &&
                            !grandParent.Equals("Users", StringComparison.OrdinalIgnoreCase))
                        {
                            artist = grandParent;
                            if (album == "Unknown Album") album = parent;
                        }
                        else if (!string.IsNullOrEmpty(parent) &&
                                 !parent.Equals("Music", StringComparison.OrdinalIgnoreCase) &&
                                 !parent.Contains(":"))
                        {
                            artist = parent;
                        }
                    }
                }
            }

            // Clean up leading track number from title if needed (e.g. "04. Track Title" -> "Track Title")
            if (!string.IsNullOrEmpty(title) && char.IsDigit(title[0]))
            {
                int sepIdx = title.IndexOfAny(new[] { '.', '-', ' ' });
                if (sepIdx > 0 && sepIdx <= 3)
                {
                    string candidate = title.Substring(sepIdx + 1).Trim().TrimStart('-', '.', ' ');
                    if (!string.IsNullOrWhiteSpace(candidate))
                    {
                        title = candidate;
                    }
                }
            }

            // Fallback: check folder artwork if no embedded PICTURE block was found
            if (artwork == null)
            {
                artwork = TryLoadFolderArtwork(filePath);
            }

            TimeSpan duration = sampleRate > 0 && totalSamples > 0
                ? TimeSpan.FromSeconds((double)totalSamples / sampleRate)
                : TimeSpan.Zero;

            return new TrackMetadata(
                FilePath: filePath,
                Title: title,
                Artist: artist,
                Album: album,
                AlbumArtist: albumArtist,
                TrackNumber: trackNumber,
                SampleRate: sampleRate,
                BitsPerSample: bitsPerSample,
                Channels: channels,
                TotalSamples: totalSamples,
                Duration: duration,
                ArtworkBytes: artwork,
                Genre: genre,
                Composer: composer
            );
        }

        private static void ParseVorbisComment(
            byte[] data,
            ref string title,
            ref string artist,
            ref string album,
            ref string albumArtist,
            ref string genre,
            ref string composer,
            ref uint trackNumber)
        {
            if (data.Length < 8) return;
            int offset = 0;

            int vendorLen = BitConverter.ToInt32(data, offset);
            offset += 4;
            if (offset + vendorLen > data.Length || vendorLen < 0) return;
            offset += vendorLen;

            if (offset + 4 > data.Length) return;
            int commentCount = BitConverter.ToInt32(data, offset);
            offset += 4;

            for (int i = 0; i < commentCount && offset + 4 <= data.Length; i++)
            {
                int len = BitConverter.ToInt32(data, offset);
                offset += 4;
                if (len < 0 || offset + len > data.Length) break;

                string comment = System.Text.Encoding.UTF8.GetString(data, offset, len);
                offset += len;

                int eqIdx = comment.IndexOf('=');
                if (eqIdx > 0)
                {
                    string key = comment.Substring(0, eqIdx).Trim().ToUpperInvariant();
                    string val = comment.Substring(eqIdx + 1).Trim();
                    if (!string.IsNullOrEmpty(val))
                    {
                        switch (key)
                        {
                            case "TITLE": title = val; break;
                            case "ARTIST": artist = val; break;
                            case "ALBUM": album = val; break;
                            case "ALBUMARTIST": albumArtist = val; break;
                            case "GENRE": genre = val; break;
                            case "COMPOSER": composer = val; break;
                            case "WRITER": if (string.IsNullOrEmpty(composer)) composer = val; break;
                            case "TRACKNUMBER":
                                if (uint.TryParse(val.Split('/')[0], out uint tn)) trackNumber = tn;
                                break;
                        }
                    }
                }
            }
        }

        private static (int PicType, byte[]? Bytes) ParsePictureBlock(byte[] data)
        {
            if (data.Length < 32) return (0, null);
            int offset = 0;

            // 4 bytes: picture type (3 = Cover Front)
            int picType = ReadInt32BigEndian(data, offset);
            offset += 4;

            // 4 bytes: MIME length
            if (offset + 4 > data.Length) return (picType, null);
            int mimeLen = ReadInt32BigEndian(data, offset);
            offset += 4;
            if (mimeLen < 0 || offset + mimeLen > data.Length) return (picType, null);
            offset += mimeLen;

            // 4 bytes: Description length
            if (offset + 4 > data.Length) return (picType, null);
            int descLen = ReadInt32BigEndian(data, offset);
            offset += 4;
            if (descLen < 0 || offset + descLen > data.Length) return (picType, null);
            offset += descLen;

            // Width (4), Height (4), Depth (4), Colors (4) = 16 bytes
            offset += 16;
            if (offset + 4 > data.Length) return (picType, null);

            int dataLen = ReadInt32BigEndian(data, offset);
            offset += 4;
            if (dataLen <= 0 || offset + dataLen > data.Length) return (picType, null);

            byte[] picBytes = new byte[dataLen];
            Buffer.BlockCopy(data, offset, picBytes, 0, dataLen);
            return (picType, picBytes);
        }

        public static byte[]? TryLoadFolderArtwork(string filePath)
        {
            try
            {
                string? dir = Path.GetDirectoryName(filePath);
                if (string.IsNullOrEmpty(dir) || !Directory.Exists(dir)) return null;

                string[] commonNames = { "cover.jpg", "cover.png", "cover.jpeg", "folder.jpg", "folder.png", "folder.jpeg", "front.jpg", "front.png", "front.jpeg", "album.jpg", "album.png", "albumart.jpg", "albumart.png", "art.jpg", "art.png" };
                foreach (var name in commonNames)
                {
                    string candidate = Path.Combine(dir, name);
                    if (File.Exists(candidate))
                    {
                        return File.ReadAllBytes(candidate);
                    }
                }

                // Fallback: search for any image in folder
                var files = Directory.GetFiles(dir, "*.*");
                foreach (var f in files)
                {
                    string ext = Path.GetExtension(f);
                    if (ext.Equals(".jpg", StringComparison.OrdinalIgnoreCase) ||
                        ext.Equals(".jpeg", StringComparison.OrdinalIgnoreCase) ||
                        ext.Equals(".png", StringComparison.OrdinalIgnoreCase))
                    {
                        return File.ReadAllBytes(f);
                    }
                }
            }
            catch
            {
                // Ignore folder scan errors
            }
            return null;
        }

        private static int ReadInt32BigEndian(byte[] buf, int offset)
        {
            return (buf[offset] << 24) | (buf[offset + 1] << 16) | (buf[offset + 2] << 8) | buf[offset + 3];
        }
    }
}
