#pragma once

#include <cstddef>
#include <cstdint>
#include <istream>
#include <ostream>
#include <vector>

namespace huff {

// Writes individual bits (MSB-first) to a byte stream, using an internal
// buffer so the underlying stream is touched in large blocks.
class BitWriter {
public:
    explicit BitWriter(std::ostream& out, std::size_t bufferSize = 1 << 16);
    BitWriter(const BitWriter&) = delete;
    BitWriter& operator=(const BitWriter&) = delete;

    void writeBit(bool bit);
    // Writes the lowest `count` bits of `bits`, most significant first.
    void writeBits(std::uint64_t bits, unsigned count);
    void writeByte(std::uint8_t byte);

    // Pads the final partial byte with zeros and pushes everything to the stream.
    void flush();

    std::uint64_t bitsWritten() const { return totalBits_; }

private:
    void pushByte(std::uint8_t byte);
    void drainBuffer();

    std::ostream& out_;
    std::vector<char> buffer_;
    std::size_t pos_ = 0;
    std::uint8_t current_ = 0;
    unsigned filled_ = 0;
    std::uint64_t totalBits_ = 0;
};

// Reads individual bits (MSB-first) from a byte stream.
class BitReader {
public:
    explicit BitReader(std::istream& in, std::size_t bufferSize = 1 << 16);
    BitReader(const BitReader&) = delete;
    BitReader& operator=(const BitReader&) = delete;

    // Throws std::runtime_error if the stream ends unexpectedly.
    bool readBit();
    std::uint8_t readByte();

private:
    bool refill();

    std::istream& in_;
    std::vector<char> buffer_;
    std::size_t pos_ = 0;
    std::size_t len_ = 0;
    std::uint8_t current_ = 0;
    unsigned remaining_ = 0;
};

}  // namespace huff
