#pragma once

#include <cstdint>

namespace undertow {

enum class LfoShape { Sine, Triangle, SawDown, SawUp, Square, SampleHold };

/// Number of LFO rate choices (indices 0–16).
constexpr int kLfoRates = 17;

/// Returns the cycle length in quarter-note beats for rate index [0, kLfoRates).
double lfoCycleBeats(int rate);

/// Stateless beat-locked LFO: valueAt(ppq) is a pure function of the beat
/// position and the configured rate, shape, phase offset, and seed.
class Lfo {
public:
    void setRate(int rate);
    void setShape(LfoShape shape);
    void setPhaseOffset(double offset);   // 0–1
    void setSeed(std::uint32_t seed);

    /// Returns the LFO value in [-1, 1] at the given host beat position (ppq).
    double valueAt(double ppq) const;

private:
    static double sampleHold(std::uint32_t seed, std::int64_t cycle);

    int rate_ = 0;
    LfoShape shape_ = LfoShape::Sine;
    double phaseOffset_ = 0.0;
    std::uint32_t seed_ = 1;
};

} // namespace undertow
