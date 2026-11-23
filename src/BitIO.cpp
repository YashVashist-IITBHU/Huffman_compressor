#include "BitIO.hpp"

#include <stdexcept>

namespace huff {

// ---------------------------- BitWriter ----------------------------

BitWriter::BitWriter(std::ostream& out, std::size_t bufferSize)
    : out_(out), buffer_(bufferSize == 0 ? 1 : bufferSize) {}

void BitWriter::writeBit(bool bit) {
    current_ = static_cast<std::uint8_t>((current_ << 1) | (bit ? 1u : 0u));
    ++filled_;
    ++totalBits_;
    if (filled_ == 8) {
        pushByte(current_);
        current_ = 0;
        filled_ = 0;
    }
}

void BitWriter::writeBits(std::uint64_t bits, unsigned count) {
    for (unsigned i = count; i > 0; --i) {
        writeBit(((bits >> (i - 1)) & 1u) != 0);
    }
}

void BitWriter::writeByte(std::uint8_t byte) { writeBits(byte, 8); }

void BitWriter::pushByte(std::uint8_t byte) {
    buffer_[pos_++] = static_cast<char>(byte);
    if (pos_ == buffer_.size()) drainBuffer();
}

void BitWriter::drainBuffer() {
    if (pos_ == 0) return;
    out_.write(buffer_.data(), static_cast<std::streamsize>(pos_));
    if (!out_) throw std::runtime_error("BitWriter: failed to write to output stream");
    pos_ = 0;
}

void BitWriter::flush() {
    if (filled_ > 0) {
        // Shift the remaining bits to the top of the byte; low bits become padding.
        pushByte(static_cast<std::uint8_t>(current_ << (8 - filled_)));
        current_ = 0;
        filled_ = 0;
    }
    drainBuffer();
    out_.flush();
}

// ---------------------------- BitReader ----------------------------

BitReader::BitReader(std::istream& in, std::size_t bufferSize)
    : in_(in), buffer_(bufferSize == 0 ? 1 : bufferSize) {}

bool BitReader::refill() {
    in_.read(buffer_.data(), static_cast<std::streamsize>(buffer_.size()));
    len_ = static_cast<std::size_t>(in_.gcount());
    pos_ = 0;
    return len_ > 0;
}

bool BitReader::readBit() {
    if (remaining_ == 0) {
        if (pos_ == len_ && !refill()) {
            throw std::runtime_error("BitReader: unexpected end of compressed data");
        }
        current_ = static_cast<std::uint8_t>(buffer_[pos_++]);
        remaining_ = 8;
    }
    --remaining_;
    return ((current_ >> remaining_) & 1u) != 0;
}

std::uint8_t BitReader::readByte() {
    std::uint8_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value = static_cast<std::uint8_t>((value << 1) | (readBit() ? 1u : 0u));
    }
    return value;
}

}  // namespace huff
