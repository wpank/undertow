#include "undertow/output_stage.h"
#include "undertow/params.h"
#include <algorithm>
#include <cmath>

namespace undertow {

namespace {

constexpr double kPi      = 3.14159265358979323846;
constexpr double kDenorm  = 1e-20;        // denormal flush threshold
constexpr double kDcFreq  = 5.0;          // DC blocker cutoff (Hz)
constexpr double kGainTau = 0.005;        // master gain smoothing time constant (5 ms)
constexpr double kWidthMs = 12.0;         // width delay (ms)
constexpr double kClipS   = 0.5;          // clip hold duration (seconds)

inline double flushDenorm(double x) {
    return (std::fabs(x) < kDenorm) ? 0.0 : x;
}

}  // namespace

// ---------------------------------------------------------------------------

float softClip(float x) {
    const float ax = std::fabs(x);
    if (ax <= 0.5f)
        return x;   // identity through the linear region

    const float sign = (x >= 0.0f) ? 1.0f : -1.0f;
    constexpr float kRange   = 0.466f;   // 0.966 - 0.5
    constexpr float kCeiling = 0.966f;

    // Guard against infinity / huge values
    if (ax > 1e10f)
        return sign * kCeiling;

    const float result = 0.5f + kRange * std::tanhf((ax - 0.5f) / kRange);
    return sign * std::min(result, kCeiling);
}

// ---------------------------------------------------------------------------

void OutputStage::prepare(double sampleRate) {
    sampleRate_ = std::max(sampleRate, 1.0);

    // DC blocker coefficient: pole at R = exp(-2*pi*fc/fs)
    dcR_ = std::exp(-2.0 * kPi * kDcFreq / sampleRate_);

    // Master gain one-pole smoother
    gainSmooth_ = 1.0 - std::exp(-1.0 / (kGainTau * sampleRate_));

    // Width delay line (12 ms)
    delaySize_ = static_cast<std::size_t>(std::round(kWidthMs * 0.001 * sampleRate_));
    delaySize_ = std::max<std::size_t>(delaySize_, 1);
    delayLine_.assign(delaySize_, 0.0f);
    delayWrite_ = 0;

    // Clip hold in samples
    clipHold_ = kClipS * sampleRate_;

    // Initialise crossover coefficients (also sets hpSideC_)
    updateCrossover();
}

void OutputStage::reset() {
    // DC blocker
    dcX1_ = 0.0;
    dcY1_ = 0.0;

    // Master gain: snap to target (no smoothing transient after reset)
    masterCurrent_ = masterTarget_;

    // Crossover biquads
    for (auto& b : lp_) b.resetState();
    for (auto& b : hp_) b.resetState();

    // Side-path one-pole HP
    hpSideLp_ = 0.0;

    // Width delay
    std::fill(delayLine_.begin(), delayLine_.end(), 0.0f);
    delayWrite_ = 0;

    // Clip detector
    clipCountdown_ = 0.0;
}

void OutputStage::setParams(const float* values) {
    satDrive_  = static_cast<double>(values[param::SatDrive]);
    width_     = static_cast<double>(values[param::Width]);
    monoFreq_  = static_cast<double>(values[param::MonoFreq]);

    const double dB = static_cast<double>(values[param::MasterGain]);
    masterTarget_ = std::pow(10.0, dB / 20.0);

    updateCrossover();
    updateWidth();
}

// ---------------------------------------------------------------------------

void OutputStage::updateCrossover() {
    const double fc = std::clamp(monoFreq_, 20.0, sampleRate_ * 0.499);
    const double g  = std::tan(kPi * fc / sampleRate_);
    const double k  = std::sqrt(2.0);      // 2nd-order Butterworth damping
    const double g2 = g * g;

    // Common denominator
    const double a0 = 1.0 + k * g + g2;

    // 2nd-order Butterworth LP: numerator = g^2*(1 + 2z^-1 + z^-2)
    const double lpB0 =  g2  / a0;
    const double lpB1 = 2.0 * g2 / a0;
    const double lpB2 =  g2  / a0;

    // 2nd-order Butterworth HP: numerator = (1 - 2z^-1 + z^-2)
    const double hpB0 =  1.0  / a0;
    const double hpB1 = -2.0  / a0;
    const double hpB2 =  1.0  / a0;

    // Shared denominator coefficients (same for LP and HP)
    const double a1 = 2.0 * (g2 - 1.0) / a0;
    const double a2 = (1.0 - k * g + g2) / a0;

    // Both LP sections are identical (LR4 = two cascaded identical Butterworth LP2)
    for (auto& b : lp_) {
        b.b0 = lpB0; b.b1 = lpB1; b.b2 = lpB2;
        b.a1 = a1;   b.a2 = a2;
    }
    // Both HP sections are identical
    for (auto& b : hp_) {
        b.b0 = hpB0; b.b1 = hpB1; b.b2 = hpB2;
        b.a1 = a1;   b.a2 = a2;
    }

    // One-pole HP coefficient for the side path (steeper low-frequency rejection)
    hpSideC_ = std::exp(-2.0 * kPi * fc / sampleRate_);
}

void OutputStage::updateWidth() {
    const double theta = width_ * kPi / 3.5;
    cosTheta_ = std::cos(theta);
    sinTheta_ = std::sin(theta);
}

// ---------------------------------------------------------------------------

void OutputStage::process(float mono, float& left, float& right) {
    double x = static_cast<double>(mono);

    // ---- 1. DC blocker: y[n] = x[n] - x[n-1] + R * y[n-1] ----
    const double dcY = x - dcX1_ + dcR_ * dcY1_;
    dcX1_ = x;
    dcY1_ = dcY;
    // Flush both states when both are negligible (exact zero once silent)
    if (std::fabs(dcX1_) < kDenorm && std::fabs(dcY1_) < kDenorm) {
        dcX1_ = 0.0;
        dcY1_ = 0.0;
    }
    x = dcY;

    // ---- 2. Saturation: cross-fade identity <-> tanh(g*x)/g ----
    // At satDrive = 0 the output is exactly x (bypassed).
    // At satDrive = 1 the output is tanh(4*x)/4  (g = 1 + 3*1 = 4).
    if (satDrive_ > 0.0) {
        const double g   = 1.0 + 3.0 * satDrive_;
        const double sat = std::tanh(g * x) / g;
        x = x + satDrive_ * (sat - x);
    }

    // ---- 3. Master gain (smoothed) ----
    masterCurrent_ += gainSmooth_ * (masterTarget_ - masterCurrent_);
    x *= masterCurrent_;

    // ---- 4. High-pass for the side/width path (4th-order Linkwitz-Riley + one-pole) ----
    // The mono path is x (untouched after DC blocking, saturation, and gain).
    double high = x;
    high = hp_[0].process(high);
    high = hp_[1].process(high);

    // Cascade a one-pole HP at mono_freq for steeper low-frequency rejection.
    // HP = input - LP, where LP is a one-pole low-pass.
    const double sideLp = hpSideC_ * hpSideLp_ + (1.0 - hpSideC_) * high;
    hpSideLp_ = sideLp;
    high -= sideLp;

    // Flush HP biquad states
    for (auto& b : hp_) {
        if (std::fabs(b.z1) < kDenorm && std::fabs(b.z2) < kDenorm) {
            b.z1 = 0.0; b.z2 = 0.0;
        }
    }
    // Flush one-pole LP state
    if (std::fabs(hpSideLp_) < kDenorm)
        hpSideLp_ = 0.0;

    // ---- 5. Width: delay the high band by 12 ms ----
    const double delayed = static_cast<double>(delayLine_[delayWrite_]);
    delayLine_[delayWrite_] = static_cast<float>(high);
    delayWrite_ = (delayWrite_ + 1) % delaySize_;

    // ---- 6. Stereo output ----
    // The mono signal is x (direct path). The width/side signal is a delayed,
    // high-passed copy scaled by sin(theta).
    const double side = sinTheta_ * delayed;
    const double l = x + side;
    const double r = x - side;

    // ---- 7. Clip detection (before the clipper) ----
    if (std::fabs(l) > 1.0 || std::fabs(r) > 1.0)
        clipCountdown_ = clipHold_;
    if (clipCountdown_ > 0.0)
        clipCountdown_ -= 1.0;

    // ---- 8. Soft clip each channel ----
    left  = softClip(static_cast<float>(l));
    right = softClip(static_cast<float>(r));
}

bool OutputStage::clipped() const {
    return clipCountdown_ > 0.0;
}

}  // namespace undertow
