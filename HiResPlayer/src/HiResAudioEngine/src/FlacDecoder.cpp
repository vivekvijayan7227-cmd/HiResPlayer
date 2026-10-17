#include "FlacDecoder.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <codecvt>
#include <locale>

// Helper: wide to narrow UTF-8 for file open (Windows)
static std::string WideToUtf8(const std::wstring& ws) {
    if (ws.empty()) return {};
    int sz = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
    if (sz <= 0) return {};
    std::string s(sz, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), &s[0], sz, nullptr, nullptr);
    return s;
}

FlacDecoder::FlacDecoder() {
    m_decoder = FLAC__stream_decoder_new();
}

FlacDecoder::~FlacDecoder() {
    Close();
    if (m_decoder) {
        FLAC__stream_decoder_delete(m_decoder);
        m_decoder = nullptr;
    }
}

bool FlacDecoder::Open(const std::wstring& path, std::string& error_out) {
    Close();
    m_has_error.store(false, std::memory_order_release);
    m_stop_requested.store(false, std::memory_order_relaxed);
    m_seeking.store(false, std::memory_order_relaxed);
    m_meta_ready    = false;

    // Open file with wchar API on Windows
    m_file = _wfopen(path.c_str(), L"rb");
    if (!m_file) {
        error_out = "Cannot open file: " + WideToUtf8(path);
        return false;
    }

    // Request all metadata types we care about
    FLAC__stream_decoder_set_metadata_respond(m_decoder, FLAC__METADATA_TYPE_VORBIS_COMMENT);
    FLAC__stream_decoder_set_metadata_respond(m_decoder, FLAC__METADATA_TYPE_PICTURE);

    FLAC__StreamDecoderInitStatus init_status = FLAC__stream_decoder_init_stream(
        m_decoder,
        StaticRead, StaticSeek, StaticTell, StaticLength, StaticEof,
        StaticWrite, StaticMetadata, StaticError,
        this
    );

    if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
        error_out = std::string("libFLAC init failed: ") +
                    FLAC__StreamDecoderInitStatusString[init_status];
        fclose(m_file); m_file = nullptr;
        return false;
    }

    // Process metadata (reads STREAMINFO + tags)
    if (!FLAC__stream_decoder_process_until_end_of_metadata(m_decoder)) {
        error_out = "Failed to read FLAC metadata";
        FLAC__stream_decoder_finish(m_decoder);
        fclose(m_file); m_file = nullptr;
        return false;
    }

    if (m_meta.sample_rate == 0 || m_meta.channels == 0) {
        error_out = "FLAC STREAMINFO missing or invalid";
        FLAC__stream_decoder_finish(m_decoder);
        fclose(m_file); m_file = nullptr;
        return false;
    }

    return true;
}

void FlacDecoder::Close() {
    if (m_decoder) {
        FLAC__StreamDecoderState st = FLAC__stream_decoder_get_state(m_decoder);
        if (st != FLAC__STREAM_DECODER_UNINITIALIZED) {
            FLAC__stream_decoder_finish(m_decoder);
        }
    }
    if (m_file) {
        fclose(m_file);
        m_file = nullptr;
    }
    m_pcm_cb = nullptr;
    m_err_cb = nullptr;
    m_meta = {};
    m_has_error.store(false, std::memory_order_release);
    m_stop_requested.store(false, std::memory_order_relaxed);
    m_seeking.store(false, std::memory_order_relaxed);
}

bool FlacDecoder::DecodeAll(std::string& error_out) {
    if (!m_decoder || !m_file) {
        error_out = "Decoder not open";
        return false;
    }
    m_stop_requested.store(false, std::memory_order_relaxed);
    m_seeking.store(false, std::memory_order_relaxed);
    m_has_error.store(false, std::memory_order_release);
    FLAC__StreamDecoderState st = FLAC__stream_decoder_get_state(m_decoder);
    if (st == FLAC__STREAM_DECODER_ABORTED || st == FLAC__STREAM_DECODER_END_OF_STREAM || st == FLAC__STREAM_DECODER_SEEK_ERROR) {
        FLAC__stream_decoder_flush(m_decoder);
    }
    bool ok = FLAC__stream_decoder_process_until_end_of_stream(m_decoder) == FLAC__bool(1);
    if (!ok || m_has_error.load(std::memory_order_acquire)) {
        error_out = "FLAC decode error";
        return false;
    }
    return true;
}

bool FlacDecoder::SeekToSample(uint64_t sample_position, std::string& error_out) {
    if (!m_decoder || !m_file) {
        error_out = "Decoder not open";
        return false;
    }
    m_has_error.store(false, std::memory_order_release);
    m_stop_requested.store(false, std::memory_order_relaxed);
    m_seeking.store(true, std::memory_order_release);
    FLAC__StreamDecoderState st = FLAC__stream_decoder_get_state(m_decoder);
    if (st == FLAC__STREAM_DECODER_ABORTED || st == FLAC__STREAM_DECODER_END_OF_STREAM || st == FLAC__STREAM_DECODER_SEEK_ERROR) {
        FLAC__stream_decoder_flush(m_decoder);
    }
    bool ok = FLAC__stream_decoder_seek_absolute(m_decoder, sample_position) == FLAC__bool(1);
    m_seeking.store(false, std::memory_order_release);
    if (!ok) {
        FLAC__stream_decoder_flush(m_decoder);
        error_out = "Seek failed";
        return false;
    }
    return true;
}

// ─── libFLAC callbacks ────────────────────────────────────────────────────────

FLAC__StreamDecoderReadStatus FlacDecoder::StaticRead(
    const FLAC__StreamDecoder*, FLAC__byte buffer[], size_t* bytes, void* client
) {
    auto* self = static_cast<FlacDecoder*>(client);
    if (self->m_stop_requested.load(std::memory_order_relaxed)) return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
    *bytes = fread(buffer, 1, *bytes, self->m_file);
    if (*bytes == 0) {
        if (feof(self->m_file)) return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
        return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
    }
    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

FLAC__StreamDecoderSeekStatus FlacDecoder::StaticSeek(
    const FLAC__StreamDecoder*, FLAC__uint64 absolute_byte_offset, void* client
) {
    auto* self = static_cast<FlacDecoder*>(client);
    if (_fseeki64(self->m_file, (int64_t)absolute_byte_offset, SEEK_SET) != 0)
        return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
    return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

FLAC__StreamDecoderTellStatus FlacDecoder::StaticTell(
    const FLAC__StreamDecoder*, FLAC__uint64* absolute_byte_offset, void* client
) {
    auto* self = static_cast<FlacDecoder*>(client);
    int64_t pos = _ftelli64(self->m_file);
    if (pos < 0) return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
    *absolute_byte_offset = (FLAC__uint64)pos;
    return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

FLAC__StreamDecoderLengthStatus FlacDecoder::StaticLength(
    const FLAC__StreamDecoder*, FLAC__uint64* stream_length, void* client
) {
    auto* self = static_cast<FlacDecoder*>(client);
    int64_t cur = _ftelli64(self->m_file);
    if (_fseeki64(self->m_file, 0, SEEK_END) != 0) return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
    int64_t len = _ftelli64(self->m_file);
    _fseeki64(self->m_file, cur, SEEK_SET);
    if (len < 0) return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
    *stream_length = (FLAC__uint64)len;
    return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

FLAC__bool FlacDecoder::StaticEof(const FLAC__StreamDecoder*, void* client) {
    auto* self = static_cast<FlacDecoder*>(client);
    return feof(self->m_file) ? true : false;
}

FLAC__StreamDecoderWriteStatus FlacDecoder::StaticWrite(
    const FLAC__StreamDecoder*,
    const FLAC__Frame* frame,
    const FLAC__int32* const buffer[],
    void* client
) {
    auto* self = static_cast<FlacDecoder*>(client);
    if (self->m_stop_requested.load(std::memory_order_relaxed)) return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    if (self->m_seeking.load(std::memory_order_acquire)) return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
    if (self->m_pcm_cb) {
        self->m_pcm_cb(
            buffer,
            frame->header.channels,
            frame->header.blocksize,
            frame->header.bits_per_sample
        );
    }
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static std::wstring Utf8ToWide(const char* s, size_t len) {
    if (!s || len == 0) return {};
    int sz = MultiByteToWideChar(CP_UTF8, 0, s, (int)len, nullptr, 0);
    if (sz <= 0) return {};
    std::wstring ws(sz, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s, (int)len, &ws[0], sz);
    return ws;
}

static std::wstring ParseVorbisTag(const FLAC__StreamMetadata_VorbisComment& vc, const char* key) {
    std::string k(key);
    for (uint32_t i = 0; i < vc.num_comments; ++i) {
        const char* entry = reinterpret_cast<const char*>(vc.comments[i].entry);
        uint32_t len = vc.comments[i].length;
        // Find '='
        size_t eq = 0;
        for (; eq < len && entry[eq] != '='; ++eq) {}
        if (eq >= len) continue;
        // Case-insensitive key compare
        if (eq != k.size()) continue;
        bool match = true;
        for (size_t j = 0; j < eq; ++j) {
            if (toupper((unsigned char)entry[j]) != toupper((unsigned char)k[j])) {
                match = false; break;
            }
        }
        if (!match) continue;
        return Utf8ToWide(entry + eq + 1, len - eq - 1);
    }
    return {};
}

void FlacDecoder::StaticMetadata(
    const FLAC__StreamDecoder*,
    const FLAC__StreamMetadata* metadata,
    void* client
) {
    auto* self = static_cast<FlacDecoder*>(client);
    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
        self->m_meta.sample_rate     = metadata->data.stream_info.sample_rate;
        self->m_meta.channels        = metadata->data.stream_info.channels;
        self->m_meta.bits_per_sample = metadata->data.stream_info.bits_per_sample;
        self->m_meta.total_samples   = metadata->data.stream_info.total_samples;
        self->m_meta_ready = true;
    } else if (metadata->type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {
        const auto& vc = metadata->data.vorbis_comment;
        self->m_meta.title        = ParseVorbisTag(vc, "TITLE");
        self->m_meta.artist       = ParseVorbisTag(vc, "ARTIST");
        self->m_meta.album        = ParseVorbisTag(vc, "ALBUM");
        self->m_meta.album_artist = ParseVorbisTag(vc, "ALBUMARTIST");
        std::wstring tn = ParseVorbisTag(vc, "TRACKNUMBER");
        if (!tn.empty()) self->m_meta.track_number = (uint32_t)_wtoi(tn.c_str());
    } else if (metadata->type == FLAC__METADATA_TYPE_PICTURE) {
        const auto& pic = metadata->data.picture;
        // Take the first picture (usually front cover)
        if (self->m_meta.artwork_data.empty()) {
            self->m_meta.artwork_data.assign(pic.data, pic.data + pic.data_length);
            if (pic.mime_type) self->m_meta.artwork_mime = pic.mime_type;
        }
    }
}

void FlacDecoder::StaticError(
    const FLAC__StreamDecoder*,
    FLAC__StreamDecoderErrorStatus status,
    void* client
) {
    auto* self = static_cast<FlacDecoder*>(client);
    self->m_has_error.store(true, std::memory_order_release);
    if (self->m_err_cb) {
        self->m_err_cb(FLAC__StreamDecoderErrorStatusString[status]);
    }
}
