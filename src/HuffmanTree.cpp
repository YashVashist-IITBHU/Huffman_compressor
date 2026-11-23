#include "HuffmanTree.hpp"

#include <stdexcept>

#include "MinHeap.hpp"

namespace huff {

namespace {

struct HeapItem {
    std::uint64_t freq;
    std::uint32_t order;  // tie-breaker so the tree is deterministic
    int node;
};

struct HeapItemLess {
    bool operator()(const HeapItem& a, const HeapItem& b) const {
        if (a.freq != b.freq) return a.freq < b.freq;
        return a.order < b.order;
    }
};

}  // namespace

int HuffmanTree::addLeaf(std::uint8_t symbol, std::uint64_t freq) {
    Node n;
    n.freq = freq;
    n.symbol = symbol;
    nodes_.push_back(n);
    return static_cast<int>(nodes_.size() - 1);
}

int HuffmanTree::addInternal(int left, int right) {
    Node n;
    n.freq = node(left).freq + node(right).freq;
    n.left = left;
    n.right = right;
    nodes_.push_back(n);
    return static_cast<int>(nodes_.size() - 1);
}

HuffmanTree HuffmanTree::build(const FrequencyTable& freq) {
    HuffmanTree tree;
    tree.nodes_.reserve(511);  // at most 256 leaves + 255 internal nodes

    MinHeap<HeapItem, HeapItemLess> heap;
    heap.reserve(256);

    int lastSymbol = -1;
    for (int s = 0; s < 256; ++s) {
        const std::uint64_t f = freq[static_cast<std::size_t>(s)];
        if (f == 0) continue;
        int idx = tree.addLeaf(static_cast<std::uint8_t>(s), f);
        heap.push({f, static_cast<std::uint32_t>(s), idx});
        lastSymbol = s;
    }

    if (heap.empty()) return tree;  // empty input -> empty tree

    // A single distinct symbol would get a zero-length code. Add a dummy
    // sibling with frequency 0 so every symbol gets at least one bit.
    if (heap.size() == 1) {
        auto dummy = static_cast<std::uint8_t>((lastSymbol + 1) & 0xFF);
        int idx = tree.addLeaf(dummy, 0);
        heap.push({0, static_cast<std::uint32_t>(dummy), idx});
    }

    std::uint32_t nextOrder = 256;
    while (heap.size() > 1) {
        HeapItem a = heap.pop();
        HeapItem b = heap.pop();
        int parent = tree.addInternal(a.node, b.node);
        heap.push({a.freq + b.freq, nextOrder++, parent});
    }
    tree.root_ = heap.pop().node;
    return tree;
}

void HuffmanTree::assignCodes(int index, std::uint64_t bits, unsigned depth, CodeTable& table) const {
    const Node& n = node(index);
    if (n.isLeaf()) {
        if (depth == 0) depth = 1;  // lone leaf at the root (only from foreign input)
        table[n.symbol] = Code{bits, static_cast<std::uint8_t>(depth)};
        return;
    }
    if (depth + 1 > kMaxCodeLength) {
        throw std::runtime_error("Huffman code longer than 64 bits; input is too large/skewed");
    }
    assignCodes(n.left, bits << 1, depth + 1, table);
    assignCodes(n.right, (bits << 1) | 1u, depth + 1, table);
}

CodeTable HuffmanTree::buildCodeTable() const {
    CodeTable table{};
    if (!empty()) assignCodes(root_, 0, 0, table);
    return table;
}

std::size_t HuffmanTree::leafCount() const {
    std::size_t count = 0;
    for (const Node& n : nodes_) count += n.isLeaf() ? 1 : 0;
    return count;
}

void HuffmanTree::serializeNode(int index, BitWriter& writer) const {
    const Node& n = node(index);
    if (n.isLeaf()) {
        writer.writeBit(true);
        writer.writeByte(n.symbol);
    } else {
        writer.writeBit(false);
        serializeNode(n.left, writer);
        serializeNode(n.right, writer);
    }
}

void HuffmanTree::serialize(BitWriter& writer) const {
    if (empty()) throw std::logic_error("Cannot serialize an empty Huffman tree");
    serializeNode(root_, writer);
}

int HuffmanTree::deserializeNode(BitReader& reader, unsigned depth) {
    // Guards against corrupt or malicious input producing huge/deep trees.
    if (depth > kMaxCodeLength || nodes_.size() >= 511) {
        throw std::runtime_error("Corrupt compressed file: invalid Huffman tree");
    }
    if (reader.readBit()) {
        return addLeaf(reader.readByte(), 0);
    }
    // Reserve this node's slot first so children are added after it.
    nodes_.push_back(Node{});
    int self = static_cast<int>(nodes_.size() - 1);
    int left = deserializeNode(reader, depth + 1);
    int right = deserializeNode(reader, depth + 1);
    nodes_[static_cast<std::size_t>(self)].left = left;
    nodes_[static_cast<std::size_t>(self)].right = right;
    return self;
}

HuffmanTree HuffmanTree::deserialize(BitReader& reader) {
    HuffmanTree tree;
    tree.nodes_.reserve(511);
    tree.root_ = tree.deserializeNode(reader, 0);
    return tree;
}

}  // namespace huff
