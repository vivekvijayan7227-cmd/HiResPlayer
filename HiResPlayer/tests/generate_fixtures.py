import math
import struct
import wave
import os
import subprocess

def create_sine_wav(path, sample_rate, bit_depth, channels, duration_sec, freq=440.0):
    total_frames = int(sample_rate * duration_sec)
    with wave.open(path, 'wb') as wav:
        wav.setnchannels(channels)
        wav.setsampwidth(bit_depth // 8)
        wav.setframerate(sample_rate)
        
        amplitude = 0.5 * (2**(bit_depth - 1) - 1)
        frames = bytearray()
        for i in range(total_frames):
            t = float(i) / float(sample_rate)
            sample_val = int(amplitude * math.sin(2.0 * math.pi * freq * t))
            if bit_depth == 16:
                data = struct.pack('<h', sample_val)
            elif bit_depth == 24:
                # 3 bytes little endian signed integer
                sample_val = max(-8388608, min(8388607, sample_val))
                if sample_val < 0:
                    sample_val += 16777216
                data = bytes([sample_val & 0xFF, (sample_val >> 8) & 0xFF, (sample_val >> 16) & 0xFF])
            else:
                data = struct.pack('<i', sample_val)
            for _ in range(channels):
                frames.extend(data)
        wav.writeframes(frames)

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    fixtures_dir = os.path.join(script_dir, "Fixtures")
    os.makedirs(fixtures_dir, exist_ok=True)
    
    # 1. 16-bit 44.1kHz Stereo Sine Wave
    wav_16_44 = os.path.join(fixtures_dir, "test_16bit_44k.wav")
    flac_16_44 = os.path.join(fixtures_dir, "test_16bit_44k.flac")
    create_sine_wav(wav_16_44, 44100, 16, 2, 2.0, 440.0)
    
    # 2. 24-bit 96kHz Hi-Res Stereo Sine Wave
    wav_24_96 = os.path.join(fixtures_dir, "test_24bit_96k.wav")
    flac_24_96 = os.path.join(fixtures_dir, "test_24bit_96k.flac")
    create_sine_wav(wav_24_96, 96000, 24, 2, 2.0, 1000.0)
    
    # Convert to FLAC using built flac-1.4.3 test utility or python flac
    flac_exe = os.path.join(script_dir, "..", "..", "flac-1.4.3", "build", "objs", "flac.exe")
    # If flac.exe not built (we only built lib), write basic wav-to-flac or test directly
    print(f"Created reference WAV fixtures:\n  {wav_16_44}\n  {wav_24_96}")

if __name__ == "__main__":
    main()
