// Command-line front end for the Huffman compressor.
//
//   huffman compress   <input> <output>
//   huffman decompress <input> <output>
//   huffman bench      <input> [runs]

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "HuffmanCoder.hpp"
#include "Timer.hpp"

namespace fs = std::filesystem;
using huff::CompressionStats;
using huff::HuffmanCoder;
using huff::Timer;

namespace {

void printUsage(const char* prog) {
    std::cout << "Huffman File Compression Tool\n\n"
              << "Usage:\n"
              << "  " << prog << " compress   <input> <output>   Compress a file\n"
              << "  " << prog << " decompress <input> <output>   Restore a compressed file\n"
              << "  " << prog << " bench      <input> [runs]     Round-trip benchmark (default 3 runs)\n"
              << "  " << prog << " help                          Show this message\n";
}

std::string humanBytes(std::uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    std::ostringstream os;
    os << std::fixed << std::setprecision(unit == 0 ? 0 : 2) << value << ' ' << units[unit];
    return os.str();
}

// Avoids printing "-0.00" for values that round to zero.
double clean(double v) { return (v > -0.005 && v < 0.005) ? 0.0 : v; }

double mbPerSecond(std::uint64_t bytes, double seconds) {
    return seconds <= 0.0 ? 0.0 : (static_cast<double>(bytes) / (1024.0 * 1024.0)) / seconds;
}

void printStats(const CompressionStats& s) {
    std::cout << std::fixed << std::setprecision(2)
              << "  Original size    : " << humanBytes(s.originalBytes) << " (" << s.originalBytes << " bytes)\n"
              << "  Compressed size  : " << humanBytes(s.compressedBytes) << " (" << s.compressedBytes << " bytes)\n"
              << "  Compression ratio: " << std::setprecision(4) << s.ratio() << std::setprecision(2) << "\n"
              << "  Space saved      : " << clean(s.spaceSavingPercent()) << " %\n"
              << "  Unique symbols   : " << s.uniqueSymbols << "\n"
              << "  Avg code length  : " << std::setprecision(3) << s.avgCodeLength << " bits/byte"
              << "  (entropy " << s.entropy << ")\n"
              << std::setprecision(2)
              << "  Time             : " << s.seconds * 1000.0 << " ms  ("
              << mbPerSecond(s.originalBytes, s.seconds) << " MB/s)\n";
}

bool filesEqual(const std::string& a, const std::string& b) {
    std::ifstream fa(a, std::ios::binary), fb(b, std::ios::binary);
    if (!fa || !fb) return false;
    std::vector<char> ba(1 << 16), bb(1 << 16);
    while (true) {
        fa.read(ba.data(), static_cast<std::streamsize>(ba.size()));
        fb.read(bb.data(), static_cast<std::streamsize>(bb.size()));
        if (fa.gcount() != fb.gcount()) return false;
        if (fa.gcount() == 0) return true;
        if (!std::equal(ba.begin(), ba.begin() + fa.gcount(), bb.begin())) return false;
    }
}

int runCompress(const std::string& in, const std::string& out) {
    HuffmanCoder coder;
    CompressionStats stats = coder.compressFile(in, out);
    std::cout << "Compressed '" << in << "' -> '" << out << "'\n";
    printStats(stats);
    return 0;
}

int runDecompress(const std::string& in, const std::string& out) {
    HuffmanCoder coder;
    Timer t;
    std::uint64_t bytes = coder.decompressFile(in, out);
    double secs = t.elapsedSeconds();
    std::cout << "Decompressed '" << in << "' -> '" << out << "'\n"
              << std::fixed << std::setprecision(2)
              << "  Restored size    : " << humanBytes(bytes) << " (" << bytes << " bytes)\n"
              << "  Time             : " << secs * 1000.0 << " ms  (" << mbPerSecond(bytes, secs)
              << " MB/s)\n";
    return 0;
}

int runBench(const std::string& in, int runs) {
    if (runs < 1) runs = 1;
    const fs::path tmpDir = fs::temp_directory_path();
    const std::string base = fs::path(in).filename().string();
    const std::string packed = (tmpDir / (base + ".bench.huf")).string();
    const std::string unpacked = (tmpDir / (base + ".bench.out")).string();

    HuffmanCoder coder;
    CompressionStats stats;
    double compressTotal = 0.0, decompressTotal = 0.0;
    double compressBest = 1e300, decompressBest = 1e300;

    for (int r = 0; r < runs; ++r) {
        stats = coder.compressFile(in, packed);
        compressTotal += stats.seconds;
        compressBest = std::min(compressBest, stats.seconds);

        Timer t;
        coder.decompressFile(packed, unpacked);
        double d = t.elapsedSeconds();
        decompressTotal += d;
        decompressBest = std::min(decompressBest, d);
    }

    const bool ok = filesEqual(in, unpacked);
    std::error_code ec;
    fs::remove(packed, ec);
    fs::remove(unpacked, ec);

    const double cAvg = compressTotal / runs, dAvg = decompressTotal / runs;
    stats.seconds = cAvg;

    std::cout << "Benchmark: " << in << "  (" << runs << " run" << (runs > 1 ? "s" : "") << ")\n";
    printStats(stats);
    std::cout << std::fixed << std::setprecision(2)
              << "  Compress   avg/best : " << cAvg * 1000.0 << " / " << compressBest * 1000.0 << " ms\n"
              << "  Decompress avg/best : " << dAvg * 1000.0 << " / " << decompressBest * 1000.0 << " ms  ("
              << mbPerSecond(stats.originalBytes, dAvg) << " MB/s)\n"
              << "  Round-trip check    : " << (ok ? "PASS (byte-identical)" : "FAIL") << "\n";

    // Machine-readable line for scripts.
    std::cout << "CSV," << base << ',' << stats.originalBytes << ',' << stats.compressedBytes << ','
              << std::setprecision(4) << stats.ratio() << ',' << std::setprecision(2)
              << clean(stats.spaceSavingPercent()) << ',' << cAvg * 1000.0 << ',' << dAvg * 1000.0 << ','
              << (ok ? "PASS" : "FAIL") << "\n";
    return ok ? 0 : 1;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    const std::string cmd = argv[1];
    try {
        if ((cmd == "compress" || cmd == "-c") && argc == 4) return runCompress(argv[2], argv[3]);
        if ((cmd == "decompress" || cmd == "-d") && argc == 4) return runDecompress(argv[2], argv[3]);
        if ((cmd == "bench" || cmd == "-b") && (argc == 3 || argc == 4)) {
            return runBench(argv[2], argc == 4 ? std::atoi(argv[3]) : 3);
        }
        if (cmd == "help" || cmd == "-h" || cmd == "--help") {
            printUsage(argv[0]);
            return 0;
        }
        printUsage(argv[0]);
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 2;
    }
}
