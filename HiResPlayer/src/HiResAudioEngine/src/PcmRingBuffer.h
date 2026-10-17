#pragma once
#include <atomic>
#include <cstdint>
#include <cstring>
#include <vector>
#include <thread>
#include <algorithm>

// Single-producer / single-consumer lock-free PCM ring buffer.
// Producer and consumer run on separate threads (decoder thread and WASAPI render thread).
// Supports a "generation" counter so stale data can be discarded after seeks.
// Guaranteed whole PCM audio frame alignment across all writes, reads, and buffer wrap-arounds.
// Thread-safe reset protocol ensures in-flight reads/writes finish before positions are zeroed.
class PcmRingBuffer {
public:
    explicit PcmRingBuffer(size_t capacity_bytes)
        : m_buf(capacity_bytes),
          m_raw_capacity(capacity_bytes),
          m_frame_size(4),
          m_read_pos(0), m_write_pos(0),
          m_write_gen(0), m_read_gen(0), m_underruns(0),
          m_active_readers(0), m_active_writers(0)
    {
        update_usable_capacity();
    }

    // Configure the audio frame boundary in bytes (e.g. channels * bytes_per_sample)
    void set_frame_size(size_t frame_bytes) noexcept {
        if (frame_bytes == 0) frame_bytes = 4;
        m_frame_size.store(frame_bytes, std::memory_order_release);
        update_usable_capacity();
    }

    size_t get_frame_size() const noexcept {
        return m_frame_size.load(std::memory_order_acquire);
    }

    // Producer side: available write space in bytes (always whole audio frames)
    size_t available_write() const noexcept {
        const size_t cap = m_usable_capacity.load(std::memory_order_relaxed);
        const size_t fsize = m_frame_size.load(std::memory_order_relaxed);
        if (cap == 0 || fsize == 0) return 0;

        const size_t r = m_read_pos.load(std::memory_order_acquire);
        const size_t w = m_write_pos.load(std::memory_order_relaxed);
        const size_t used = (w - r + cap) % cap;

        // Reserve at least one full frame to distinguish full from empty
        if (used + fsize >= cap) return 0;
        const size_t avail = cap - used - fsize;
        return (avail / fsize) * fsize;
    }

    // Returns bytes actually written (always an integer multiple of frame_size)
    size_t write(const uint8_t* data, size_t bytes, uint64_t gen) noexcept {
        if (gen == 0 || gen != m_write_gen.load(std::memory_order_acquire)) return 0;

        m_active_writers.fetch_add(1, std::memory_order_acquire);
        struct WriterGuard {
            std::atomic<int32_t>& count;
            ~WriterGuard() { count.fetch_sub(1, std::memory_order_release); }
        } guard{m_active_writers};

        // Re-check generation after acquiring active writer count
        if (gen != m_write_gen.load(std::memory_order_acquire)) return 0;

        const size_t cap = m_usable_capacity.load(std::memory_order_relaxed);
        const size_t fsize = m_frame_size.load(std::memory_order_relaxed);
        if (cap == 0 || fsize == 0) return 0;

        const size_t avail = available_write();
        size_t to_write = (bytes < avail) ? bytes : avail;
        to_write = (to_write / fsize) * fsize;
        if (to_write == 0) return 0;

        const size_t w = m_write_pos.load(std::memory_order_relaxed);
        const size_t first = cap - w;
        if (first >= to_write) {
            std::memcpy(m_buf.data() + w, data, to_write);
        } else {
            std::memcpy(m_buf.data() + w, data, first);
            std::memcpy(m_buf.data(), data + first, to_write - first);
        }
        m_write_pos.store((w + to_write) % cap, std::memory_order_release);
        return to_write;
    }

    // Consumer side: available read space in bytes (always whole audio frames)
    size_t available_read() const noexcept {
        const size_t cap = m_usable_capacity.load(std::memory_order_relaxed);
        const size_t fsize = m_frame_size.load(std::memory_order_relaxed);
        if (cap == 0 || fsize == 0) return 0;

        const size_t w = m_write_pos.load(std::memory_order_acquire);
        const size_t r = m_read_pos.load(std::memory_order_relaxed);
        const size_t avail = (w - r + cap) % cap;
        return (avail / fsize) * fsize;
    }

    // Returns bytes actually read (always whole audio frames). Increments underrun counter on starvation.
    size_t read(uint8_t* out, size_t bytes, uint64_t gen) noexcept {
        if (gen == 0 || gen != m_read_gen.load(std::memory_order_acquire)) {
            std::memset(out, 0, bytes); // Silence on generation mismatch
            return 0;
        }

        m_active_readers.fetch_add(1, std::memory_order_acquire);
        struct ReaderGuard {
            std::atomic<int32_t>& count;
            ~ReaderGuard() { count.fetch_sub(1, std::memory_order_release); }
        } guard{m_active_readers};

        if (gen != m_read_gen.load(std::memory_order_acquire)) {
            std::memset(out, 0, bytes);
            return 0;
        }

        const size_t cap = m_usable_capacity.load(std::memory_order_relaxed);
        const size_t fsize = m_frame_size.load(std::memory_order_relaxed);
        if (cap == 0 || fsize == 0) {
            std::memset(out, 0, bytes);
            return 0;
        }

        const size_t avail = available_read();
        if (avail == 0) {
            m_underruns.fetch_add(1, std::memory_order_relaxed);
            std::memset(out, 0, bytes);
            return 0;
        }

        size_t to_read = (avail < bytes) ? avail : bytes;
        to_read = (to_read / fsize) * fsize;
        if (to_read == 0) {
            std::memset(out, 0, bytes);
            return 0;
        }

        const size_t r = m_read_pos.load(std::memory_order_relaxed);
        const size_t first = cap - r;
        if (first >= to_read) {
            std::memcpy(out, m_buf.data() + r, to_read);
        } else {
            std::memcpy(out, m_buf.data() + r, first);
            std::memcpy(out + first, m_buf.data(), to_read - first);
        }

        // Pad remainder with silence if buffer did not have all requested bytes
        if (to_read < bytes) {
            std::memset(out + to_read, 0, bytes - to_read);
        }

        m_read_pos.store((r + to_read) % cap, std::memory_order_release);
        return to_read;
    }

    // Thread-safe flush and generation advance.
    // Invalidates generations first, waits for active operations to finish,
    // then safely resets positions and publishes the new generation.
    uint64_t reset(uint64_t new_gen) noexcept {
        // 1. Invalidate generations immediately so in-flight/subsequent checks fail
        m_write_gen.store(0, std::memory_order_release);
        m_read_gen.store(0, std::memory_order_release);

        // 2. Wait for any active reader or writer to complete
        while (m_active_readers.load(std::memory_order_acquire) > 0 ||
               m_active_writers.load(std::memory_order_acquire) > 0) {
            std::this_thread::yield();
        }

        // 3. Reset read/write positions safely
        m_read_pos.store(0, std::memory_order_release);
        m_write_pos.store(0, std::memory_order_release);

        // 4. Publish new generation
        m_write_gen.store(new_gen, std::memory_order_release);
        m_read_gen.store(new_gen, std::memory_order_release);
        return new_gen;
    }

    uint64_t underruns() const noexcept { return m_underruns.load(std::memory_order_relaxed); }
    size_t capacity() const noexcept { return m_usable_capacity.load(std::memory_order_relaxed); }
    uint64_t get_read_gen() const noexcept { return m_read_gen.load(std::memory_order_acquire); }
    uint64_t get_write_gen() const noexcept { return m_write_gen.load(std::memory_order_acquire); }

private:
    void update_usable_capacity() noexcept {
        const size_t fsize = m_frame_size.load(std::memory_order_relaxed);
        if (fsize > 0 && m_raw_capacity >= fsize) {
            m_usable_capacity.store((m_raw_capacity / fsize) * fsize, std::memory_order_release);
        } else {
            m_usable_capacity.store(m_raw_capacity, std::memory_order_release);
        }
    }

    std::vector<uint8_t>      m_buf;
    const size_t              m_raw_capacity;
    std::atomic<size_t>       m_usable_capacity;
    std::atomic<size_t>       m_frame_size;
    std::atomic<size_t>       m_read_pos;
    std::atomic<size_t>       m_write_pos;
    std::atomic<uint64_t>     m_write_gen;
    std::atomic<uint64_t>     m_read_gen;
    std::atomic<uint64_t>     m_underruns;
    std::atomic<int32_t>      m_active_readers;
    std::atomic<int32_t>      m_active_writers;
};
