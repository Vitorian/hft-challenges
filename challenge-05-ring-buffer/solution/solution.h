#pragma once
// Challenge 05: Ring Buffer (SPSC)
// Edit this file and solution.cpp to implement your solution.
//
// This is a Single-Producer Single-Consumer ring buffer.
// The producer calls push() from one thread, the consumer calls pop() from another.
// You MUST ensure thread safety between the producer and consumer.

#include <cstdint>
#include <cstddef>
#include <mutex>
#include <vector>

namespace hftu {

class RingBuffer {
public:
    // Capacity is always a power of 2.
    explicit RingBuffer(size_t capacity);

    // Push a value (producer thread). Returns false if full.
    bool push(int64_t value);

    // Pop a value into out (consumer thread). Returns false if empty.
    bool pop(int64_t& out);

    // Number of elements currently stored.
    size_t size() const;

private:
    std::vector<int64_t> buf_;
    size_t capacity_;
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
    mutable std::mutex mtx_;
};

} // namespace hftu
