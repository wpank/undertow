#include "undertow/oscillator.h"

#include <algorithm>
#include <cmath>

namespace undertow {

void Oscillator::prepare(double sampleRate) {
    sampleRate_ = sampleRate;
}

void Oscillator::setWave(Wave wave) { wave_ = wave; }

void Oscillator::setFrequency(double frequencyHz) { frequency_ = frequencyHz; }

void Oscillator::setPulseWidth(double width) { pulseWidth_ = std::clamp(width, 0.01, 0.99); }

void Oscillator::reset(double phase) { phase_ = phase - std::floor(phase); }

double Oscillator::polyBlep(double phase, double dt) const {
    if (phase < dt) {
        phase /= dt;
        return phase + phase - phase * phase - 1.0;
    }
    if (phase > 1.0 - dt) {
        phase = (phase - 1.0) / dt;
        return phase * phase + phase + phase + 1.0;
    }
    return 0.0;
}

float Oscillator::process() {
    const double dt = frequency_ / sampleRate_;
    double out = 0.0;

    if (wave_ == Wave::Saw) {
        out = 2.0 * phase_ - 1.0;
        out -= polyBlep(phase_, dt);
    } else {
        out = phase_ < pulseWidth_ ? 1.0 : -1.0;
        out += polyBlep(phase_, dt);
        const double p = std::fmod(phase_ - pulseWidth_ + 1.0, 1.0);
        out -= polyBlep(p, dt);
    }

    phase_ += dt;
    phase_ -= std::floor(phase_);
    return static_cast<float>(out);
}

void SineOscillator::prepare(double sampleRate) { sampleRate_ = sampleRate; }

void SineOscillator::setFrequency(double frequencyHz) { frequency_ = frequencyHz; }

void SineOscillator::reset(double phase) { phase_ = phase - std::floor(phase); }

float SineOscillator::process() {
    const float out = static_cast<float>(std::sin(2.0 * 3.14159265358979323846 * phase_));
    phase_ += frequency_ / sampleRate_;
    phase_ -= std::floor(phase_);
    return out;
}

} // namespace undertow
