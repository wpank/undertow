#pragma once

#include <cstddef>

namespace spec {

// Counts the calls to the global operator new (every form: plain, array, nothrow, aligned) made on the
// calling thread since the counter was created. Each spec executable replaces operator new to do this, so
// any std::vector growth, std::string, std::function or `new` inside an audio callback is seen.
// Wrap only the audio-thread calls: doctest's own CHECK macros allocate.
class AllocationCounter {
public:
    AllocationCounter();
    std::size_t count() const;

private:
    std::size_t start_;
};

// Allocations made on this thread since the program started.
std::size_t allocationsOnThisThread();

} // namespace spec
