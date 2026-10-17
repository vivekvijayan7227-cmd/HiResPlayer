using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;

namespace HiResPlayer.Application.Equalizer
{
    public record GraphicEqPreset(
        string Name,
        float PreampDb,
        float[] GainsDb,
        bool IsFactory
    );

    public record ParametricBandPreset(
        bool Enabled,
        uint FilterType,
        float Frequency,
        float GainDb,
        float Q
    );

    public record ParametricEqPreset(
        string Name,
        float PreampDb,
        List<ParametricBandPreset> Bands,
        bool IsFactory
    );

    public class PresetService
    {
        public static readonly string DefaultUserPresetPath = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "HiResPlayer", "Presets", "user_presets.json"
        );

        private static readonly List<GraphicEqPreset> FactoryPresets = new()
        {
            new("Flat", 0.0f, new float[10] { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, true),
            new("Balanced", 0.0f, new float[10] { 1.0f, 0.5f, 0, 0, 0, 0, 0.5f, 1.0f, 1.5f, 1.0f }, true),
            new("Bass Boost", -2.0f, new float[10] { 5.5f, 4.5f, 3.0f, 1.5f, 0.5f, 0, 0, 0, 0, 0 }, true),
            new("Bass Reduction", 0.0f, new float[10] { -5.0f, -4.0f, -3.0f, -1.5f, 0, 0, 0, 0, 0, 0 }, true),
            new("Treble Boost", -2.0f, new float[10] { 0, 0, 0, 0, 0, 0.5f, 1.5f, 3.5f, 5.0f, 5.5f }, true),
            new("Treble Reduction", 0.0f, new float[10] { 0, 0, 0, 0, 0, 0, -1.5f, -3.0f, -4.5f, -5.0f }, true),
            new("Warm", -1.0f, new float[10] { 3.0f, 2.5f, 2.0f, 1.0f, 0.5f, 0, -0.5f, -1.0f, -1.5f, -2.0f }, true),
            new("Bright", -1.5f, new float[10] { -1.5f, -1.0f, -0.5f, 0, 0.5f, 1.0f, 2.0f, 3.0f, 4.0f, 4.5f }, true),
            new("Vocal Clarity", -1.0f, new float[10] { -2.0f, -1.0f, 0, 1.5f, 3.0f, 3.5f, 2.5f, 1.0f, 0.5f, 0 }, true),
            new("Acoustic", -1.0f, new float[10] { 2.0f, 1.5f, 1.0f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 2.0f, 1.5f }, true),
            new("Classical", -1.0f, new float[10] { 3.0f, 2.5f, 2.0f, 1.5f, -0.5f, -0.5f, 0, 1.5f, 2.0f, 2.5f }, true),
            new("Jazz", -1.0f, new float[10] { 2.5f, 2.0f, 1.0f, 1.5f, -1.0f, -1.0f, 0, 1.0f, 2.0f, 2.5f }, true),
            new("Rock", -2.0f, new float[10] { 4.5f, 3.5f, 2.0f, 0.5f, -0.5f, 0.5f, 2.0f, 3.5f, 4.0f, 4.5f }, true),
            new("Pop", -1.5f, new float[10] { -1.0f, 1.0f, 2.5f, 3.0f, 2.0f, 0, -1.0f, 1.5f, 3.0f, 3.5f }, true),
            new("Electronic", -2.5f, new float[10] { 5.0f, 4.0f, 1.5f, 0, -1.0f, 1.5f, 2.0f, 3.0f, 4.5f, 5.0f }, true),
            new("Hip-Hop", -2.5f, new float[10] { 6.0f, 5.0f, 3.0f, 1.0f, -0.5f, 0, 1.5f, 2.0f, 3.0f, 3.5f }, true),
            new("Night Listening", 0.0f, new float[10] { -3.0f, -2.0f, -1.0f, 0, 1.5f, 2.0f, 1.0f, 0, -1.5f, -3.0f }, true)
        };

        public IReadOnlyList<GraphicEqPreset> GetFactoryPresets() => FactoryPresets;

        public List<GraphicEqPreset> GetAllPresets()
        {
            var list = new List<GraphicEqPreset>(FactoryPresets);
            list.AddRange(LoadUserPresets());
            return list;
        }

        public List<GraphicEqPreset> LoadUserPresets()
        {
            try
            {
                if (File.Exists(DefaultUserPresetPath))
                {
                    var json = File.ReadAllText(DefaultUserPresetPath);
                    return JsonSerializer.Deserialize<List<GraphicEqPreset>>(json) ?? new();
                }
            }
            catch { }
            return new();
        }

        public void SaveUserPreset(GraphicEqPreset preset)
        {
            var list = LoadUserPresets();
            list.RemoveAll(p => p.Name.Equals(preset.Name, StringComparison.OrdinalIgnoreCase));
            list.Add(preset);
            string? dir = Path.GetDirectoryName(DefaultUserPresetPath);
            if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
            File.WriteAllText(DefaultUserPresetPath, JsonSerializer.Serialize(list, new JsonSerializerOptions { WriteIndented = true }));
        }

        public void ExportPresets(string jsonPath, IEnumerable<GraphicEqPreset> presets)
        {
            var json = JsonSerializer.Serialize(presets, new JsonSerializerOptions { WriteIndented = true });
            File.WriteAllText(jsonPath, json);
        }

        public List<GraphicEqPreset> ImportPresets(string jsonPath)
        {
            if (!File.Exists(jsonPath)) return new List<GraphicEqPreset>();
            var json = File.ReadAllText(jsonPath);
            return JsonSerializer.Deserialize<List<GraphicEqPreset>>(json) ?? new List<GraphicEqPreset>();
        }
    }
}
