// Challenge 06: Seqlock — Skeleton Implementation
// This is a correct but slow mutex-based reference. You can do MUCH better!
// The real seqlock uses a sequence counter and memory fences — no mutexes.

#include "solution.h"

namespace hftu {

Seqlock::Seqlock() {}

void Seqlock::write(const Payload& data) {
    std::lock_guard<std::mutex> lock(mtx_);
    data_ = data;
}

Payload Seqlock::read() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return data_;
}

} // namespace hftu
