using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace HiResPlayer.Application.Services
{
    public class PlaylistModel
    {
        public string Id { get; set; } = Guid.NewGuid().ToString("N");
        public string Name { get; set; } = "New Playlist";
        public DateTime CreatedUtc { get; set; } = DateTime.UtcNow;
        public List<string> TrackFilePaths { get; set; } = new();
    }

    public class PersistedPlaylists
    {
        public List<PlaylistModel> Playlists { get; set; } = new();
    }

    public class PlaylistService
    {
        private static readonly string _defaultStorePath = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "HiResPlayer",
            "playlists.json"
        );

        private readonly string _storePath;
        private readonly List<PlaylistModel> _playlists = new();
        private readonly JsonSerializerOptions _jsonOpts = new()
        {
            WriteIndented = true,
            DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
        };

        public PlaylistService(string? customPath = null)
        {
            _storePath = customPath ?? _defaultStorePath;
            Load();
        }

        public IReadOnlyList<PlaylistModel> Playlists => _playlists.AsReadOnly();

        public void Load()
        {
            try
            {
                if (File.Exists(_storePath))
                {
                    string json = File.ReadAllText(_storePath);
                    var data = JsonSerializer.Deserialize<PersistedPlaylists>(json, _jsonOpts);
                    _playlists.Clear();
                    if (data?.Playlists != null)
                    {
                        _playlists.AddRange(data.Playlists);
                    }
                }
            }
            catch
            {
                _playlists.Clear();
            }

            // Ensure "Favorites" playlist always exists
            if (!_playlists.Any(p => p.Name.Equals("Favorites", StringComparison.OrdinalIgnoreCase)))
            {
                _playlists.Insert(0, new PlaylistModel
                {
                    Id = "favorites",
                    Name = "Favorites",
                    CreatedUtc = DateTime.UtcNow,
                    TrackFilePaths = new List<string>()
                });
                Save();
            }
        }

        public void Save()
        {
            try
            {
                Directory.CreateDirectory(Path.GetDirectoryName(_storePath)!);
                string json = JsonSerializer.Serialize(new PersistedPlaylists { Playlists = _playlists }, _jsonOpts);
                File.WriteAllText(_storePath, json);
            }
            catch
            {
                // Silently ignore write failures
            }
        }

        public PlaylistModel CreatePlaylist(string name, IEnumerable<string>? initialTrackPaths = null)
        {
            if (string.IsNullOrWhiteSpace(name)) name = "Untitled Playlist";
            string uniqueName = name.Trim();
            int counter = 1;
            while (_playlists.Any(p => p.Name.Equals(uniqueName, StringComparison.OrdinalIgnoreCase)))
            {
                uniqueName = $"{name.Trim()} ({++counter})";
            }

            var pl = new PlaylistModel
            {
                Id = Guid.NewGuid().ToString("N"),
                Name = uniqueName,
                CreatedUtc = DateTime.UtcNow,
                TrackFilePaths = initialTrackPaths?.Distinct(StringComparer.OrdinalIgnoreCase).ToList() ?? new List<string>()
            };
            _playlists.Add(pl);
            Save();
            return pl;
        }

        public bool RenamePlaylist(string id, string newName)
        {
            if (string.IsNullOrWhiteSpace(newName)) return false;
            var pl = _playlists.FirstOrDefault(p => p.Id == id);
            if (pl == null) return false;
            pl.Name = newName.Trim();
            Save();
            return true;
        }

        public bool DeletePlaylist(string id)
        {
            if (id == "favorites") return false; // Prevent deleting default Favorites
            int removed = _playlists.RemoveAll(p => p.Id == id);
            if (removed > 0)
            {
                Save();
                return true;
            }
            return false;
        }

        public bool AddTrackToPlaylist(string playlistId, string filePath)
        {
            var pl = _playlists.FirstOrDefault(p => p.Id == playlistId);
            if (pl == null) return false;
            if (!pl.TrackFilePaths.Contains(filePath, StringComparer.OrdinalIgnoreCase))
            {
                pl.TrackFilePaths.Add(filePath);
                Save();
                return true;
            }
            return false;
        }

        public bool AddTracksToPlaylist(string playlistId, IEnumerable<string> filePaths)
        {
            var pl = _playlists.FirstOrDefault(p => p.Id == playlistId);
            if (pl == null) return false;
            bool addedAny = false;
            foreach (var path in filePaths)
            {
                if (!pl.TrackFilePaths.Contains(path, StringComparer.OrdinalIgnoreCase))
                {
                    pl.TrackFilePaths.Add(path);
                    addedAny = true;
                }
            }
            if (addedAny) Save();
            return addedAny;
        }

        public bool RemoveTrackFromPlaylist(string playlistId, string filePath)
        {
            var pl = _playlists.FirstOrDefault(p => p.Id == playlistId);
            if (pl == null) return false;
            int removed = pl.TrackFilePaths.RemoveAll(p => p.Equals(filePath, StringComparison.OrdinalIgnoreCase));
            if (removed > 0)
            {
                Save();
                return true;
            }
            return false;
        }

        public bool IsFavorite(string filePath)
        {
            var fav = _playlists.FirstOrDefault(p => p.Id == "favorites");
            return fav != null && fav.TrackFilePaths.Contains(filePath, StringComparer.OrdinalIgnoreCase);
        }

        public bool ToggleFavorite(string filePath)
        {
            if (IsFavorite(filePath))
            {
                return !RemoveTrackFromPlaylist("favorites", filePath);
            }
            else
            {
                return AddTrackToPlaylist("favorites", filePath);
            }
        }

        public void ExportM3u(string playlistId, string targetFilePath)
        {
            var pl = _playlists.FirstOrDefault(p => p.Id == playlistId);
            if (pl == null) return;
            var lines = new List<string> { "#EXTM3U", $"#PLAYLIST:{pl.Name}" };
            foreach (var track in pl.TrackFilePaths)
            {
                lines.Add(track);
            }
            File.WriteAllLines(targetFilePath, lines);
        }

        public PlaylistModel? ImportM3u(string m3uPath)
        {
            if (!File.Exists(m3uPath)) return null;
            string playlistName = Path.GetFileNameWithoutExtension(m3uPath);
            var lines = File.ReadAllLines(m3uPath);
            var validTracks = new List<string>();
            string dir = Path.GetDirectoryName(m3uPath)!;

            foreach (var rawLine in lines)
            {
                string line = rawLine.Trim();
                if (string.IsNullOrWhiteSpace(line) || line.StartsWith("#")) continue;
                string fullPath = Path.IsPathRooted(line) ? line : Path.GetFullPath(Path.Combine(dir, line));
                if (File.Exists(fullPath))
                {
                    validTracks.Add(fullPath);
                }
            }

            return CreatePlaylist(playlistName, validTracks);
        }
    }
}
