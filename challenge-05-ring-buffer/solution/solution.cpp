// Challenge 05: Ring Buffer (SPSC) — Skeleton Implementation
// This is a correct but slow mutex-based reference. You can do MUCH better!

#include "solution.h"

namespace hftu {

RingBuffer::RingBuffer(size_t capacity)
    : buf_(capacity), capacity_(capacity) {}

bool RingBuffer::push(int64_t value) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (count_ == capacity_) return false;
    buf_[tail_] = value;
    tail_ = (tail_ + 1) % capacity_;
    ++count_;
    return true;
}

bool RingBuffer::pop(int64_t& out) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (count_ == 0) return false;
    out = buf_[head_];
    head_ = (head_ + 1) % capacity_;
    --count_;
    return true;
}

size_t RingBuffer::size() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return count_;
}

} // namespace hftu
