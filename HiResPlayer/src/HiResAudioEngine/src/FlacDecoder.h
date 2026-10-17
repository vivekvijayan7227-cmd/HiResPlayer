#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <atomic>
#include <FLAC/stream_decoder.h>

struct FlacMetadata {
    uint32_t sample_rate    = 0;
    uint32_t channels       = 0;
    uint32_t bits_per_sample= 0;
    uint64_t total_samples  = 0;
    // Vorbis comment tags
    std::wstring title;
    std::wstring artist;
    std::wstring album;
    std::wstring album_artist;
    uint32_t     track_number = 0;
    // Artwork (raw JPEG/PNG bytes if present)
    std::vector<uint8_t> artwork_data;
    std::string artwork_mime;
};

// Callback types
using FlacPcmCallback = std::function<void(
    const int32_t* const* samples,  // planar: samples[ch][i]
    uint32_t channels,
    uint32_t samples_per_channel,
    uint32_t bits_per_sample
)>;

using FlacErrorCallback = std::function<void(const char* description)>;

class FlacDecoder {
public:
    FlacDecoder();
    ~FlacDecoder();

    // Open and read STREAMINFO + tags. Returns false on failure.
    bool Open(const std::wstring& path, std::string& error_out);
    void Close();

    const FlacMetadata& metadata() const { return m_meta; }

    // Set callbacks before calling DecodeAll or DecodeSome
    void SetPcmCallback(FlacPcmCallback cb)   { m_pcm_cb  = std::move(cb); }
    void SetErrorCallback(FlacErrorCallback cb){ m_err_cb  = std::move(cb); }

    // Decode entire file — blocks until EOF or error
    bool DecodeAll(std::string& error_out);

    // Seek to a specific sample position (per-channel).
    // Must be called from decoder thread.
    bool SeekToSample(uint64_t sample_position, std::string& error_out);

    // Returns true if any decoder error has occurred since last reset.
    bool HasError() const { return m_has_error; }
    void ResetError() { m_has_error = false; }

    // Abort an in-progress decode (thread-safe via atomic flag)
    void RequestStop() { m_stop_requested.store(true, std::memory_order_release); }
    bool StopRequested() const { return m_stop_requested.load(std::memory_order_acquire); }

private:
    // libFLAC callback statics
    static FLAC__StreamDecoderReadStatus  StaticRead(const FLAC__StreamDecoder*, FLAC__byte[], size_t*, void*);
    static FLAC__StreamDecoderSeekStatus  StaticSeek(const FLAC__StreamDecoder*, FLAC__uint64, void*);
    static FLAC__StreamDecoderTellStatus  StaticTell(const FLAC__StreamDecoder*, FLAC__uint64*, void*);
    static FLAC__StreamDecoderLengthStatus StaticLength(const FLAC__StreamDecoder*, FLAC__uint64*, void*);
    static FLAC__bool                     StaticEof(const FLAC__StreamDecoder*, void*);
    static FLAC__StreamDecoderWriteStatus StaticWrite(const FLAC__StreamDecoder*,
                                                      const FLAC__Frame*,
                                                      const FLAC__int32* const[],
                                                      void*);
    static void StaticMetadata(const FLAC__StreamDecoder*, const FLAC__StreamMetadata*, void*);
    static void StaticError(const FLAC__StreamDecoder*, FLAC__StreamDecoderErrorStatus, void*);

    FLAC__StreamDecoder*  m_decoder = nullptr;
    FILE*                 m_file    = nullptr;
    FlacMetadata          m_meta;
    FlacPcmCallback       m_pcm_cb;
    FlacErrorCallback     m_err_cb;
    bool                  m_has_error       = false;
    bool                  m_meta_ready      = false;
    std::atomic<bool>     m_stop_requested  {false};
    std::atomic<bool>     m_seeking         {false};
};
