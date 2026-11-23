#pragma once

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace huff {

// A binary min-heap backed by a std::vector. `Compare(a, b)` returns true when
// `a` should come out of the heap before `b` (std::less gives a min-heap).
// Implemented by hand (instead of std::priority_queue) to keep the data
// structure explicit and testable.
template <typename T, typename Compare = std::less<T>>
class MinHeap {
public:
    explicit MinHeap(Compare cmp = Compare()) : cmp_(std::move(cmp)) {}

    void push(T value) {
        data_.push_back(std::move(value));
        siftUp(data_.size() - 1);
    }

    const T& top() const {
        if (data_.empty()) throw std::out_of_range("MinHeap::top on empty heap");
        return data_.front();
    }

    T pop() {
        if (data_.empty()) throw std::out_of_range("MinHeap::pop on empty heap");
        T result = std::move(data_.front());
        data_.front() = std::move(data_.back());
        data_.pop_back();
        if (!data_.empty()) siftDown(0);
        return result;
    }

    std::size_t size() const { return data_.size(); }
    bool empty() const { return data_.empty(); }
    void reserve(std::size_t n) { data_.reserve(n); }

private:
    void siftUp(std::size_t i) {
        while (i > 0) {
            std::size_t parent = (i - 1) / 2;
            if (!cmp_(data_[i], data_[parent])) break;
            std::swap(data_[i], data_[parent]);
            i = parent;
        }
    }

    void siftDown(std::size_t i) {
        const std::size_t n = data_.size();
        while (true) {
            std::size_t left = 2 * i + 1;
            std::size_t right = left + 1;
            std::size_t best = i;
            if (left < n && cmp_(data_[left], data_[best])) best = left;
            if (right < n && cmp_(data_[right], data_[best])) best = right;
            if (best == i) break;
            std::swap(data_[i], data_[best]);
            i = best;
        }
    }

    std::vector<T> data_;
    Compare cmp_;
};

}  // namespace huff
