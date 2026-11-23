#pragma once

#include <cstdint>
#include <istream>
#include <ostream>
#include <string>

namespace huff {

// Summary of a single compression run.
struct CompressionStats {
    std::uint64_t originalBytes = 0;
    std::uint64_t compressedBytes = 0;   // including header and tree
    std::size_t uniqueSymbols = 0;
    double avgCodeLength = 0.0;          // bits per input byte (payload only)
    double entropy = 0.0;                // Shannon entropy, bits per byte
    double seconds = 0.0;

    // compressed / original (lower is better)
    double ratio() const {
        return originalBytes == 0 ? 0.0
                                  : static_cast<double>(compressedBytes) / static_cast<double>(originalBytes);
    }
    // percentage of space saved (higher is better; negative = file grew)
    double spaceSavingPercent() const { return originalBytes == 0 ? 0.0 : (1.0 - ratio()) * 100.0; }
};

// Compressed file layout (all multi-byte integers little-endian):
//
//   offset  size  field
//   0       4     magic "HUFZ"
//   4       1     format version (1)
//   5       8     original size in bytes
//   13      ...   bit stream: pre-order serialized tree, then encoded data,
//                 zero-padded to a whole byte
//
// An empty input produces only the 13-byte header.
class HuffmanCoder {
public:
    static constexpr char kMagic[4] = {'H', 'U', 'F', 'Z'};
    static constexpr std::uint8_t kVersion = 1;
    static constexpr std::uint64_t kHeaderSize = 13;

    // `in` must be seekable: frequencies are counted in a first pass and the
    // data is encoded in a second pass, so files of any size stream through
    // a fixed-size buffer instead of being loaded into memory.
    CompressionStats compress(std::istream& in, std::ostream& out) const;

    // Returns the number of bytes written to `out`.
    std::uint64_t decompress(std::istream& in, std::ostream& out) const;

    CompressionStats compressFile(const std::string& inputPath, const std::string& outputPath) const;
    std::uint64_t decompressFile(const std::string& inputPath, const std::string& outputPath) const;
};

}  // namespace huff
