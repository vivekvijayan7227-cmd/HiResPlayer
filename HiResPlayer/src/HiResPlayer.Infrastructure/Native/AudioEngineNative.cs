using System;
using System.Runtime.InteropServices;

namespace HiResPlayer.Infrastructure.Native
{
    public enum HP_PlaybackMode : uint
    {
        Shared = 0,
        Exclusive = 1
    }

    public enum HP_State : uint
    {
        Uninitialized = 0,
        Ready = 1,
        Loading = 2,
        Primed = 3,
        Playing = 4,
        Paused = 5,
        Seeking = 6,
        SwitchingDevice = 7,
        RecoverableError = 8,
        Stopped = 9,
        Disposed = 10
    }

    public enum HP_EventType : uint
    {
        DeviceAdded = 1,
        DeviceRemoved = 2,
        DeviceStateChanged = 3,
        DefaultDeviceChanged = 4,
        StateChanged = 10,
        TrackEnd = 11,
        Underrun = 12,
        Error = 13
    }


    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public unsafe struct HP_StatusV1
    {
        public uint StructureSize;
        public uint State;
        public uint SourceRate;
        public uint SourceValidBits;
        public uint SourceChannels;
        public ulong SourceTotalSamples;
        public uint OutputRate;
        public uint OutputValidBits;
        public uint OutputContainerBits;
        public uint OutputChannels;
        public uint ActualMode;
        public uint BitperfectEligible;
        public uint BpNoDecoderErrors;
        public uint BpRateMatch;
        public uint BpBitsMatch;
        public uint BpChannelsMatch;
        public uint BpExclusiveActive;
        public uint BpDspDisabled;
        public uint BpPcmPassthrough;
        public ulong RenderedSourceSamples;
        public ulong UnderrunCount;
        public int LastHresult;
        public uint StatusMessageId;
        public fixed char EndpointId[256];
        public fixed char EndpointFriendlyName[256];
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public unsafe struct HP_DeviceInfoV1
    {
        public uint StructureSize;
        public fixed char Id[256];
        public fixed char FriendlyName[256];
        public uint State;
        public uint IsDefaultRender;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct HP_FormatCapV1
    {
        public uint StructureSize;
        public uint SampleRate;
        public uint ValidBits;
        public uint ContainerBits;
        public uint Channels;
        public uint ExclusiveSupported;
        public uint SharedMixRate;
        public uint SharedMixBits;
        public int ProbeHresult;
    }

    public enum HP_TransportType : uint
    {
        Unknown = 0,
        Builtin = 1,
        Usb = 2,
        Bluetooth = 3
    }

    public enum HP_BluetoothProfile : uint
    {
        None = 0,
        Unknown = 1,
        A2DP = 2,
        Hfp = 3,
        LeAudio = 4
    }

    public enum HP_BluetoothCodec : uint
    {
        Unknown = 0,
        Sbc = 1,
        Aac = 2,
        Aptx = 3,
        AptxHd = 4,
        AptxAdaptive = 5,
        AptxLossless = 6,
        Ldac = 7,
        Lc3 = 8
    }

    public enum HP_TelemetryConfidence : uint
    {
        Unknown = 0,
        Inferred = 1,
        ReportedByDriver = 2,
        Verified = 3,
        Unsupported = 4
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public unsafe struct HP_BluetoothDeviceInfoV1
    {
        public uint StructureSize;
        public uint IsBluetooth;
        public uint TransportType;
        public uint BtProfile;
        public uint DetectedCodec;
        public uint CodecConfidence;
        public uint NativeMixRate;
        public uint NativeMixBits;
        public uint NativeChannels;
        public uint IsExclusiveSupported;
        public fixed char AdapterName[128];
        public fixed char DeviceAddress[32];
        public fixed char DriverName[128];
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public unsafe struct HP_BluetoothFidelityStatusV1
    {
        public uint StructureSize;
        public uint FidelityModeEnabled;
        public uint PcmTransparent;
        public uint OutputFormatMatched;
        public uint WirelessLosslessState;
        public uint IsHfpWarning;
        public uint ActiveCodec;
        public uint CodecConfidence;
        public uint EstimatedBitrateKbps;
        public ulong UnderrunCount;
        public fixed char StatusSummary[256];
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void HP_DeviceEventCallback(
        IntPtr userData,
        uint eventType,
        [MarshalAs(UnmanagedType.LPWStr)] string endpointId
    );

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void HP_PlaybackEventCallback(
        IntPtr userData,
        uint eventType,
        uint newState,
        int hresult,
        ulong positionSamples
    );

    public static unsafe partial class AudioEngineNative
    {
        private const string LibName = "HiResAudioEngine.dll";

        [LibraryImport(LibName, EntryPoint = "hp_api_version")]
        public static partial uint ApiVersion();

        [LibraryImport(LibName, EntryPoint = "hp_create")]
        public static partial IntPtr Create();

        [LibraryImport(LibName, EntryPoint = "hp_destroy")]
        public static partial void Destroy(IntPtr handle);

        [LibraryImport(LibName, EntryPoint = "hp_open_flac", StringMarshalling = StringMarshalling.Utf16)]
        public static partial int OpenFlac(IntPtr handle, string path);

        [LibraryImport(LibName, EntryPoint = "hp_select_device", StringMarshalling = StringMarshalling.Utf16)]
        public static partial int SelectDevice(IntPtr handle, string endpointId);

        [LibraryImport(LibName, EntryPoint = "hp_set_mode")]
        public static partial int SetMode(IntPtr handle, HP_PlaybackMode mode);

        [LibraryImport(LibName, EntryPoint = "hp_play")]
        public static partial int Play(IntPtr handle);

        [LibraryImport(LibName, EntryPoint = "hp_pause")]
        public static partial int Pause(IntPtr handle);

        [LibraryImport(LibName, EntryPoint = "hp_stop")]
        public static partial int Stop(IntPtr handle);

        [LibraryImport(LibName, EntryPoint = "hp_seek_sample")]
        public static partial int SeekSample(IntPtr handle, ulong samplePosition);

        [LibraryImport(LibName, EntryPoint = "hp_set_volume")]
        public static partial int SetVolume(IntPtr handle, float volume);

        [LibraryImport(LibName, EntryPoint = "hp_get_volume")]
        public static partial int GetVolume(IntPtr handle, out float volume);

        [LibraryImport(LibName, EntryPoint = "hp_get_status")]
        public static partial int GetStatus(IntPtr handle, ref HP_StatusV1 status);

        [LibraryImport(LibName, EntryPoint = "hp_enumerate_devices")]
        public static partial int EnumerateDevices(
            IntPtr handle,
            [Out] HP_DeviceInfoV1[]? buffer,
            uint bufferCount,
            out uint count
        );

        [LibraryImport(LibName, EntryPoint = "hp_query_format", StringMarshalling = StringMarshalling.Utf16)]
        public static partial int QueryFormat(
            IntPtr handle,
            string endpointId,
            HP_PlaybackMode mode,
            ref HP_FormatCapV1 fmt
        );

        [LibraryImport(LibName, EntryPoint = "hp_set_device_event_callback")]
        public static partial int SetDeviceEventCallback(
            IntPtr handle,
            HP_DeviceEventCallback callback,
            IntPtr userData
        );

        [LibraryImport(LibName, EntryPoint = "hp_set_playback_event_callback")]
        public static partial int SetPlaybackEventCallback(
            IntPtr handle,
            HP_PlaybackEventCallback callback,
            IntPtr userData
        );

        [LibraryImport(LibName, EntryPoint = "hp_get_status_message", StringMarshalling = StringMarshalling.Utf16)]
        public static partial int GetStatusMessage(
            IntPtr handle,
            [Out] char[] buffer,
            uint bufferWcharCount
        );

        // DSP Extensions (M7 & M8)
        [LibraryImport(LibName, EntryPoint = "hp_dsp_set_preamp")]
        public static partial int SetPreamp(IntPtr handle, float preampDb);

        [LibraryImport(LibName, EntryPoint = "hp_dsp_set_eq_bypass")]
        public static partial int SetEqBypass(IntPtr handle, uint bypass);

        [LibraryImport(LibName, EntryPoint = "hp_dsp_set_graphic_eq_band")]
        public static partial int SetGraphicEqBand(IntPtr handle, uint bandIndex, float gainDb);

        [LibraryImport(LibName, EntryPoint = "hp_dsp_reset_graphic_eq")]
        public static partial int ResetGraphicEq(IntPtr handle);

        [LibraryImport(LibName, EntryPoint = "hp_dsp_set_parametric_eq_band")]
        public static partial int SetParametricEqBand(
            IntPtr handle,
            uint bandIndex,
            uint enabled,
            uint filterType,
            float frequency,
            float gainDb,
            float q
        );

        [LibraryImport(LibName, EntryPoint = "hp_dsp_reset_parametric_eq")]
        public static partial int ResetParametricEq(IntPtr handle);

        [LibraryImport(LibName, EntryPoint = "hp_dsp_evaluate_curve")]
        public static partial int EvaluateCurve(IntPtr handle, float freqHz, out float outDb);

        [LibraryImport(LibName, EntryPoint = "hp_dsp_evaluate_center_curve")]
        public static partial int EvaluateCenterCurve(IntPtr handle, float freqHz, out float outDb);

        [LibraryImport(LibName, EntryPoint = "hp_dsp_get_peaks")]
        public static partial int GetPeaks(IntPtr handle, out float outPeakL, out float outPeakR, out uint outClipped);

        [LibraryImport(LibName, EntryPoint = "hp_dsp_reset_peaks")]
        public static partial int ResetPeaks(IntPtr handle);

        [LibraryImport(LibName, EntryPoint = "hp_dsp_set_center_processor")]
        public static partial int SetCenterProcessor(IntPtr handle, uint mode, float amount);

        [LibraryImport(LibName, EntryPoint = "hp_dsp_set_stem_mixer")]
        public static partial int SetStemMixer(
            IntPtr handle,
            float vocalVolume,
            float instVolume,
            uint muteVocal,
            uint muteInst
        );

        [LibraryImport(LibName, EntryPoint = "hp_dsp_disable_stem_mixer")]
        public static partial int DisableStemMixer(IntPtr handle);

        [LibraryImport(LibName, EntryPoint = "hp_dsp_get_spectrum")]
        public static unsafe partial int GetSpectrum(IntPtr handle, float* outBands, uint bandCount);

        [LibraryImport(LibName, EntryPoint = "hp_dsp_get_stereo_correlation")]
        public static partial int GetStereoCorrelation(IntPtr handle, out float outCorrelation);

        [LibraryImport(LibName, EntryPoint = "hp_dsp_get_detailed_meters")]
        public static partial int GetDetailedMeters(
            IntPtr handle,
            out float outInPeakL,
            out float outInPeakR,
            out float outOutPeakL,
            out float outOutPeakR,
            out float outLimiterGr,
            out uint outClipped
        );

        // Bluetooth Architecture API
        [LibraryImport(LibName, EntryPoint = "hp_get_bluetooth_device_info", StringMarshalling = StringMarshalling.Utf16)]
        public static partial int GetBluetoothDeviceInfo(
            IntPtr handle,
            string endpointId,
            ref HP_BluetoothDeviceInfoV1 info
        );

        [LibraryImport(LibName, EntryPoint = "hp_set_bluetooth_fidelity_mode")]
        public static partial int SetBluetoothFidelityMode(
            IntPtr handle,
            uint enabled
        );

        [LibraryImport(LibName, EntryPoint = "hp_get_bluetooth_fidelity_status")]
        public static partial int GetBluetoothFidelityStatus(
            IntPtr handle,
            ref HP_BluetoothFidelityStatusV1 status
        );

        [LibraryImport(LibName, EntryPoint = "hp_verify_flac_pcm_exact", StringMarshalling = StringMarshalling.Utf16)]
        public static partial int VerifyFlacPcmExact(
            string flacPath,
            string wavPath,
            out uint outMismatches,
            out ulong outComparedSamples
        );
    }
}
