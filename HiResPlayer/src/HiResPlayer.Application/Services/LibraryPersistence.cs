using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;

namespace HiResPlayer.Application.Services
{
    /// <summary>
    /// Persists the track library to JSON in AppData\Roaming\HiResPlayer\library.json.
    /// Provides save, load, and auto-discovery of FLAC files on common music paths.
    /// </summary>
    public class LibraryPersistence
    {
        private static readonly string _defaultStorePath = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "HiResPlayer",
            "library.json"
        );

        private readonly string _storePath;

        public LibraryPersistence(string? customPath = null)
        {
            _storePath = customPath ?? _defaultStorePath;
        }

        private readonly JsonSerializerOptions _jsonOpts = new()
        {
            WriteIndented = true,
            DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
        };

        public void Save(IEnumerable<PersistedTrack> tracks)
        {
            try
            {
                Directory.CreateDirectory(Path.GetDirectoryName(_storePath)!);
                string json = JsonSerializer.Serialize(new PersistedLibrary { Tracks = new List<PersistedTrack>(tracks) }, _jsonOpts);
                File.WriteAllText(_storePath, json);
            }
            catch
            {
                // Never crash the app because of persistence failure
            }
        }

        public List<PersistedTrack> Load()
        {
            try
            {
                if (!File.Exists(_storePath)) return new();
                string json = File.ReadAllText(_storePath);
                var lib = JsonSerializer.Deserialize<PersistedLibrary>(json, _jsonOpts);
                return lib?.Tracks ?? new();
            }
            catch
            {
                return new();
            }
        }

        /// <summary>
        /// Scans well-known music directories (Music folder, common paths) for FLAC files
        /// and returns any that are not already in the provided known-paths set.
        /// </summary>
        public async Task<List<PersistedTrack>> AutoDiscoverAsync(
            ISet<string> alreadyKnown,
            IProgress<(int current, int total, string file)>? progress = null,
            CancellationToken ct = default)
        {
            var searchRoots = new List<string>();

            // Always check the user's Music library
            string myMusic = Environment.GetFolderPath(Environment.SpecialFolder.MyMusic);
            if (Directory.Exists(myMusic)) searchRoots.Add(myMusic);

            // Common external/portable media roots
            foreach (var drive in DriveInfo.GetDrives())
            {
                if (drive.DriveType == DriveType.Removable && drive.IsReady)
                    searchRoots.Add(drive.RootDirectory.FullName);
            }

            var results = new List<PersistedTrack>();

            await Task.Run(() =>
            {
                var allFiles = new List<string>();
                foreach (var root in searchRoots)
                {
                    try
                    {
                        allFiles.AddRange(Directory.GetFiles(root, "*.flac", SearchOption.AllDirectories));
                    }
                    catch { /* Skip inaccessible paths */ }
                }

                int total = allFiles.Count;
                int current = 0;
                foreach (var file in allFiles)
                {
                    if (ct.IsCancellationRequested) break;
                    current++;
                    if (alreadyKnown.Contains(file)) { progress?.Report((current, total, file)); continue; }

                    try
                    {
                        var meta = LibraryScanner.ReadFlacMetadata(file);
                        results.Add(new PersistedTrack
                        {
                            FilePath = file,
                            Title = meta.Title,
                            Artist = meta.Artist,
                            Album = meta.Album,
                            AlbumArtist = meta.AlbumArtist,
                            Genre = meta.Genre,
                            Composer = meta.Composer,
                            SampleRate = meta.SampleRate,
                            BitsPerSample = meta.BitsPerSample,
                            Channels = meta.Channels,
                            TotalSamples = meta.TotalSamples
                        });
                    }
                    catch
                    {
                        string title = Path.GetFileNameWithoutExtension(file);
                        results.Add(new PersistedTrack
                        {
                            FilePath = file,
                            Title = title,
                            Artist = "Unknown Artist",
                            Album = "Unknown Album",
                            Genre = "Unknown Genre",
                            Composer = "",
                            SampleRate = 44100,
                            BitsPerSample = 16,
                            Channels = 2,
                            TotalSamples = 0
                        });
                    }

                    progress?.Report((current, total, file));
                }
            }, ct);

            return results;
        }
    }

    public class PersistedLibrary
    {
        public List<PersistedTrack> Tracks { get; set; } = new();
    }

    public class PersistedTrack
    {
        public string FilePath { get; set; } = string.Empty;
        public string Title { get; set; } = string.Empty;
        public string Artist { get; set; } = string.Empty;
        public string Album { get; set; } = string.Empty;
        public string AlbumArtist { get; set; } = string.Empty;
        public string Genre { get; set; } = "Unknown Genre";
        public string Composer { get; set; } = string.Empty;
        public uint SampleRate { get; set; } = 44100;
        public uint BitsPerSample { get; set; } = 16;
        public uint Channels { get; set; } = 2;
        public ulong TotalSamples { get; set; }
    }
}
