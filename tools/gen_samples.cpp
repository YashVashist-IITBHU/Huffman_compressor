// Generates reproducible sample files for benchmarking.
//
//   gen_samples <output_dir> [size_mb]
//
// Produces:
//   text.txt     English-like text (letter frequencies of English prose)
//   server.log   repetitive application log lines
//   dna.txt      genome-style sequence over {A,C,G,T} plus newlines
//   random.bin   uniformly random bytes (incompressible baseline)

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

void writeText(const fs::path& path, std::size_t bytes, std::mt19937& rng) {
    // Word list weighted roughly like English usage (Zipf-like).
    const std::vector<std::string> words = {
        "the", "of", "and", "to", "a", "in", "is", "it", "that", "was", "for", "on", "are", "with",
        "as", "they", "be", "at", "one", "have", "this", "from", "by", "word", "but", "not", "what",
        "all", "were", "when", "we", "there", "can", "an", "your", "which", "their", "said", "if",
        "will", "each", "about", "how", "up", "out", "them", "then", "she", "many", "some", "so",
        "these", "would", "other", "into", "has", "more", "her", "two", "like", "him", "see", "time",
        "could", "no", "make", "than", "first", "been", "its", "who", "now", "people", "my", "made",
        "over", "did", "down", "only", "way", "find", "use", "may", "water", "long", "little", "very",
        "after", "words", "called", "just", "where", "most", "know", "compression", "algorithm",
        "frequency", "symbol", "binary", "tree", "encoding", "efficient", "system", "information"};
    std::vector<double> weights;
    for (std::size_t i = 0; i < words.size(); ++i) weights.push_back(1.0 / static_cast<double>(i + 1));
    std::discrete_distribution<std::size_t> pick(weights.begin(), weights.end());
    std::uniform_int_distribution<int> sentenceLen(6, 18);

    std::ofstream out(path, std::ios::binary);
    std::size_t written = 0;
    while (written < bytes) {
        std::string sentence;
        int n = sentenceLen(rng);
        for (int i = 0; i < n; ++i) {
            std::string w = words[pick(rng)];
            if (i == 0) w[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(w[0])));
            sentence += w;
            sentence += (i + 1 == n) ? ". " : " ";
        }
        if (rng() % 6 == 0) sentence += "\n";
        out << sentence;
        written += sentence.size();
    }
}

void writeLog(const fs::path& path, std::size_t bytes, std::mt19937& rng) {
    const std::vector<std::string> levels = {"INFO", "INFO", "INFO", "DEBUG", "WARN", "ERROR"};
    const std::vector<std::string> msgs = {
        "Request completed status=200", "Cache hit for key user_profile", "Connection pool size=32",
        "Request completed status=404", "Retrying upstream call attempt=2", "Scheduled job finished"};
    const std::vector<std::string> paths = {"/api/v1/orders", "/api/v1/users", "/health", "/api/v1/cart"};
    std::ofstream out(path, std::ios::binary);
    std::size_t written = 0;
    unsigned long long ts = 1700000000000ULL;
    char line[256];
    while (written < bytes) {
        ts += rng() % 50;
        int len = std::snprintf(line, sizeof line, "%llu [%s] svc=api path=%s latency_ms=%u %s\n", ts,
                                levels[rng() % levels.size()].c_str(), paths[rng() % paths.size()].c_str(),
                                static_cast<unsigned>(rng() % 900), msgs[rng() % msgs.size()].c_str());
        out.write(line, len);
        written += static_cast<std::size_t>(len);
    }
}

void writeDna(const fs::path& path, std::size_t bytes, std::mt19937& rng) {
    std::discrete_distribution<int> dist({30, 20, 20, 30});
    const char bases[] = "ACGT";
    std::ofstream out(path, std::ios::binary);
    std::string line(80, 'A');
    std::size_t written = 0;
    while (written < bytes) {
        for (auto& c : line) c = bases[dist(rng)];
        out << line << '\n';
        written += line.size() + 1;
    }
}

void writeRandom(const fs::path& path, std::size_t bytes, std::mt19937& rng) {
    std::ofstream out(path, std::ios::binary);
    std::vector<char> buf(1 << 16);
    std::size_t written = 0;
    while (written < bytes) {
        for (auto& c : buf) c = static_cast<char>(rng() & 0xFF);
        std::size_t n = std::min(buf.size(), bytes - written);
        out.write(buf.data(), static_cast<std::streamsize>(n));
        written += n;
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <output_dir> [size_mb]\n";
        return 1;
    }
    const fs::path dir = argv[1];
    const std::size_t mb = argc > 2 ? static_cast<std::size_t>(std::atoi(argv[2])) : 10;
    const std::size_t bytes = (mb == 0 ? 1 : mb) * 1024 * 1024;
    fs::create_directories(dir);

    std::mt19937 rng(42);  // fixed seed -> reproducible benchmarks
    writeText(dir / "text.txt", bytes, rng);
    writeLog(dir / "server.log", bytes, rng);
    writeDna(dir / "dna.txt", bytes, rng);
    writeRandom(dir / "random.bin", bytes, rng);
    std::cout << "Generated 4 sample files (~" << mb << " MB each) in " << dir << '\n';
    return 0;
}
