// Self-contained unit tests (no external framework required).
#include <cstdint>
#include <functional>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "BitIO.hpp"
#include "HuffmanCoder.hpp"
#include "HuffmanTree.hpp"
#include "MinHeap.hpp"

using namespace huff;

namespace {

int g_failed = 0;
int g_passed = 0;

#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            throw std::runtime_error(std::string("CHECK failed: ") + #cond + " (line " + \
                                     std::to_string(__LINE__) + ")");                  \
        }                                                                              \
    } while (0)

void runTest(const std::string& name, const std::function<void()>& fn) {
    try {
        fn();
        ++g_passed;
        std::cout << "  [PASS] " << name << '\n';
    } catch (const std::exception& e) {
        ++g_failed;
        std::cout << "  [FAIL] " << name << " -> " << e.what() << '\n';
    }
}

std::string roundTrip(const std::string& input, CompressionStats* statsOut = nullptr) {
    HuffmanCoder coder;
    std::istringstream in(input);
    std::ostringstream packed;
    CompressionStats stats = coder.compress(in, packed);
    CHECK(stats.originalBytes == input.size());
    CHECK(stats.compressedBytes == packed.str().size());
    if (statsOut) *statsOut = stats;

    std::istringstream packedIn(packed.str());
    std::ostringstream restored;
    std::uint64_t n = coder.decompress(packedIn, restored);
    CHECK(n == input.size());
    return restored.str();
}

std::string randomBytes(std::size_t n, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, 255);
    std::string s(n, '\0');
    for (auto& c : s) c = static_cast<char>(dist(rng));
    return s;
}

}  // namespace

int main() {
    std::cout << "Running Huffman tests\n";

    runTest("MinHeap pops in ascending order", [] {
        MinHeap<int> heap;
        std::vector<int> values = {42, 7, 19, 3, 3, 88, -5, 0, 61, 12};
        for (int v : values) heap.push(v);
        CHECK(heap.size() == values.size());
        int prev = heap.pop();
        while (!heap.empty()) {
            int cur = heap.pop();
            CHECK(prev <= cur);
            prev = cur;
        }
    });

    runTest("MinHeap throws on empty pop", [] {
        MinHeap<int> heap;
        bool threw = false;
        try { heap.pop(); } catch (const std::out_of_range&) { threw = true; }
        CHECK(threw);
    });

    runTest("BitWriter/BitReader round-trip", [] {
        std::ostringstream out;
        BitWriter w(out);
        w.writeBits(0b101, 3);
        w.writeByte(0xAB);
        w.writeBit(true);
        w.writeBits(0x3FF, 10);
        w.flush();
        CHECK(w.bitsWritten() == 22);
        CHECK(out.str().size() == 3);

        std::istringstream in(out.str());
        BitReader r(in);
        CHECK(r.readBit() == true);
        CHECK(r.readBit() == false);
        CHECK(r.readBit() == true);
        CHECK(r.readByte() == 0xAB);
        CHECK(r.readBit() == true);
        for (int i = 0; i < 10; ++i) CHECK(r.readBit() == true);
    });

    runTest("Huffman codes are prefix-free and respect frequency", [] {
        FrequencyTable f{};
        f['a'] = 45; f['b'] = 13; f['c'] = 12; f['d'] = 16; f['e'] = 9; f['f'] = 5;
        HuffmanTree tree = HuffmanTree::build(f);
        CodeTable codes = tree.buildCodeTable();
        CHECK(tree.leafCount() == 6);
        CHECK(codes['a'].length == 1);                  // most frequent -> shortest
        CHECK(codes['f'].length >= codes['a'].length);  // rare -> not shorter
        // Classic CLRS example: optimal cost is 224 bits.
        std::uint64_t cost = 0;
        for (char c : std::string("abcdef")) cost += f[static_cast<unsigned char>(c)] * codes[static_cast<unsigned char>(c)].length;
        CHECK(cost == 224);
        // Prefix-free: no code is a prefix of another.
        std::string syms = "abcdef";
        for (char x : syms) for (char y : syms) {
            if (x == y) continue;
            const Code& a = codes[static_cast<unsigned char>(x)];
            const Code& b = codes[static_cast<unsigned char>(y)];
            if (a.length <= b.length) CHECK((b.bits >> (b.length - a.length)) != a.bits);
        }
    });

    runTest("Tree serialize/deserialize preserves codes", [] {
        FrequencyTable f{};
        for (int i = 0; i < 256; ++i) f[static_cast<std::size_t>(i)] = static_cast<std::uint64_t>(i * i + 1);
        HuffmanTree tree = HuffmanTree::build(f);
        std::ostringstream out;
        BitWriter w(out);
        tree.serialize(w);
        w.flush();
        std::istringstream in(out.str());
        BitReader r(in);
        HuffmanTree copy = HuffmanTree::deserialize(r);
        CodeTable a = tree.buildCodeTable(), b = copy.buildCodeTable();
        for (int i = 0; i < 256; ++i) {
            CHECK(a[static_cast<std::size_t>(i)].bits == b[static_cast<std::size_t>(i)].bits);
            CHECK(a[static_cast<std::size_t>(i)].length == b[static_cast<std::size_t>(i)].length);
        }
    });

    runTest("Empty input", [] {
        CompressionStats s;
        CHECK(roundTrip("", &s).empty());
        CHECK(s.compressedBytes == HuffmanCoder::kHeaderSize);
    });

    runTest("Single byte", [] { CHECK(roundTrip("x") == "x"); });

    runTest("One repeated symbol", [] {
        std::string in(100000, 'z');
        CompressionStats s;
        CHECK(roundTrip(in, &s) == in);
        CHECK(s.avgCodeLength == 1.0);
        CHECK(s.spaceSavingPercent() > 85.0);
    });

    runTest("Symbol 0xFF alone (dummy wraps to 0x00)", [] {
        std::string in(500, static_cast<char>(0xFF));
        CHECK(roundTrip(in) == in);
    });

    runTest("All 256 byte values", [] {
        std::string in;
        for (int rep = 0; rep < 10; ++rep)
            for (int i = 0; i < 256; ++i) in.push_back(static_cast<char>(i));
        CHECK(roundTrip(in) == in);
    });

    runTest("Random binary data (1 MB)", [] {
        std::string in = randomBytes(1 << 20, 1234);
        CompressionStats s;
        CHECK(roundTrip(in, &s) == in);
        CHECK(s.ratio() < 1.01);  // incompressible, but overhead stays tiny
    });

    runTest("English text compresses", [] {
        std::string base = "The quick brown fox jumps over the lazy dog. Huffman coding assigns "
                           "shorter codes to frequent symbols and longer codes to rare ones. ";
        std::string in;
        while (in.size() < 200000) in += base;
        CompressionStats s;
        CHECK(roundTrip(in, &s) == in);
        CHECK(s.ratio() < 0.65);
        CHECK(s.avgCodeLength >= s.entropy);          // Huffman never beats entropy
        CHECK(s.avgCodeLength < s.entropy + 1.0);     // and is within 1 bit of it
    });

    runTest("Skewed 4-symbol data saves > 50%", [] {
        std::mt19937 rng(7);
        std::discrete_distribution<int> dist({50, 25, 15, 10});
        const char alphabet[] = "ACGT";
        std::string in(500000, ' ');
        for (auto& c : in) c = alphabet[dist(rng)];
        CompressionStats s;
        CHECK(roundTrip(in, &s) == in);
        CHECK(s.spaceSavingPercent() > 50.0);
    });

    runTest("Rejects bad magic number", [] {
        HuffmanCoder coder;
        std::istringstream in("NOPE\x01\0\0\0\0\0\0\0\0");
        std::ostringstream out;
        bool threw = false;
        try { coder.decompress(in, out); } catch (const std::runtime_error&) { threw = true; }
        CHECK(threw);
    });

    runTest("Rejects truncated data", [] {
        HuffmanCoder coder;
        std::istringstream in(std::string(5000, 'a') + std::string(5000, 'b'));
        std::ostringstream packed;
        coder.compress(in, packed);
        std::string cut = packed.str().substr(0, packed.str().size() / 2);
        std::istringstream bad(cut);
        std::ostringstream out;
        bool threw = false;
        try { coder.decompress(bad, out); } catch (const std::runtime_error&) { threw = true; }
        CHECK(threw);
    });

    std::cout << '\n' << g_passed << " passed, " << g_failed << " failed\n";
    return g_failed == 0 ? 0 : 1;
}
