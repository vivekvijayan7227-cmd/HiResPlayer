namespace HiResPlayer.Infrastructure.Native
{
    public record BluetoothDeviceInfo(
        bool IsBluetooth,
        HP_TransportType TransportType,
        HP_BluetoothProfile Profile,
        HP_BluetoothCodec DetectedCodec,
        HP_TelemetryConfidence CodecConfidence,
        uint NativeMixRate,
        uint NativeMixBits,
        uint NativeChannels,
        bool IsExclusiveSupported,
        string AdapterName,
        string DeviceAddress,
        string DriverName
    )
    {
        public string ProfileDisplayName => Profile switch
        {
            HP_BluetoothProfile.A2DP => "A2DP Stereo (Media Sink)",
            HP_BluetoothProfile.Hfp => "Hands-Free (HFP Telephony)",
            HP_BluetoothProfile.LeAudio => "LE Audio (LC3)",
            _ => "Standard Output"
        };

        public string CodecDisplayName => DetectedCodec switch
        {
            HP_BluetoothCodec.Sbc => "SBC",
            HP_BluetoothCodec.Aac => "AAC",
            HP_BluetoothCodec.Aptx => "aptX",
            HP_BluetoothCodec.AptxHd => "aptX HD",
            HP_BluetoothCodec.AptxAdaptive => "aptX Adaptive",
            HP_BluetoothCodec.AptxLossless => "aptX Lossless",
            HP_BluetoothCodec.Ldac => "LDAC",
            HP_BluetoothCodec.Lc3 => "LC3",
            _ => "Unknown Codec"
        };

        public string ConfidenceBadge => CodecConfidence switch
        {
            HP_TelemetryConfidence.Verified => "Verified",
            HP_TelemetryConfidence.ReportedByDriver => "Reported by Driver",
            HP_TelemetryConfidence.Inferred => "Inferred",
            HP_TelemetryConfidence.Unsupported => "Unsupported",
            _ => "Unknown"
        };
    }

    public record BluetoothFidelityStatus(
        bool FidelityModeEnabled,
        bool PcmTransparent,
        uint OutputFormatMatched, // 1 = Exact Match, 2 = Sinc Resampled
        uint WirelessLosslessState, // 0 = Lossy Bluetooth, 1 = Lossless
        bool IsHfpWarning,
        HP_BluetoothCodec ActiveCodec,
        HP_TelemetryConfidence CodecConfidence,
        uint EstimatedBitrateKbps,
        ulong UnderrunCount,
        string StatusSummary
    )
    {
        public string CodecDisplayName => ActiveCodec switch
        {
            HP_BluetoothCodec.Sbc => "SBC",
            HP_BluetoothCodec.Aac => "AAC",
            HP_BluetoothCodec.Aptx => "aptX",
            HP_BluetoothCodec.AptxHd => "aptX HD",
            HP_BluetoothCodec.AptxAdaptive => "aptX Adaptive",
            HP_BluetoothCodec.AptxLossless => "aptX Lossless",
            HP_BluetoothCodec.Ldac => "LDAC",
            HP_BluetoothCodec.Lc3 => "LC3",
            _ => "Standard A2DP"
        };

        public string ConfidenceBadge => CodecConfidence switch
        {
            HP_TelemetryConfidence.Verified => "Verified",
            HP_TelemetryConfidence.ReportedByDriver => "Reported by Driver",
            HP_TelemetryConfidence.Inferred => "Inferred",
            HP_TelemetryConfidence.Unsupported => "Unsupported",
            _ => "Unknown"
        };

        public string PcmPathDescription => (PcmTransparent, OutputFormatMatched) switch
        {
            (true, 1) => "100% Bit-Transparent (Exact Rate Match, Zero DSP)",
            (true, 2) => "Transparent Kaiser Polyphase Resampling (Source != Endpoint Rate)",
            (false, _) => "Studio DSP Processing Active",
            _ => "Direct Windows Audio Pipeline"
        };

        public string WirelessLosslessDescription => WirelessLosslessState switch
        {
            1 => "Verified Wireless Lossless (aptX Lossless)",
            _ => "Lossy Bluetooth (Perceptual Compression - Not Lossless)"
        };
    }
}
