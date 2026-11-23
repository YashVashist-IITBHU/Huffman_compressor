#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "BitIO.hpp"

namespace huff {

using FrequencyTable = std::array<std::uint64_t, 256>;

// A prefix code for one byte value: the low `length` bits of `bits`, MSB first.
struct Code {
    std::uint64_t bits = 0;
    std::uint8_t length = 0;
};
using CodeTable = std::array<Code, 256>;

// Huffman tree stored as a node pool (indices instead of raw pointers), so the
// whole tree is freed automatically and copying/moving is trivial.
class HuffmanTree {
public:
    static constexpr int kNull = -1;
    static constexpr unsigned kMaxCodeLength = 64;

    struct Node {
        std::uint64_t freq = 0;
        int left = kNull;
        int right = kNull;
        std::uint8_t symbol = 0;
        bool isLeaf() const { return left == kNull && right == kNull; }
    };

    HuffmanTree() = default;

    // Builds an optimal prefix-code tree from byte frequencies using a min-heap.
    static HuffmanTree build(const FrequencyTable& freq);

    // Compact pre-order encoding: internal node = 0, leaf = 1 + 8-bit symbol.
    void serialize(BitWriter& writer) const;
    static HuffmanTree deserialize(BitReader& reader);

    CodeTable buildCodeTable() const;

    bool empty() const { return root_ == kNull; }
    int root() const { return root_; }
    const Node& node(int index) const { return nodes_[static_cast<std::size_t>(index)]; }
    std::size_t nodeCount() const { return nodes_.size(); }
    std::size_t leafCount() const;

private:
    int addLeaf(std::uint8_t symbol, std::uint64_t freq);
    int addInternal(int left, int right);
    void assignCodes(int index, std::uint64_t bits, unsigned depth, CodeTable& table) const;
    void serializeNode(int index, BitWriter& writer) const;
    int deserializeNode(BitReader& reader, unsigned depth);

    std::vector<Node> nodes_;
    int root_ = kNull;
};

}  // namespace huff
