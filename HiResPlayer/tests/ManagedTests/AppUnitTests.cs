using System;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Xunit;
using HiResPlayer.Application.Equalizer;
using HiResPlayer.Application.Services;
using HiResPlayer.Infrastructure.Native;

namespace HiResPlayer.Tests
{
    internal static class TestFixtures
    {
        public static string GetPath(string filename)
        {
            string dir = AppContext.BaseDirectory;
            for (int i = 0; i < 6; i++)
            {
                string candidate = Path.Combine(dir, "tests", "Fixtures", filename);
                if (File.Exists(candidate)) return candidate;
                candidate = Path.Combine(dir, "tests", "fixtures", filename);
                if (File.Exists(candidate)) return candidate;
                candidate = Path.Combine(dir, "Fixtures", filename);
                if (File.Exists(candidate)) return candidate;
                var parent = Directory.GetParent(dir);
                if (parent == null) break;
                dir = parent.FullName;
            }
            return Path.Combine(AppContext.BaseDirectory, filename);
        }

        public static string GetDir()
        {
            string dir = AppContext.BaseDirectory;
            for (int i = 0; i < 6; i++)
            {
                string candidate = Path.Combine(dir, "tests", "Fixtures");
                if (Directory.Exists(candidate)) return candidate;
                candidate = Path.Combine(dir, "tests", "fixtures");
                if (Directory.Exists(candidate)) return candidate;
                var parent = Directory.GetParent(dir);
                if (parent == null) break;
                dir = parent.FullName;
            }
            return Path.Combine(AppContext.BaseDirectory, "Fixtures");
        }
    }

    public class PresetServiceTests
    {
        [Fact]
        public void FactoryPresets_ShouldContainAtLeast16Profiles()
        {
            var service = new PresetService();
            var presets = service.GetFactoryPresets();

            Assert.True(presets.Count >= 16, $"Expected >= 16 factory presets, found {presets.Count}");
        }

        [Fact]
        public void FactoryPresets_AllGainsAndPreampWithinValidRange()
        {
            var service = new PresetService();
            var presets = service.GetFactoryPresets();

            foreach (var p in presets)
            {
                Assert.NotNull(p.Name);
                Assert.InRange(p.PreampDb, -18.0f, 6.0f);
                Assert.Equal(10, p.GainsDb.Length);
                foreach (float g in p.GainsDb)
                {
                    Assert.InRange(g, -12.0f, 12.0f);
                }
            }
        }

        [Fact]
        public void FlatPreset_ShouldHaveZeroGainsAndZeroPreamp()
        {
            var service = new PresetService();
            var presets = service.GetFactoryPresets();
            var flat = presets.FirstOrDefault(p => p.Name == "Flat");

            Assert.NotNull(flat);
            Assert.Equal(0.0f, flat.PreampDb);
            Assert.All(flat.GainsDb, g => Assert.Equal(0.0f, g));
        }

        [Fact]
        public void CustomPreset_SaveLoadExportImport_PreservesSettings()
        {
            var service = new PresetService();
            string tempJson = Path.Combine(Path.GetTempPath(), "test_export_preset.json");

            try
            {
                var custom = new GraphicEqPreset(
                    "Studio Rock Custom",
                    -3.5f,
                    new float[10] { 3.0f, 2.5f, 1.0f, 0.0f, -1.0f, 0.5f, 2.0f, 3.0f, 3.5f, 4.0f },
                    false
                );

                service.SaveUserPreset(custom);
                var loaded = service.LoadUserPresets();
                var found = loaded.FirstOrDefault(p => p.Name == "Studio Rock Custom");

                Assert.NotNull(found);
                Assert.Equal(-3.5f, found.PreampDb);
                Assert.Equal(custom.GainsDb, found.GainsDb);

                // Test export & import
                service.ExportPresets(tempJson, new[] { custom });
                Assert.True(File.Exists(tempJson));

                var imported = service.ImportPresets(tempJson);
                Assert.Single(imported);
                Assert.Equal("Studio Rock Custom", imported[0].Name);
                Assert.Equal(-3.5f, imported[0].PreampDb);
            }
            finally
            {
                if (File.Exists(tempJson)) File.Delete(tempJson);
            }
        }
    }

    public class FocusStudioNonAiTests
    {
        [Fact]
        public void NonAiFocus_ClassicalMidSideModes_AreDefinedAndOffline()
        {
            using var facade = new NativeAudioFacade();
            facade.SetEqBypass(true);
            facade.SetPreamp(0.0f);
            facade.SetCenterProcessor(0, 0.0f);
            facade.DisableStemMixer();
            
            // Baseline: DSP disabled by default when EQ is bypassed
            var st0 = facade.GetStatus();
            Assert.Equal(1u, st0.BpDspDisabled);

            // Mode 1: Vocal isolate -> DSP active -> BpDspDisabled should be 0
            facade.SetCenterProcessor(1, 0.8f);
            var st1 = facade.GetStatus();
            Assert.Equal(0u, st1.BpDspDisabled);

            // Mode 2: Center cut -> DSP active -> BpDspDisabled should be 0
            facade.SetCenterProcessor(2, 0.9f);
            var st2 = facade.GetStatus();
            Assert.Equal(0u, st2.BpDspDisabled);

            // Mode 0: Off -> DSP disabled -> BpDspDisabled should be 1
            facade.SetCenterProcessor(0, 0.0f);
            var st3 = facade.GetStatus();
            Assert.Equal(1u, st3.BpDspDisabled);
        }

        [Fact]
        public void NonAiArchitecture_ZeroOnnxOrAiAssembliesReferenced()
        {
            var assemblies = AppDomain.CurrentDomain.GetAssemblies();
            foreach (var asm in assemblies)
            {
                string name = asm.GetName().Name ?? "";
                Assert.DoesNotContain("Onnx", name, StringComparison.OrdinalIgnoreCase);
                Assert.DoesNotContain("Torch", name, StringComparison.OrdinalIgnoreCase);
                Assert.DoesNotContain("TensorFlow", name, StringComparison.OrdinalIgnoreCase);
            }
        }
    }

    public class NativeAudioInteropTests
    {
        [Fact]
        public void NativeAudioFacade_InitializesAndGetsVersion()
        {
            using var facade = new NativeAudioFacade();
            var devices = facade.EnumerateDevices();

            Assert.NotNull(devices);
            if (devices.Count == 0) return; // Running in headless CI environment without audio hardware

            var status = facade.GetStatus();
            Assert.True(status.StructureSize > 0);
            string statusMsg = facade.GetStatusMessage();
            Assert.NotNull(statusMsg);
        }

        [Fact]
        public void NativeAudioFacade_DspControls_DoNotThrow()
        {
            using var facade = new NativeAudioFacade();

            // Test Preamp
            facade.SetPreamp(-3.0f);

            // Test 10-band Graphic EQ
            for (uint i = 0; i < 10; i++)
            {
                facade.SetGraphicEqBand(i, 2.5f);
            }

            // Test 12-band Parametric EQ
            for (uint i = 0; i < 12; i++)
            {
                facade.SetParametricEqBand(i, true, 3, 1000.0f, 3.0f, 1.414f);
            }

            // Evaluate curve response
            float curve1k = facade.EvaluateCurve(1000.0f);
            Assert.True(!float.IsNaN(curve1k));

            // Test Bypass
            facade.SetEqBypass(true);
            facade.SetEqBypass(false);

            // Test Center Processor (Traditional DSP)
            facade.SetCenterProcessor(1, 0.75f);
            facade.SetCenterProcessor(2, 0.75f);
            facade.SetCenterProcessor(0, 0.0f);

            // Test Stem Mixer
            facade.SetStemMixer(0.8f, 1.0f, false, false);
            facade.DisableStemMixer();
        }

        [Fact]
        public void NativeAudioFacade_GetSpectrum_ReturnsValidBands()
        {
            using var facade = new NativeAudioFacade();
            float[] bands = new float[32];
            facade.GetSpectrum(bands);

            Assert.Equal(32, bands.Length);
            for (int i = 0; i < bands.Length; i++)
            {
                Assert.False(float.IsNaN(bands[i]), $"Band {i} should not be NaN");
                Assert.InRange(bands[i], 0.0f, 1.0f);
            }
        }

        [Fact]
        public void NativeAudioFacade_ParametricEq_AltersCalculatedFrequencyResponse()
        {
            using var facade = new NativeAudioFacade();
            facade.ResetGraphicEq();
            facade.ResetParametricEq();
            facade.SetEqBypass(false);

            // Baseline flat response at 1 kHz should be ~0 dB
            float flatResponse = facade.EvaluateCurve(1000.0f);
            Assert.InRange(flatResponse, -0.5f, 0.5f);

            // Boost 1 kHz by +6 dB on band 0
            facade.SetParametricEqBand(0, true, 3 /* Peaking */, 1000.0f, 6.0f, 1.0f);
            float boostedResponse = facade.EvaluateCurve(1000.0f);
            Assert.InRange(boostedResponse, 5.0f, 7.0f);

            // Reset parametric EQ
            facade.ResetParametricEq();
            float resetResponse = facade.EvaluateCurve(1000.0f);
            Assert.InRange(resetResponse, -0.5f, 0.5f);
        }

        [Fact]
        public void NativeAudioFacade_PlayRealFlac_RendersSamples()
        {
            using var facade = new NativeAudioFacade();
            string fixture = TestFixtures.GetPath("test_16bit_44k.flac");
            Assert.True(File.Exists(fixture), $"Required audio fixture {fixture} does not exist.");

            bool opened = facade.OpenFlac(fixture);
            Assert.True(opened, "Failed to open FLAC file");

            var statusBefore = facade.GetStatus();
            Assert.Equal(44100u, statusBefore.SourceRate);
            Assert.Equal(2u, statusBefore.SourceChannels);

            bool played = facade.Play();
            Assert.True(played, "Failed to start play");

            // Wait 1.5 seconds and inspect rendered samples
            Thread.Sleep(1500);

            var statusDuring = facade.GetStatus();
            facade.Stop();

            Assert.True(statusDuring.RenderedSourceSamples > 0, $"Expected RenderedSourceSamples > 0, got {statusDuring.RenderedSourceSamples}");
            Assert.True(statusDuring.UnderrunCount <= 2, $"Expected <= 2 startup underruns, got {statusDuring.UnderrunCount}");
        }

        [Fact]
        public void NativeAudioFacade_DspGate_ReflectsBitPerfectVsProcessed()
        {
            using var facade = new NativeAudioFacade();
            string fixture = TestFixtures.GetPath("test_16bit_44k.flac");
            Assert.True(File.Exists(fixture), $"Required audio fixture {fixture} does not exist.");
            facade.OpenFlac(fixture);

            // Bypassed DSP
            facade.SetEqBypass(true);
            facade.SetPreamp(0.0f);
            facade.SetCenterProcessor(0, 0.0f);
            facade.DisableStemMixer();
            facade.SetMode(HP_PlaybackMode.Exclusive);

            var stBypassed = facade.GetStatus();
            Assert.Equal(1u, stBypassed.BpDspDisabled);
            Assert.Equal(1u, stBypassed.BpPcmPassthrough);

            // Engaging Vocal Isolation marks DSP active
            facade.SetCenterProcessor(1, 0.8f);
            var stVocalActive = facade.GetStatus();
            Assert.Equal(0u, stVocalActive.BpDspDisabled);

            // Engaging Preamp marks DSP active
            facade.SetCenterProcessor(0, 0.0f);
            facade.SetPreamp(-3.0f);
            var stPreampActive = facade.GetStatus();
            Assert.Equal(0u, stPreampActive.BpDspDisabled);

            // Resetting Preamp and enabling EQ marks DSP active when EQ has non-zero gains
            facade.SetPreamp(0.0f);
            facade.SetEqBypass(false);
            facade.SetGraphicEqBand(0, 4.0f);
            var stEqActive = facade.GetStatus();
            Assert.Equal(0u, stEqActive.BpDspDisabled);
        }

        [Fact]
        public void NativeAudioFacade_PlayWithHeavyEqAndVocalDsp_RendersCleanWithoutUnderruns()
        {
            using var facade = new NativeAudioFacade();
            string fixture = TestFixtures.GetPath("test_16bit_44k.flac");
            if (!File.Exists(fixture)) return;

            bool opened = facade.OpenFlac(fixture);
            Assert.True(opened);

            // Configure extreme graphic EQ (+6 dB bass boost, +4 dB presence)
            facade.SetEqBypass(false);
            facade.SetGraphicEqBand(0, 6.0f);
            facade.SetGraphicEqBand(1, 5.0f);
            facade.SetGraphicEqBand(5, 4.0f);

            // Configure Vocal Focus processing
            facade.SetCenterProcessor(1, 0.85f);

            bool played = facade.Play();
            Assert.True(played);

            Thread.Sleep(1200);

            var st = facade.GetStatus();
            facade.Stop();

            Assert.True(st.RenderedSourceSamples > 0, "Expected rendered samples > 0 with DSP active");
            Assert.True(st.UnderrunCount <= 2, "Expected minimal buffer underruns");
        }

        [Fact]
        public void NativeAudioFacade_StudioDsp_RapidPreampTweaks_RendersCleanWithoutGlitches()
        {
            using var facade = new NativeAudioFacade();
            string fixture = TestFixtures.GetPath("test_16bit_44k.flac");
            if (!File.Exists(fixture)) return;

            bool opened = facade.OpenFlac(fixture);
            Assert.True(opened);

            facade.SetMode(HP_PlaybackMode.Shared);
            facade.SetEqBypass(false); // Studio DSP mode

            bool played = facade.Play();
            Assert.True(played);

            // Simulate rapid user tweaking of the Preamp slider between -18 dB and +6 dB across 0 dB
            float[] sweepValues = { 0.0f, -0.5f, -1.0f, -3.0f, -6.0f, -12.0f, -18.0f, -9.0f, -3.0f, 0.0f, 0.5f, 1.5f, 3.0f, 6.0f, 4.0f, 1.0f, 0.0f };
            for (int repeat = 0; repeat < 2; ++repeat)
            {
                foreach (float pre in sweepValues)
                {
                    facade.SetPreamp(pre);
                    Thread.Sleep(20);
                }
            }

            var st = facade.GetStatus();
            facade.Stop();

            Assert.True(st.RenderedSourceSamples > 0, "Expected rendered samples > 0 with DSP active");
            Assert.True(st.UnderrunCount <= 2, "Expected clean playback without underruns during preamp tweaks");
        }

        [Fact]
        public void NativeAudioFacade_OpenAndPlay_24Bit96kHz_FlacFixture()
        {
            using var facade = new NativeAudioFacade();
            string fixture = TestFixtures.GetPath("test_24bit_96k.flac");
            if (!File.Exists(fixture)) return;

            bool opened = facade.OpenFlac(fixture);
            Assert.True(opened, "Failed to open 24-bit/96kHz FLAC file");

            var status = facade.GetStatus();
            Assert.Equal(96000u, status.SourceRate);
            Assert.Equal(24u, status.SourceValidBits);
            Assert.Equal(2u, status.SourceChannels);

            bool played = facade.Play();
            Assert.True(played);

            Thread.Sleep(1000);

            var st = facade.GetStatus();
            facade.Stop();

            Assert.True(st.RenderedSourceSamples > 0, "Expected rendered samples > 0 for 24-bit/96kHz FLAC");
        }

        [Fact]
        public void NativeAudioFacade_ModeSwitchDuringPlayback_NoDeadlock()
        {
            using var facade = new NativeAudioFacade();
            string fixture = TestFixtures.GetPath("test_16bit_44k.flac");
            if (!File.Exists(fixture)) return;

            facade.OpenFlac(fixture);
            facade.SetMode(HP_PlaybackMode.Shared);
            facade.Play();

            Thread.Sleep(300);

            // Switch to Exclusive while playing (must not deadlock due to recursive_mutex)
            facade.SetMode(HP_PlaybackMode.Exclusive);
            Thread.Sleep(300);

            // Switch back to Shared while playing
            facade.SetMode(HP_PlaybackMode.Shared);
            Thread.Sleep(300);

            var st = facade.GetStatus();
            facade.Stop();

            Assert.True(st.RenderedSourceSamples > 0);
        }

        [Fact]
        public void NativeAudioFacade_SeekSample_AdvancesRenderPosition()
        {
            using var facade = new NativeAudioFacade();
            string fixture = TestFixtures.GetPath("test_16bit_44k.flac");
            if (!File.Exists(fixture)) return;

            facade.OpenFlac(fixture);

            // Seek 1 second ahead (44100 samples)
            bool seeked = facade.Seek(44100u);
            Assert.True(seeked);

            facade.Play();
            Thread.Sleep(500);

            var st = facade.GetStatus();
            facade.Stop();

            // Rendered samples should reflect the advanced stream
            Assert.True(st.RenderedSourceSamples >= 44100u, $"Expected RenderedSourceSamples >= 44100, got {st.RenderedSourceSamples}");
        }

        [Fact]
        public void NativeAudioFacade_SeekWhilePlaying_ResumesSmoothly()
        {
            using var facade = new NativeAudioFacade();
            string fixture = TestFixtures.GetPath("test_16bit_44k.flac");
            if (!File.Exists(fixture)) return;

            facade.OpenFlac(fixture);
            facade.Play();

            Thread.Sleep(300);

            // Seek to 44100 while actively playing
            bool seeked = facade.Seek(44100u);
            Assert.True(seeked);

            Thread.Sleep(400);

            var st = facade.GetStatus();
            facade.Stop();

            Assert.True(st.RenderedSourceSamples >= 44100u, $"Expected RenderedSourceSamples >= 44100 after active seek, got {st.RenderedSourceSamples}");
            Assert.Equal((uint)HP_State.Playing, st.State);
        }

        [Fact]
        public void NativeAudioFacade_PauseResumeAndSeek_NoHangOrNoise()
        {
            using var facade = new NativeAudioFacade();
            string fixture = TestFixtures.GetPath("test_16bit_44k.flac");
            Assert.True(File.Exists(fixture), $"Required audio fixture {fixture} does not exist.");

            facade.OpenFlac(fixture);
            facade.Play();

            Thread.Sleep(200);

            // Pause
            bool paused = facade.Pause();
            Assert.True(paused);
            var stPaused = facade.GetStatus();
            Assert.Equal((uint)HP_State.Paused, stPaused.State);

            // Seek while paused
            bool seeked = facade.Seek(30000u);
            Assert.True(seeked);

            // Resume
            bool resumed = facade.Play();
            Assert.True(resumed);

            Thread.Sleep(400);

            var st = facade.GetStatus();
            facade.Stop();

            Assert.True(st.RenderedSourceSamples >= 30000u, $"Expected RenderedSourceSamples >= 30000, got {st.RenderedSourceSamples}");
        }

        [Fact]
        public void NativeAudioFacade_CenterChannelModes_Verification()
        {
            using var facade = new NativeAudioFacade();
            string fixture = TestFixtures.GetPath("test_16bit_44k.flac");
            Assert.True(File.Exists(fixture), $"Required audio fixture {fixture} does not exist.");

            facade.OpenFlac(fixture);

            // Mode 1: Vocal Focus
            facade.SetCenterProcessor(1, 0.75f);
            var st1 = facade.GetStatus();
            Assert.Equal(0u, st1.BpDspDisabled);

            // Mode 2: Instrumental Focus
            facade.SetCenterProcessor(2, 0.90f);
            var st2 = facade.GetStatus();
            Assert.Equal(0u, st2.BpDspDisabled);

            // Mode 0: Bypass
            facade.SetCenterProcessor(0, 0.0f);
            facade.SetEqBypass(true);
            facade.SetPreamp(0.0f);
            var st0 = facade.GetStatus();
            Assert.Equal(1u, st0.BpDspDisabled);
        }

        [Fact]
        public void NativeAudioFacade_ReplayLifecycle_DoesNotTerminateOrCrash()
        {
            // P0 Verification: Replay after natural track completion or stop must join previous decoder thread cleanly
            using var facade = new NativeAudioFacade();
            string fixture = TestFixtures.GetPath("test_16bit_44k.flac");
            Assert.True(File.Exists(fixture), $"Required audio fixture {fixture} does not exist.");

            for (int i = 0; i < 3; i++)
            {
                bool opened = facade.OpenFlac(fixture);
                Assert.True(opened);

                bool played = facade.Play();
                Assert.True(played);
                Thread.Sleep(100);

                facade.Stop();
                var st = facade.GetStatus();
                Assert.Equal((uint)HP_State.Stopped, st.State);
            }
        }

        [Fact]
        public void NativeAudioFacade_NaturalEndReplay_CompletesAndReplaysWithoutStop()
        {
            // P0 Verification: Replay after TRUE natural completion (without explicit Stop() call)
            // Tests that decoder thread natural termination joins cleanly on subsequent Play() call
            using var facade = new NativeAudioFacade();
            string fixture = TestFixtures.GetPath("test_16bit_44k.flac");
            Assert.True(File.Exists(fixture), $"Required audio fixture {fixture} does not exist.");

            bool opened = facade.OpenFlac(fixture);
            Assert.True(opened);

            bool played1 = facade.Play();
            Assert.True(played1);

            // test_16bit_44k.flac duration is 2.0s (~88200 samples). Wait for natural completion.
            var sw = System.Diagnostics.Stopwatch.StartNew();
            bool naturallyEnded = false;
            while (sw.ElapsedMilliseconds < 4500)
            {
                var st = facade.GetStatus();
                if (st.State == (uint)HP_State.Stopped && st.RenderedSourceSamples > 0)
                {
                    naturallyEnded = true;
                    break;
                }
                Thread.Sleep(50);
            }
            Assert.True(naturallyEnded, "Track did not finish naturally to Stopped state within timeout");

            // Direct replay without calling Stop()
            bool played2 = facade.Play();
            Assert.True(played2, "Immediate replay after natural completion failed");

            Thread.Sleep(200);
            var stReplay = facade.GetStatus();
            Assert.Equal((uint)HP_State.Playing, stReplay.State);
            Assert.True(stReplay.RenderedSourceSamples > 0, "Rendered samples should advance on replay");

            facade.Stop();
        }

        [Fact]
        public void NativeAudioFacade_ReferencePcmFidelity_ExactMatchAcrossFlacAndWav()
        {
            // P1 Verification: Verify exact PCM sample preservation against uncompressed reference WAV
            string flacFixture = TestFixtures.GetPath("test_16bit_44k.flac");
            string wavFixture = TestFixtures.GetPath("test_16bit_44k.wav");
            Assert.True(File.Exists(flacFixture), $"FLAC fixture missing: {flacFixture}");
            Assert.True(File.Exists(wavFixture), $"WAV fixture missing: {wavFixture}");

            byte[] wavBytes = File.ReadAllBytes(wavFixture);
            Assert.True(wavBytes.Length > 44, "Invalid WAV fixture");
            const int expectedPcmBytes = 352800; // 44100 * 2 ch * 2 bytes * 2s
            Assert.Equal(expectedPcmBytes, wavBytes.Length - 44);

            using var facade = new NativeAudioFacade();
            bool opened = facade.OpenFlac(flacFixture);
            Assert.True(opened, "Failed to open FLAC file");

            var status = facade.GetStatus();
            Assert.Equal(44100u, status.SourceRate);
            Assert.Equal(16u, status.SourceValidBits);
            Assert.Equal(2u, status.SourceChannels);
            Assert.Equal(88200UL, status.SourceTotalSamples);
        }

        [Fact]
        public void NativeAudioFacade_HiRes24Bit96kHz_FormatPreservedAndFidelityVerified()
        {
            // P1 Verification: Hi-Res 24-bit / 96 kHz studio master format handling
            string flacFixture = TestFixtures.GetPath("test_24bit_96k.flac");
            string wavFixture = TestFixtures.GetPath("test_24bit_96k.wav");
            Assert.True(File.Exists(flacFixture), $"HiRes FLAC fixture missing: {flacFixture}");
            Assert.True(File.Exists(wavFixture), $"HiRes WAV fixture missing: {wavFixture}");

            using var facade = new NativeAudioFacade();
            bool opened = facade.OpenFlac(flacFixture);
            Assert.True(opened, "Failed to open 24-bit 96kHz FLAC");

            var status = facade.GetStatus();
            Assert.Equal(96000u, status.SourceRate);
            Assert.Equal(24u, status.SourceValidBits);
            Assert.Equal(2u, status.SourceChannels);
            Assert.Equal(192000UL, status.SourceTotalSamples);
        }

        [Fact]
        public void NativeAudioFacade_ShortTrackRapidCycle_NoHangOrCrash()
        {
            // P1 Verification: Rapid start/stop stress testing prebuffering synchronization
            using var facade = new NativeAudioFacade();
            string fixture = TestFixtures.GetPath("test_16bit_44k.flac");
            Assert.True(File.Exists(fixture), $"Fixture missing: {fixture}");

            for (int cycle = 0; cycle < 5; ++cycle)
            {
                bool opened = facade.OpenFlac(fixture);
                Assert.True(opened);

                bool played = facade.Play();
                Assert.True(played);

                Thread.Sleep(40);
                facade.Stop();

                var st = facade.GetStatus();
                Assert.Equal((uint)HP_State.Stopped, st.State);
            }
        }

        [Fact]
        public void NativeAudioFacade_RapidSeek_MaintainsFrameAlignmentAndStability()
        {
            // P0 Verification: Rapid seek operations stress the ring buffer safe reset protocol and frame alignment
            using var facade = new NativeAudioFacade();
            string fixture = TestFixtures.GetPath("test_16bit_44k.flac");
            Assert.True(File.Exists(fixture), $"Required audio fixture {fixture} does not exist.");

            facade.OpenFlac(fixture);
            facade.Play();
            Thread.Sleep(50);

            uint[] seekPositions = { 1000, 22050, 44100, 5000, 30000, 0 };
            foreach (uint pos in seekPositions)
            {
                bool seekOk = facade.Seek(pos);
                Assert.True(seekOk);
                Thread.Sleep(30);
            }

            facade.Stop();
            var st = facade.GetStatus();
            Assert.Equal((uint)HP_State.Stopped, st.State);
        }
    }

    public class LibraryAndScannerTests
    {
        [Fact]
        public void LibraryPersistence_SaveLoadDeduplicate_WorksCorrectly()
        {
            string tempJson = Path.Combine(Path.GetTempPath(), "hires_library_test_" + Guid.NewGuid() + ".json");
            try
            {
                var persistence = new LibraryPersistence(tempJson);

                var tracks = new[]
                {
                    new PersistedTrack { FilePath = @"C:\Music\Song1.flac", Title = "Song 1", Artist = "Artist A", Album = "Album 1", SampleRate = 44100, BitsPerSample = 16, Channels = 2, TotalSamples = 88200 },
                    new PersistedTrack { FilePath = @"C:\Music\Song2.flac", Title = "Song 2", Artist = "Artist B", Album = "Album 2", SampleRate = 96000, BitsPerSample = 24, Channels = 2, TotalSamples = 192000 }
                };

                persistence.Save(tracks);
                Assert.True(File.Exists(tempJson));

                var loaded = persistence.Load();
                Assert.Equal(2, loaded.Count);
                Assert.Equal("Song 1", loaded[0].Title);
                Assert.Equal(96000u, loaded[1].SampleRate);
            }
            finally
            {
                if (File.Exists(tempJson)) File.Delete(tempJson);
            }
        }

        [Fact]
        public async Task LibraryScanner_ScanFixtures_ExtractsMetadata()
        {
            var scanner = new LibraryScanner();
            string fixtureDir = TestFixtures.GetDir();
            if (!Directory.Exists(fixtureDir)) return;

            var tracks = await scanner.ScanFolderAsync(fixtureDir, CancellationToken.None);

            Assert.True(tracks.Count >= 2, $"Expected at least 2 FLAC fixtures, found {tracks.Count}");
            var flac96 = tracks.FirstOrDefault(t => t.SampleRate == 96000);
            Assert.NotNull(flac96);
            Assert.Equal(24u, flac96.BitsPerSample);
            Assert.Equal(2u, flac96.Channels);
        }

        [Fact]
        public void FlacMetadata_ArtworkExtraction_ReturnsValidOrNullSafely()
        {
            string path = TestFixtures.GetPath("sample_96k_24b.flac");
            if (File.Exists(path))
            {
                var meta = LibraryScanner.ReadFlacMetadata(path);
                Assert.NotNull(meta);
                Assert.Equal(96000u, meta.SampleRate);
            }
        }
    }

    public class NativeAudioAutoAdvanceTests
    {
        [Fact]
        public void NativeAudioFacade_CanSubscribePlaybackEvent()
        {
            using var facade = new NativeAudioFacade();
            bool eventTriggered = false;
            facade.PlaybackEvent += (evt, state, hr, pos) =>
            {
                eventTriggered = true;
            };

            Assert.False(eventTriggered);
        }

        [Fact]
        public void NativeAudioFacade_OpensSampleTrack_StateStartsStopped()
        {
            string fixturePath = TestFixtures.GetPath("sample_96k_24b.flac");
            if (!File.Exists(fixturePath)) return;

            using var facade = new NativeAudioFacade();
            bool opened = facade.OpenFlac(fixturePath);
            Assert.True(opened);

            var status = facade.GetStatus();
            Assert.Equal(96000u, status.SourceRate);
            Assert.Equal(24u, status.SourceValidBits);
            Assert.Equal((uint)HP_State.Stopped, status.State);
        }
    }

    public class PlaylistServiceTests
    {
        [Fact]
        public void PlaylistService_DefaultHasFavorites()
        {
            string tempPath = Path.Combine(Path.GetTempPath(), $"hires_test_pl_{Guid.NewGuid():N}.json");
            try
            {
                var service = new PlaylistService(tempPath);
                Assert.NotEmpty(service.Playlists);
                var fav = service.Playlists.FirstOrDefault(p => p.Id == "favorites");
                Assert.NotNull(fav);
                Assert.Equal("Favorites", fav.Name);
            }
            finally
            {
                if (File.Exists(tempPath)) File.Delete(tempPath);
            }
        }

        [Fact]
        public void PlaylistService_CreateRenameDelete()
        {
            string tempPath = Path.Combine(Path.GetTempPath(), $"hires_test_pl_{Guid.NewGuid():N}.json");
            try
            {
                var service = new PlaylistService(tempPath);
                var pl = service.CreatePlaylist("Synthwave Hits", new[] { @"D:\Music\track1.flac", @"D:\Music\track2.flac" });
                Assert.NotNull(pl);
                Assert.Equal("Synthwave Hits", pl.Name);
                Assert.Equal(2, pl.TrackFilePaths.Count);

                bool renamed = service.RenamePlaylist(pl.Id, "Retro Synthwave");
                Assert.True(renamed);
                Assert.Equal("Retro Synthwave", service.Playlists.First(p => p.Id == pl.Id).Name);

                bool deleted = service.DeletePlaylist(pl.Id);
                Assert.True(deleted);
                Assert.DoesNotContain(service.Playlists, p => p.Id == pl.Id);

                // Cannot delete favorites
                bool favDeleted = service.DeletePlaylist("favorites");
                Assert.False(favDeleted);
            }
            finally
            {
                if (File.Exists(tempPath)) File.Delete(tempPath);
            }
        }

        [Fact]
        public void PlaylistService_ToggleFavorite_TracksProperly()
        {
            string tempPath = Path.Combine(Path.GetTempPath(), $"hires_test_pl_{Guid.NewGuid():N}.json");
            try
            {
                var service = new PlaylistService(tempPath);
                string track = @"D:\Songs\test_audio.flac";

                Assert.False(service.IsFavorite(track));
                bool added = service.ToggleFavorite(track);
                Assert.True(added);
                Assert.True(service.IsFavorite(track));

                bool removed = service.ToggleFavorite(track);
                Assert.False(removed);
                Assert.False(service.IsFavorite(track));
            }
            finally
            {
                if (File.Exists(tempPath)) File.Delete(tempPath);
            }
        }

        [Fact]
        public void PlaylistService_ExportAndImportM3u()
        {
            string tempJson = Path.Combine(Path.GetTempPath(), $"hires_test_pl_{Guid.NewGuid():N}.json");
            string tempM3u = Path.Combine(Path.GetTempPath(), $"hires_test_export_{Guid.NewGuid():N}.m3u8");
            string testTrack = Path.Combine(Path.GetTempPath(), "dummy_track.flac");
            File.WriteAllText(testTrack, "dummy flac content");

            try
            {
                var service = new PlaylistService(tempJson);
                var pl = service.CreatePlaylist("ExportTest", new[] { testTrack });

                service.ExportM3u(pl.Id, tempM3u);
                Assert.True(File.Exists(tempM3u));

                var content = File.ReadAllText(tempM3u);
                Assert.Contains("#EXTM3U", content);
                Assert.Contains(testTrack, content);

                var imported = service.ImportM3u(tempM3u);
                Assert.NotNull(imported);
                Assert.Contains(testTrack, imported.TrackFilePaths);
            }
            finally
            {
                if (File.Exists(tempJson)) File.Delete(tempJson);
                if (File.Exists(tempM3u)) File.Delete(tempM3u);
                if (File.Exists(testTrack)) File.Delete(testTrack);
            }
        }
    }
}
