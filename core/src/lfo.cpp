#include "undertow/lfo.h"

#include <cmath>
#include <cstdint>

namespace undertow {

// 17 rate choices mapped to cycle lengths in beats.
// Straight:       4 bars  2 bars  1 bar  1/2  1/4  1/8  1/16  1/32
// Triplet:        1/4T    1/8T    1/16T
// Dotted/poly:    3/16    5/16    7/16    3/4  5/4   7/4
static constexpr double kCycleBeats[kLfoRates] = {
    16.0,        // 4 bars
    8.0,         // 2 bars
    4.0,         // 1 bar
    2.0,         // 1/2
    1.0,         // 1/4
    0.5,         // 1/8
    0.25,        // 1/16
    0.125,       // 1/32
    2.0 / 3.0,   // 1/4T
    1.0 / 3.0,   // 1/8T
    1.0 / 6.0,   // 1/16T
    0.75,        // 3/16
    1.25,        // 5/16
    1.75,        // 7/16
    3.0,         // 3/4
    5.0,         // 5/4
    7.0,         // 7/4
};

double lfoCycleBeats(int rate) {
    return kCycleBeats[rate];
}

// ── Lfo setters ──────────────────────────────────────────────────────

void Lfo::setRate(int rate)       { rate_ = rate; }
void Lfo::setShape(LfoShape shape) { shape_ = shape; }
void Lfo::setPhaseOffset(double offset) { phaseOffset_ = offset; }
void Lfo::setSeed(std::uint32_t seed)   { seed_ = seed; }

// ── Shape helpers ────────────────────────────────────────────────────

static double sineAt(double phase) {
    return std::sin(2.0 * M_PI * phase);
}

static double triangleAt(double phase) {
    // 0→0  0.25→1  0.5→0  0.75→-1  1→0
    if (phase < 0.25)
        return 4.0 * phase;
    if (phase < 0.75)
        return 2.0 - 4.0 * phase;
    return -4.0 + 4.0 * phase;
}

static double sawDownAt(double phase) {
    return 1.0 - 2.0 * phase;
}

static double sawUpAt(double phase) {
    return -1.0 + 2.0 * phase;
}

static double squareAt(double phase) {
    return phase < 0.5 ? 1.0 : -1.0;
}

// ── Sample & Hold hash ───────────────────────────────────────────────

// A simple but effective integer hash (Murmur3 finalizer) to turn a
// (seed, cycle) pair into a deterministic value in [-1, 1].
double Lfo::sampleHold(std::uint32_t seed, std::int64_t cycle) {
    // Mix seed and cycle into a single 32-bit hash input.
    // Split the 64-bit cycle into two 32-bit halves and fold them in.
    std::uint32_t h = seed;
    h ^= static_cast<std::uint32_t>(cycle) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= static_cast<std::uint32_t>(cycle >> 32) + 0x9e3779b9 + (h << 6) + (h >> 2);
    // Murmur3 32-bit finalizer
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    // Map to [-1, 1)
    return (static_cast<double>(h) / static_cast<double>(0x80000000u)) - 1.0;
}

// ── valueAt ──────────────────────────────────────────────────────────

double Lfo::valueAt(double ppq) const {
    const double cycle = kCycleBeats[rate_];
    const double rawPhase = ppq / cycle + phaseOffset_;
    const double phase = rawPhase - std::floor(rawPhase);  // [0, 1)

    switch (shape_) {
    case LfoShape::Sine:       return sineAt(phase);
    case LfoShape::Triangle:   return triangleAt(phase);
    case LfoShape::SawDown:    return sawDownAt(phase);
    case LfoShape::SawUp:      return sawUpAt(phase);
    case LfoShape::Square:     return squareAt(phase);
    case LfoShape::SampleHold: return sampleHold(seed_, static_cast<std::int64_t>(std::floor(rawPhase)));
    }
    return 0.0;  // unreachable
}

} // namespace undertow
