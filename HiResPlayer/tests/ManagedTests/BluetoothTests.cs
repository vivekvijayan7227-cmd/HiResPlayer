using System;
using Xunit;
using HiResPlayer.Infrastructure.Native;

namespace HiResPlayer.Tests
{
    public class BluetoothTests
    {
        [Fact]
        public void BluetoothDeviceInfo_DisplayProperties_FormatCorrectly()
        {
            var info = new BluetoothDeviceInfo(
                IsBluetooth: true,
                TransportType: HP_TransportType.Bluetooth,
                Profile: HP_BluetoothProfile.A2DP,
                DetectedCodec: HP_BluetoothCodec.Aac,
                CodecConfidence: HP_TelemetryConfidence.Inferred,
                NativeMixRate: 48000,
                NativeMixBits: 24,
                NativeChannels: 2,
                IsExclusiveSupported: false,
                AdapterName: "Intel(R) Wireless Bluetooth(R)",
                DeviceAddress: "40:72:18:EE:E8:57",
                DriverName: "Microsoft Bluetooth A2DP Driver"
            );

            Assert.True(info.IsBluetooth);
            Assert.Equal("A2DP Stereo (Media Sink)", info.ProfileDisplayName);
            Assert.Equal("AAC", info.CodecDisplayName);
            Assert.Equal("Inferred", info.ConfidenceBadge);
            Assert.False(info.IsExclusiveSupported);
            Assert.Equal("40:72:18:EE:E8:57", info.DeviceAddress);
        }

        [Fact]
        public void BluetoothFidelityStatus_LossyState_NeverReportsLossless()
        {
            var status = new BluetoothFidelityStatus(
                FidelityModeEnabled: true,
                PcmTransparent: true,
                OutputFormatMatched: 1,
                WirelessLosslessState: 0, // Lossy Bluetooth
                IsHfpWarning: false,
                ActiveCodec: HP_BluetoothCodec.Aac,
                CodecConfidence: HP_TelemetryConfidence.Inferred,
                EstimatedBitrateKbps: 256,
                UnderrunCount: 0,
                StatusSummary: "Bluetooth Fidelity Mode active: A2DP Stereo (AAC [Inferred], 48000 Hz, 24-bit). Signal path is bit-transparent."
            );

            Assert.True(status.FidelityModeEnabled);
            Assert.True(status.PcmTransparent);
            Assert.Equal("100% Bit-Transparent (Exact Rate Match, Zero DSP)", status.PcmPathDescription);
            Assert.Equal("Lossy Bluetooth (Perceptual Compression - Not Lossless)", status.WirelessLosslessDescription);
            Assert.Contains("AAC", status.CodecDisplayName);
            Assert.Equal("Inferred", status.ConfidenceBadge);
        }

        [Fact]
        public void NativeAudioFacade_BluetoothFidelityMode_ToggleAndStatusInspection()
        {
            using var facade = new NativeAudioFacade();

            // Initial status: Bluetooth Fidelity Mode defaults to enabled for high-fidelity resampling
            var initialStatus = facade.GetBluetoothFidelityStatus();
            Assert.True(initialStatus.FidelityModeEnabled);
            Assert.Equal(0u, initialStatus.WirelessLosslessState); // Must be 0 (lossy)

            // Disable Fidelity Mode
            bool disabled = facade.SetBluetoothFidelityMode(false);
            Assert.True(disabled);

            var revertedStatus = facade.GetBluetoothFidelityStatus();
            Assert.False(revertedStatus.FidelityModeEnabled);

            // Re-enable Fidelity Mode
            bool enabled = facade.SetBluetoothFidelityMode(true);
            Assert.True(enabled);

            var activeStatus = facade.GetBluetoothFidelityStatus();
            Assert.True(activeStatus.FidelityModeEnabled);
            Assert.Equal(0u, activeStatus.WirelessLosslessState); // Must be 0 (lossy)
            Assert.NotNull(activeStatus.StatusSummary);
        }

        [Fact]
        [Trait("Category", "RequiresPairedBluetooth")]
        public void NativeAudioFacade_GetBluetoothDeviceInfo_ClassifiesKnownPairedBluetoothDevice()
        {
            using var facade = new NativeAudioFacade();

            // Query using Sony WH-CH720N device ID known to be in developer's registry
            string sonyEndpointId = "{0.0.0.00000000}.{af2282dc-936d-4e89-ad2c-a8eee685fa11}";
            var info = facade.GetBluetoothDeviceInfo(sonyEndpointId);
            if (!info.IsBluetooth)
            {
                // Headless CI or clean host without this specific paired hardware device
                return;
            }

            Assert.True(info.IsBluetooth);
            Assert.Equal(HP_TransportType.Bluetooth, info.TransportType);
            Assert.Equal(HP_BluetoothProfile.A2DP, info.Profile);
            Assert.Equal(HP_BluetoothCodec.Aac, info.DetectedCodec);
            Assert.Contains("Intel", info.AdapterName);
            Assert.Contains("Microsoft Bluetooth A2DP Driver", info.DriverName);
            Assert.False(info.IsExclusiveSupported);
        }

        [Fact]
        public void NativeAudioFacade_GetBluetoothDeviceInfo_NonBluetoothEndpoint()
        {
            using var facade = new NativeAudioFacade();

            // Query with an empty string or dummy endpoint
            var info = facade.GetBluetoothDeviceInfo("dummy-internal-speaker-endpoint");

            // Should safely identify as non-bluetooth without throwing
            Assert.False(info.IsBluetooth);
            Assert.NotEqual(HP_TransportType.Bluetooth, info.TransportType);
        }
    }
}
