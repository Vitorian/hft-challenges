#pragma once
// Challenge 06: Seqlock
// Edit this file and solution.cpp to implement your solution.
//
// A seqlock allows one writer and multiple readers to share data
// without blocking. Readers retry if they detect a concurrent write.
// No mutexes allowed in your final solution — this is a lock-free exercise.

#include <cstdint>
#include <mutex>

namespace hftu {

struct Payload {
    int64_t a;
    int64_t b;
    int64_t c;
    int64_t d;
};

class Seqlock {
public:
    Seqlock();

    // Update the protected payload.
    void write(const Payload& data);

    // Read the payload. Must never return a torn value.
    Payload read() const;

private:
    Payload data_{};
    mutable std::mutex mtx_;
};

} // namespace hftu
