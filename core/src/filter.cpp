#include "undertow/filter.h"
#include <cmath>
#include <algorithm>

namespace undertow {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Butterworth 4th-order damping for each 2nd-order section:
//   B4(s) = (s^2 + k1*s + 1)(s^2 + k2*s + 1)
//   k1 = 2*sin(pi/8),  k2 = 2*sin(3*pi/8)
constexpr float kButterK1 = 0.7653668647301795f;
constexpr float kButterK2 = 1.8477590650225735f;

// Below this magnitude, flush state to zero to avoid denormals.
constexpr float kDenormal = 1e-18f;

inline float flushDenormal(float x) {
    return (std::fabs(x) < kDenormal) ? 0.0f : x;
}

// Smooth limiter: linear for |x| <= threshold, smoothly bounded to ±bound
// for |x| > threshold.  C1-continuous at the join.
inline float softLimit(float x, float threshold, float bound) {
    const float ax = std::fabs(x);
    if (ax <= threshold) return x;
    const float excess = ax - threshold;
    const float headroom = bound - threshold;
    const float soft = headroom * std::tanhf(excess / headroom);
    return std::copysignf(threshold + soft, x);
}

} // anonymous namespace

// ---------------------------------------------------------------------------

void LowpassFilter::prepare(double sampleRate) {
    sampleRate_ = std::max(sampleRate, 1.0);
    const double fc = std::min(cutoff_, sampleRate_ * 0.499);
    g_ = static_cast<float>(std::tan(kPi * fc / sampleRate_));
}

void LowpassFilter::setCutoff(double hz) {
    cutoff_ = std::clamp(hz, 20.0, sampleRate_ * 0.499);
    const double fc = std::min(cutoff_, sampleRate_ * 0.499);
    g_ = static_cast<float>(std::tan(kPi * fc / sampleRate_));
}

void LowpassFilter::setResonance(double r) {
    resonance_ = std::clamp(r, 0.0, 1.0);
}

void LowpassFilter::setDrive(double d) {
    drive_ = std::clamp(d, 0.0, 1.0);
}

void LowpassFilter::reset() {
    for (auto& s : stage_) {
        s.ic1eq = 0.0f;
        s.ic2eq = 0.0f;
    }
}

float LowpassFilter::process(float input) {
    // ---- Drive: cross-fade between linear and tanh-saturated input ----
    // At drive=0 the path is fully linear; at drive=1 it is fully saturated.
    // The saturation is symmetric (odd function) → only odd harmonics.
    // Makeup gain keeps the compressed path closer to the dry signal while the
    // internal headroom safeguards remain in place.
    float x = input;
    if (drive_ > 0.0) {
        const float drive = static_cast<float>(drive_);
        const float driveGain = 1.0f + drive * 10.0f;
        const float makeupGain = 1.0f + drive * 2.0f;
        const float sat = std::tanhf(x * driveGain) / driveGain;
        x = x + drive * (makeupGain * sat - x);
    }

    // ---- Resonance → per-stage damping ----
    // Preserve the phase-2.2 middle-of-knob response, but let the top end
    // continue increasing so phase 5 reaches the stronger resonance gain.
    // The curve is piecewise linear in dB: the early segment stays aligned with
    // the earlier checks, and the upper segment steepens toward the endpoint.
    const float resonance = static_cast<float>(resonance_);
    const float resonanceDb = (resonance <= 0.8f)
        ? (-3.01f + 19.39f * resonance)
        : (12.5f + 80.75f * (resonance - 0.8f));
    const float k1 = static_cast<float>(0.5412 * std::pow(10.0, -resonanceDb / 20.0));
    const float k2 = kButterK2;

    const float g = g_;

    // ---- Stage 1 (k1 < k2 → higher Q, provides most of the resonance peak) ----
    {
        const float t = 1.0f / (1.0f + g * (k1 + g));
        const float v1 = t * (stage_[0].ic1eq + g * (x - stage_[0].ic2eq));
        const float lp = g * v1 + stage_[0].ic2eq;
        stage_[0].ic1eq = flushDenormal(2.0f * v1 - stage_[0].ic1eq);
        stage_[0].ic2eq = flushDenormal(2.0f * lp - stage_[0].ic2eq);
        // Limit state to prevent blowup under swept coefficients
        // Widened so full-level voice signals remain linear at drive 0.
        stage_[0].ic1eq = softLimit(stage_[0].ic1eq, 10.0f, 20.0f);
        stage_[0].ic2eq = softLimit(stage_[0].ic2eq, 10.0f, 20.0f);
        x = softLimit(lp, 10.0f, 20.0f);  // limit inter-stage signal
    }

    // ---- Stage 2 ----
    {
        const float t = 1.0f / (1.0f + g * (k2 + g));
        const float v1 = t * (stage_[1].ic1eq + g * (x - stage_[1].ic2eq));
        const float lp = g * v1 + stage_[1].ic2eq;
        stage_[1].ic1eq = flushDenormal(2.0f * v1 - stage_[1].ic1eq);
        stage_[1].ic2eq = flushDenormal(2.0f * lp - stage_[1].ic2eq);
        stage_[1].ic1eq = softLimit(stage_[1].ic1eq, 10.0f, 20.0f);
        stage_[1].ic2eq = softLimit(stage_[1].ic2eq, 10.0f, 20.0f);
        x = softLimit(lp, 3.5f, 4.0f);  // limit final output
    }

    return x;
}

}  // namespace undertow
