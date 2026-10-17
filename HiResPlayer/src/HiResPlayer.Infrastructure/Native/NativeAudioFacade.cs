using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace HiResPlayer.Infrastructure.Native
{
    public class NativeAudioFacade : IDisposable
    {
        private IntPtr _handle;
        private bool _disposed;
        private HP_PlaybackEventCallback? _playbackCallbackHolder;
        private HP_DeviceEventCallback? _deviceCallbackHolder;

        public event Action<uint, uint, int, ulong>? PlaybackEvent;
        public event Action<uint, string>? DeviceEvent;

        public NativeAudioFacade()
        {
            _handle = AudioEngineNative.Create();
            if (_handle == IntPtr.Zero)
            {
                throw new InvalidOperationException("Failed to create native audio engine handle.");
            }

            _playbackCallbackHolder = (userData, eventType, newState, hr, pos) =>
            {
                PlaybackEvent?.Invoke(eventType, newState, hr, pos);
            };
            AudioEngineNative.SetPlaybackEventCallback(_handle, _playbackCallbackHolder, IntPtr.Zero);

            _deviceCallbackHolder = (userData, eventType, endpointId) =>
            {
                DeviceEvent?.Invoke(eventType, endpointId ?? string.Empty);
            };
            AudioEngineNative.SetDeviceEventCallback(_handle, _deviceCallbackHolder, IntPtr.Zero);
        }

        public bool OpenFlac(string path)
        {
            EnsureNotDisposed();
            return AudioEngineNative.OpenFlac(_handle, path) == 0;
        }

        public bool SelectDevice(string endpointId)
        {
            EnsureNotDisposed();
            return AudioEngineNative.SelectDevice(_handle, endpointId) == 0;
        }

        public bool SetMode(HP_PlaybackMode mode)
        {
            EnsureNotDisposed();
            return AudioEngineNative.SetMode(_handle, mode) == 0;
        }

        public bool Play()
        {
            EnsureNotDisposed();
            return AudioEngineNative.Play(_handle) == 0;
        }

        public bool Pause()
        {
            EnsureNotDisposed();
            return AudioEngineNative.Pause(_handle) == 0;
        }

        public bool Stop()
        {
            EnsureNotDisposed();
            return AudioEngineNative.Stop(_handle) == 0;
        }

        public bool Seek(ulong samplePos)
        {
            EnsureNotDisposed();
            return AudioEngineNative.SeekSample(_handle, samplePos) == 0;
        }

        public bool SetVolume(float volume)
        {
            EnsureNotDisposed();
            return AudioEngineNative.SetVolume(_handle, volume) == 0;
        }

        public float GetVolume()
        {
            EnsureNotDisposed();
            return AudioEngineNative.GetVolume(_handle, out float vol) == 0 ? vol : 1.0f;
        }

        public HP_StatusV1 GetStatus()
        {
            EnsureNotDisposed();
            var status = new HP_StatusV1
            {
                StructureSize = (uint)Marshal.SizeOf<HP_StatusV1>()
            };
            AudioEngineNative.GetStatus(_handle, ref status);
            return status;
        }

        public unsafe List<AudioDeviceItem> EnumerateDevices()
        {
            EnsureNotDisposed();
            var list = new List<AudioDeviceItem>();
            AudioEngineNative.EnumerateDevices(_handle, null, 0, out uint count);
            if (count == 0) return list;

            var buffer = new HP_DeviceInfoV1[count];
            AudioEngineNative.EnumerateDevices(_handle, buffer, count, out count);

            for (int i = 0; i < count; i++)
            {
                string id;
                string name;
                fixed (char* pId = buffer[i].Id)
                {
                    id = new string(pId);
                }
                fixed (char* pName = buffer[i].FriendlyName)
                {
                    name = new string(pName);
                }

                list.Add(new AudioDeviceItem(
                    id.TrimEnd('\0'),
                    name.TrimEnd('\0'),
                    buffer[i].State,
                    buffer[i].IsDefaultRender != 0
                ));
            }
            return list;
        }

        public string GetStatusMessage()
        {
            EnsureNotDisposed();
            var buf = new char[256];
            int res = AudioEngineNative.GetStatusMessage(_handle, buf, 256);
            return res == 0 ? new string(buf).TrimEnd('\0') : "Ready";
        }

        // DSP Extensions (M7 & M8)
        public void SetPreamp(float preampDb)
        {
            EnsureNotDisposed();
            AudioEngineNative.SetPreamp(_handle, preampDb);
        }

        public void SetEqBypass(bool bypass)
        {
            EnsureNotDisposed();
            AudioEngineNative.SetEqBypass(_handle, bypass ? 1u : 0u);
        }

        public void SetGraphicEqBand(uint band, float gainDb)
        {
            EnsureNotDisposed();
            AudioEngineNative.SetGraphicEqBand(_handle, band, gainDb);
        }

        public void ResetGraphicEq()
        {
            EnsureNotDisposed();
            AudioEngineNative.ResetGraphicEq(_handle);
        }

        public void SetParametricEqBand(uint band, bool enabled, uint filterType, float freq, float gainDb, float q)
        {
            EnsureNotDisposed();
            AudioEngineNative.SetParametricEqBand(_handle, band, enabled ? 1u : 0u, filterType, freq, gainDb, q);
        }

        public void ResetParametricEq()
        {
            EnsureNotDisposed();
            AudioEngineNative.ResetParametricEq(_handle);
        }

        public float EvaluateCurve(float freqHz)
        {
            EnsureNotDisposed();
            AudioEngineNative.EvaluateCurve(_handle, freqHz, out float outDb);
            return outDb;
        }

        public float EvaluateCenterCurve(float freqHz)
        {
            EnsureNotDisposed();
            AudioEngineNative.EvaluateCenterCurve(_handle, freqHz, out float outDb);
            return outDb;
        }

        public (float Left, float Right, bool Clipped) GetPeaks()
        {
            EnsureNotDisposed();
            AudioEngineNative.GetPeaks(_handle, out float l, out float r, out uint clipped);
            return (l, r, clipped != 0);
        }

        public void SetCenterProcessor(uint mode, float amount)
        {
            EnsureNotDisposed();
            AudioEngineNative.SetCenterProcessor(_handle, mode, amount);
        }

        public void SetStemMixer(float vocalVol, float instVol, bool muteVocal, bool muteInst)
        {
            EnsureNotDisposed();
            AudioEngineNative.SetStemMixer(_handle, vocalVol, instVol, muteVocal ? 1u : 0u, muteInst ? 1u : 0u);
        }

        public void DisableStemMixer()
        {
            EnsureNotDisposed();
            AudioEngineNative.DisableStemMixer(_handle);
        }

        public unsafe void GetSpectrum(float[] outBands)
        {
            EnsureNotDisposed();
            if (outBands == null || outBands.Length == 0) return;
            fixed (float* p = outBands)
            {
                AudioEngineNative.GetSpectrum(_handle, p, (uint)outBands.Length);
            }
        }

        public float GetStereoCorrelation()
        {
            EnsureNotDisposed();
            AudioEngineNative.GetStereoCorrelation(_handle, out float corr);
            return corr;
        }

        public (float InL, float InR, float OutL, float OutR, float LimiterGr, bool Clipped) GetDetailedMeters()
        {
            EnsureNotDisposed();
            AudioEngineNative.GetDetailedMeters(_handle, out float inL, out float inR, out float outL, out float outR, out float limiterGr, out uint clipped);
            return (inL, inR, outL, outR, limiterGr, clipped != 0);
        }

        // Bluetooth Architecture Methods
        public unsafe BluetoothDeviceInfo GetBluetoothDeviceInfo(string endpointId)
        {
            EnsureNotDisposed();
            var info = new HP_BluetoothDeviceInfoV1
            {
                StructureSize = (uint)sizeof(HP_BluetoothDeviceInfoV1)
            };
            int res = AudioEngineNative.GetBluetoothDeviceInfo(_handle, endpointId ?? string.Empty, ref info);
            if (res != 0)
            {
                return new BluetoothDeviceInfo(false, HP_TransportType.Unknown, HP_BluetoothProfile.None, HP_BluetoothCodec.Unknown, HP_TelemetryConfidence.Unknown, 0, 0, 0, false, "", "", "");
            }

            string adapter = new string(info.AdapterName).TrimEnd('\0');
            string address = new string(info.DeviceAddress).TrimEnd('\0');
            string driver = new string(info.DriverName).TrimEnd('\0');

            return new BluetoothDeviceInfo(
                info.IsBluetooth != 0,
                (HP_TransportType)info.TransportType,
                (HP_BluetoothProfile)info.BtProfile,
                (HP_BluetoothCodec)info.DetectedCodec,
                (HP_TelemetryConfidence)info.CodecConfidence,
                info.NativeMixRate,
                info.NativeMixBits,
                info.NativeChannels,
                info.IsExclusiveSupported != 0,
                adapter,
                address,
                driver
            );
        }

        public bool SetBluetoothFidelityMode(bool enabled)
        {
            EnsureNotDisposed();
            return AudioEngineNative.SetBluetoothFidelityMode(_handle, enabled ? 1u : 0u) == 0;
        }

        public unsafe BluetoothFidelityStatus GetBluetoothFidelityStatus()
        {
            EnsureNotDisposed();
            var status = new HP_BluetoothFidelityStatusV1
            {
                StructureSize = (uint)sizeof(HP_BluetoothFidelityStatusV1)
            };
            int res = AudioEngineNative.GetBluetoothFidelityStatus(_handle, ref status);
            if (res != 0)
            {
                return new BluetoothFidelityStatus(false, false, 0, 0, false, HP_BluetoothCodec.Unknown, HP_TelemetryConfidence.Unknown, 0, 0, "Unknown");
            }

            string summary = new string(status.StatusSummary).TrimEnd('\0');

            return new BluetoothFidelityStatus(
                status.FidelityModeEnabled != 0,
                status.PcmTransparent != 0,
                status.OutputFormatMatched,
                status.WirelessLosslessState,
                status.IsHfpWarning != 0,
                (HP_BluetoothCodec)status.ActiveCodec,
                (HP_TelemetryConfidence)status.CodecConfidence,
                status.EstimatedBitrateKbps,
                status.UnderrunCount,
                summary
            );
        }

        private void EnsureNotDisposed()
        {
            if (_disposed) throw new ObjectDisposedException(nameof(NativeAudioFacade));
        }

        public void Dispose()
        {
            if (!_disposed)
            {
                if (_handle != IntPtr.Zero)
                {
                    AudioEngineNative.Destroy(_handle);
                    _handle = IntPtr.Zero;
                }
                _disposed = true;
                GC.SuppressFinalize(this);
            }
        }

        ~NativeAudioFacade()
        {
            Dispose();
        }
    }
}
