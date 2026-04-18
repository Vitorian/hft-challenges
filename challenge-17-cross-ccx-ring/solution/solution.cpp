// Challenge 17: Ring Buffer (SPSC) — Skeleton Implementation
// This is a correct but slow mutex-based reference. You can do MUCH better!

#include "solution.h"

namespace hftu {

RingBuffer::RingBuffer(size_t capacity)
    : buf_(capacity), capacity_(capacity) {}

bool RingBuffer::push(const Message& msg) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (count_ == capacity_) return false;
    buf_[tail_] = msg;
    tail_ = (tail_ + 1) % capacity_;
    ++count_;
    return true;
}

bool RingBuffer::pop(Message& out) {
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
