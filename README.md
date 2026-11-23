# Huffman File Compression Tool

A lossless file compressor and decompressor written in modern C++ (C++17), built on
Huffman coding. It uses hand-written data structures (a binary min-heap priority queue
and a pooled binary tree), bit-level I/O, and includes a benchmarking mode that reports
compression ratio, space saved, and runtime.

**Concepts:** C++17, OOP, Huffman encoding, priority queues, binary trees, bit
manipulation, binary file I/O, unit testing, benchmarking.

---

## Features

- **Lossless** compression/decompression of any file type (text or binary).
- **Custom priority queue** (`MinHeap<T, Compare>`) – a templated binary heap used to build the tree.
- **Pooled Huffman tree** – nodes live in a `std::vector` and refer to each other by index, so there are no raw `new`/`delete` calls and no leaks.
- **Bit-level encoding** – `BitWriter` / `BitReader` pack variable-length codes MSB-first with 64 KB buffering.
- **Compact header** – the tree is stored in pre-order (≈10 bits per symbol) instead of a full 256-entry frequency table.
- **Streaming two-pass design** – files are processed in 64 KB chunks, so memory use stays constant regardless of file size.
- **Benchmark mode** – averages multiple runs, reports ratio, % saved, average code length vs. Shannon entropy, throughput, and verifies byte-identical round trips.
- **Robust edge cases** – empty files, single-symbol files, all 256 byte values, corrupt/truncated input detection.
- **15 unit tests** with a zero-dependency test harness.

---

## Project structure

```
huffman-compression/
├── include/
│   ├── BitIO.hpp          # BitWriter / BitReader (bit-level buffered I/O)
│   ├── MinHeap.hpp        # Templated binary min-heap (priority queue)
│   ├── HuffmanTree.hpp    # Tree construction, code table, (de)serialization
│   ├── HuffmanCoder.hpp   # High-level compress / decompress + stats
│   └── Timer.hpp          # steady_clock stopwatch
├── src/
│   ├── BitIO.cpp
│   ├── HuffmanTree.cpp
│   ├── HuffmanCoder.cpp
│   └── main.cpp           # CLI: compress / decompress / bench
├── tests/
│   └── test_huffman.cpp   # Unit tests
├── tools/
│   └── gen_samples.cpp    # Reproducible benchmark data generator
├── scripts/
│   └── benchmark.sh       # Build + generate samples + benchmark summary table
├── Makefile
└── CMakeLists.txt
```

---

## Build

**Make (Linux / macOS / MinGW):**
```bash
make            # builds bin/huffman, bin/tests, bin/gen_samples
make test       # runs unit tests
make bench      # generates sample data and benchmarks it
```

**CMake (any platform, including MSVC):**
```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build
```

Requires a C++17 compiler (GCC 8+, Clang 7+, MSVC 2019+).

---

## Usage

```bash
./bin/huffman compress   input.txt  input.huf
./bin/huffman decompress input.huf  restored.txt
./bin/huffman bench      input.txt  5          # 5 timed round trips
```

Example output:
```
Compressed 'samples/text.txt' -> 'text.huf'
  Original size    : 20.00 MB (20971574 bytes)
  Compressed size  : 9.83 MB (10310786 bytes)
  Compression ratio: 0.4917
  Space saved      : 50.83 %
  Unique symbols   : 48
  Avg code length  : 3.933 bits/byte  (entropy 3.903)
  Time             : 285.13 ms  (70.14 MB/s)
```

---

## Benchmark results

Generated with `./scripts/benchmark.sh 20 3` (20 MB per file, average of 3 runs,
`g++ -O2`, single thread). Timings depend on hardware.

| File         | Content                         | Original | Compressed | Ratio  | Saved   | Compress | Decompress |
|--------------|---------------------------------|---------:|-----------:|-------:|--------:|---------:|-----------:|
| `dna.txt`    | genome-style A/C/G/T sequence   | 20.0 MB  | 5.5 MB     | 0.276  | 72.4 %  | 164 ms   | 213 ms     |
| `text.txt`   | English-like prose              | 20.0 MB  | 9.8 MB     | 0.492  | 50.8 %  | 285 ms   | 477 ms     |
| `server.log` | application log lines           | 20.0 MB  | 12.9 MB    | 0.646  | 35.4 %  | 289 ms   | 475 ms     |
| `random.bin` | uniformly random bytes          | 20.0 MB  | 20.0 MB    | 1.000  | 0.0 %   | 508 ms   | 402 ms     |

All four round trips are verified byte-identical.

**Reading the numbers honestly:** Huffman coding is an *entropy coder* over single
bytes, so its result is bounded by the byte-level Shannon entropy (the tool prints
both, and the average code length is always within 1 bit of entropy). Data with a
small, skewed alphabet (DNA, simple text) compresses by 50 %+; real-world English text
typically lands around 40–45 %; data with many distinct bytes (logs with numbers)
compresses less; random or already-compressed data (ZIP, JPEG, MP4) cannot be
compressed at all — the overhead here is only ~330 bytes on a 20 MB file.

---

## How it works

1. **Count frequencies** – first pass over the input fills a 256-entry table.
2. **Build the tree** – every byte that occurs becomes a leaf in a `MinHeap`. The two
   lowest-frequency nodes are repeatedly popped and merged under a new parent until one
   root remains. Ties are broken by insertion order, so output is deterministic.
3. **Generate codes** – a DFS assigns `0` for left and `1` for right; frequent bytes get
   short codes, rare bytes long ones. The result is an optimal prefix-free code.
4. **Encode** – second pass writes each byte's code through `BitWriter`.
5. **Decode** – the tree is rebuilt from the header, then bits walk from the root to a
   leaf for each output byte, stopping after exactly `originalSize` symbols (so the
   zero padding in the last byte is never misread).

### Compressed file format

| Offset | Size | Field                                                    |
|-------:|-----:|----------------------------------------------------------|
| 0      | 4    | Magic number `HUFZ`                                      |
| 4      | 1    | Format version (`1`)                                     |
| 5      | 8    | Original size in bytes (little-endian `uint64`)          |
| 13     | …    | Bit stream: pre-order tree (`0` = internal, `1` + 8-bit symbol = leaf), then encoded data, zero-padded |

### Complexity

Let *n* be the file size and *k* ≤ 256 the number of distinct bytes.

- Tree construction: **O(k log k)** using the heap.
- Encoding and decoding: **O(n · L)** where *L* is the average code length (≤ 8 bits for typical input), i.e. linear in the input.
- Memory: **O(k)** for the tree plus fixed 64 KB I/O buffers — independent of *n*.

### Edge cases handled

- **Empty file** → 13-byte header only.
- **Single distinct byte** → a zero-frequency dummy sibling is added so the symbol gets a 1-bit code.
- **Corrupt input** → bad magic, wrong version, truncated data, or malformed trees raise clear errors instead of crashing.
- **Very skewed data** → code lengths are checked against a 64-bit limit (reached only for multi-terabyte inputs).

---

## Possible extensions

- Table-driven decoding (look up 8–12 bits at a time) for faster decompression.
- Canonical Huffman codes to shrink the header further.
- Multithreaded block-based compression.
- An LZ77 stage before Huffman (the idea behind DEFLATE / ZIP) to exploit repeated strings, which would greatly improve results on logs and text.
