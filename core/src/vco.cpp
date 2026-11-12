#include "undertow/vco.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace undertow {
namespace {
constexpr double kPi = 3.14159265358979323846;
}

double Vco::wrapPhase(double phase) {
    phase -= std::floor(phase);
    if (phase < 0.0)
        phase += 1.0;
    return phase;
}

double Vco::triangleAt(double phase) {
    if (phase < 0.25)
        return 4.0 * phase;
    if (phase < 0.75)
        return 2.0 - 4.0 * phase;
    return 4.0 * phase - 4.0;
}

double Vco::polyBlep(double phase, double dt) {
    if (dt <= 0.0)
        return 0.0;
    if (phase < dt) {
        const double x = phase / dt;
        return 2.0 * x - x * x - 1.0;
    }
    if (phase > 1.0 - dt) {
        const double x = (phase - 1.0) / dt;
        return x * x + 2.0 * x + 1.0;
    }
    return 0.0;
}

void Vco::prepare(double sampleRate) {
    sampleRate_ = sampleRate > 1.0 ? sampleRate : 1.0;
}

void Vco::reset(double phase) {
    phase_ = wrapPhase(phase);
    lastPhase_ = phase_;
    currentParity_ = 0;
    lastParity_ = 0;
}

void Vco::setWave(Wave wave) {
    wave_ = wave;
}

void Vco::setFrequency(double hz) {
    frequency_ = hz < 0.0 ? 0.0 : hz;
}

float Vco::process() {
    const double playedPhase = phase_;
    lastPhase_ = playedPhase;
    lastParity_ = currentParity_;

    const double dt = frequency_ / sampleRate_;
    float out = 0.0f;
    if (frequency_ < 0.5 * sampleRate_) {
        if (wave_ == Wave::Triangle) {
            out = static_cast<float>(triangleAt(playedPhase));
        } else {
            const double square = playedPhase < 0.5 ? 1.0 : -1.0;
            const double shaped = square + polyBlep(playedPhase, dt) - polyBlep(wrapPhase(playedPhase + 0.5), dt);
            out = static_cast<float>(shaped);
        }
    }

    const double nextPhase = phase_ + dt;
    if (nextPhase >= 1.0) {
        const double wraps = std::floor(nextPhase);
        phase_ = nextPhase - wraps;
        if (static_cast<unsigned long long>(wraps) & 1ULL)
            currentParity_ ^= 1U;
    } else {
        phase_ = nextPhase;
    }
    return out;
}

float Vco::triangle() const {
    return static_cast<float>(triangleAt(lastPhase_));
}

float Vco::sub(int octaves) const {
    if (octaves == 0)
        return static_cast<float>(std::sin(2.0 * kPi * lastPhase_));
    if (octaves == -1)
        return static_cast<float>(std::sin(kPi * (static_cast<double>(lastParity_) + lastPhase_)));
    return 0.0f;
}

double Vco::phase() const {
    return phase_;
}

}  // namespace undertow
