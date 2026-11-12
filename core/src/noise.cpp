#include "undertow/noise.h"

#include <cstdint>

namespace undertow {

void Noise::reset() {
    state_ = kSeed;
}

float Noise::process() {
    unsigned int x = state_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state_ = x;

    const double u = static_cast<double>(x) * (1.0 / 4294967296.0);
    return static_cast<float>((u * 2.0 - 1.0) * kScale);
}

}  // namespace undertow
