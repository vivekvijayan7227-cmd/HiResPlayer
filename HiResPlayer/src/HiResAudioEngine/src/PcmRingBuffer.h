#pragma once
#include <atomic>
#include <cstdint>
#include <cstring>
#include <vector>

// Single-producer / single-consumer lock-free PCM ring buffer.
// Producer and consumer MUST be on separate threads.
// Supports a "generation" counter so stale data can be discarded after seeks.
class PcmRingBuffer {
public:
    explicit PcmRingBuffer(size_t capacity_bytes)
        : m_buf(capacity_bytes), m_capacity(capacity_bytes),
          m_read_pos(0), m_write_pos(0),
          m_write_gen(0), m_read_gen(0), m_underruns(0) {}

    // Producer side
    size_t available_write() const noexcept {
        size_t r = m_read_pos.load(std::memory_order_acquire);
        size_t w = m_write_pos.load(std::memory_order_relaxed);
        size_t used = (w - r + m_capacity) % m_capacity;
        return m_capacity - used - 1; // keep 1 byte unused to distinguish full/empty
    }

    // Returns bytes actually written
    size_t write(const uint8_t* data, size_t bytes, uint64_t gen) noexcept {
        if (gen != m_write_gen.load(std::memory_order_acquire)) return 0;
        size_t avail = available_write();
        size_t to_write = bytes < avail ? bytes : avail;
        if (to_write == 0) return 0;
        size_t w = m_write_pos.load(std::memory_order_relaxed);
        size_t first = m_capacity - w;
        if (first >= to_write) {
            std::memcpy(m_buf.data() + w, data, to_write);
        } else {
            std::memcpy(m_buf.data() + w, data, first);
            std::memcpy(m_buf.data(), data + first, to_write - first);
        }
        m_write_pos.store((w + to_write) % m_capacity, std::memory_order_release);
        return to_write;
    }

    // Consumer side
    size_t available_read() const noexcept {
        size_t w = m_write_pos.load(std::memory_order_acquire);
        size_t r = m_read_pos.load(std::memory_order_relaxed);
        return (w - r + m_capacity) % m_capacity;
    }

    // Returns bytes actually read. Increments underrun counter if bytes unavailable.
    size_t read(uint8_t* out, size_t bytes, uint64_t gen) noexcept {
        if (gen != m_read_gen.load(std::memory_order_acquire)) {
            std::memset(out, 0, bytes); // silence on generation mismatch
            return 0;
        }
        size_t avail = available_read();
        if (avail == 0) {
            m_underruns.fetch_add(1, std::memory_order_relaxed);
            std::memset(out, 0, bytes);
            return 0;
        }
        size_t to_read = (avail < bytes) ? avail : bytes;
        size_t r = m_read_pos.load(std::memory_order_relaxed);
        size_t first = m_capacity - r;
        if (first >= to_read) {
            std::memcpy(out, m_buf.data() + r, to_read);
        } else {
            std::memcpy(out, m_buf.data() + r, first);
            std::memcpy(out + first, m_buf.data(), to_read - first);
        }
        if (to_read < bytes) {
            std::memset(out + to_read, 0, bytes - to_read);
        }
        m_read_pos.store((r + to_read) % m_capacity, std::memory_order_release);
        return to_read;
    }

    // Flush and advance generation — call on control thread before seek/stop
    uint64_t reset(uint64_t new_gen) noexcept {
        m_read_pos.store(0, std::memory_order_relaxed);
        m_write_pos.store(0, std::memory_order_relaxed);
        m_write_gen.store(new_gen, std::memory_order_release);
        m_read_gen.store(new_gen, std::memory_order_release);
        return new_gen;
    }

    uint64_t underruns() const noexcept { return m_underruns.load(std::memory_order_relaxed); }
    size_t capacity() const noexcept { return m_capacity; }
    uint64_t get_read_gen() const noexcept { return m_read_gen.load(std::memory_order_acquire); }
    uint64_t get_write_gen() const noexcept { return m_write_gen.load(std::memory_order_acquire); }

private:
    std::vector<uint8_t>      m_buf;
    const size_t              m_capacity;
    std::atomic<size_t>       m_read_pos;
    std::atomic<size_t>       m_write_pos;
    std::atomic<uint64_t>     m_write_gen;
    std::atomic<uint64_t>     m_read_gen;
    std::atomic<uint64_t>     m_underruns;
};
