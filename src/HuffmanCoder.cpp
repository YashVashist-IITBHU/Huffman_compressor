#include "HuffmanCoder.hpp"

#include <cmath>
#include <fstream>
#include <stdexcept>
#include <vector>

#include "BitIO.hpp"
#include "HuffmanTree.hpp"
#include "Timer.hpp"

namespace huff {

constexpr char HuffmanCoder::kMagic[4];

namespace {

constexpr std::size_t kChunkSize = 1 << 16;

void writeU64(std::ostream& out, std::uint64_t value) {
    char bytes[8];
    for (int i = 0; i < 8; ++i) bytes[i] = static_cast<char>((value >> (8 * i)) & 0xFF);
    out.write(bytes, 8);
}

std::uint64_t readU64(std::istream& in) {
    unsigned char bytes[8];
    if (!in.read(reinterpret_cast<char*>(bytes), 8)) {
        throw std::runtime_error("Corrupt compressed file: truncated header");
    }
    std::uint64_t value = 0;
    for (int i = 7; i >= 0; --i) value = (value << 8) | bytes[i];
    return value;
}

// Reads the stream in chunks and calls fn(byte) for every byte.
template <typename Fn>
std::uint64_t forEachByte(std::istream& in, Fn fn) {
    std::vector<char> buffer(kChunkSize);
    std::uint64_t total = 0;
    while (true) {
        in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto got = static_cast<std::size_t>(in.gcount());
        if (got == 0) break;
        for (std::size_t i = 0; i < got; ++i) fn(static_cast<std::uint8_t>(buffer[i]));
        total += got;
    }
    return total;
}

double shannonEntropy(const FrequencyTable& freq, std::uint64_t total) {
    if (total == 0) return 0.0;
    double h = 0.0;
    for (std::uint64_t f : freq) {
        if (f == 0) continue;
        double p = static_cast<double>(f) / static_cast<double>(total);
        h -= p * std::log2(p);
    }
    return h;
}

}  // namespace

CompressionStats HuffmanCoder::compress(std::istream& in, std::ostream& out) const {
    Timer timer;
    CompressionStats stats;

    // Pass 1: count byte frequencies.
    FrequencyTable freq{};
    const std::uint64_t total = forEachByte(in, [&freq](std::uint8_t b) { ++freq[b]; });

    stats.originalBytes = total;
    stats.entropy = shannonEntropy(freq, total);

    out.write(kMagic, 4);
    out.put(static_cast<char>(kVersion));
    writeU64(out, total);
    if (!out) throw std::runtime_error("Failed to write compressed header");

    if (total == 0) {
        stats.compressedBytes = kHeaderSize;
        stats.seconds = timer.elapsedSeconds();
        return stats;
    }

    const HuffmanTree tree = HuffmanTree::build(freq);
    const CodeTable codes = tree.buildCodeTable();

    std::uint64_t payloadBits = 0;
    for (int s = 0; s < 256; ++s) {
        if (freq[static_cast<std::size_t>(s)] == 0) continue;
        ++stats.uniqueSymbols;
        payloadBits += freq[static_cast<std::size_t>(s)] * codes[static_cast<std::size_t>(s)].length;
    }
    stats.avgCodeLength = static_cast<double>(payloadBits) / static_cast<double>(total);

    // Pass 2: rewind and encode.
    in.clear();
    in.seekg(0, std::ios::beg);
    if (!in) throw std::runtime_error("Input stream is not seekable; cannot run second pass");

    BitWriter writer(out);
    tree.serialize(writer);
    const std::uint64_t encoded = forEachByte(in, [&writer, &codes](std::uint8_t b) {
        writer.writeBits(codes[b].bits, codes[b].length);
    });
    if (encoded != total) throw std::runtime_error("Input changed between compression passes");
    writer.flush();

    stats.compressedBytes = kHeaderSize + (writer.bitsWritten() + 7) / 8;
    stats.seconds = timer.elapsedSeconds();
    return stats;
}

std::uint64_t HuffmanCoder::decompress(std::istream& in, std::ostream& out) const {
    char magic[4];
    if (!in.read(magic, 4) || magic[0] != kMagic[0] || magic[1] != kMagic[1] ||
        magic[2] != kMagic[2] || magic[3] != kMagic[3]) {
        throw std::runtime_error("Not a Huffman-compressed file (bad magic number)");
    }
    const int version = in.get();
    if (version != kVersion) {
        throw std::runtime_error("Unsupported compressed file version: " + std::to_string(version));
    }
    const std::uint64_t originalSize = readU64(in);
    if (originalSize == 0) return 0;

    BitReader reader(in);
    const HuffmanTree tree = HuffmanTree::deserialize(reader);

    std::vector<char> buffer;
    buffer.reserve(kChunkSize);
    const int root = tree.root();

    for (std::uint64_t i = 0; i < originalSize; ++i) {
        int idx = root;
        while (!tree.node(idx).isLeaf()) {
            const auto& n = tree.node(idx);
            idx = reader.readBit() ? n.right : n.left;
        }
        buffer.push_back(static_cast<char>(tree.node(idx).symbol));
        if (buffer.size() == kChunkSize) {
            out.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            buffer.clear();
        }
    }
    out.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    out.flush();
    if (!out) throw std::runtime_error("Failed to write decompressed output");
    return originalSize;
}

CompressionStats HuffmanCoder::compressFile(const std::string& inputPath, const std::string& outputPath) const {
    std::ifstream in(inputPath, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open input file: " + inputPath);
    std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Cannot open output file: " + outputPath);
    return compress(in, out);
}

std::uint64_t HuffmanCoder::decompressFile(const std::string& inputPath, const std::string& outputPath) const {
    std::ifstream in(inputPath, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open input file: " + inputPath);
    std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Cannot open output file: " + outputPath);
    return decompress(in, out);
}

}  // namespace huff
