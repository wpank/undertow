#include "undertow/output_stage.h"
#include "undertow/params.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace undertow {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDcHz = 5.0;
constexpr double kGainTauSeconds = 0.005;
constexpr double kClipHoldSeconds = 0.5;
constexpr double kFlushThreshold = 1e-20;
constexpr double kHighpassHz = 20.0;
constexpr double kButterworthQ1 = 0.5411961;
constexpr double kButterworthQ2 = 1.3065630;
constexpr float kClipCeiling = 0.966f;
constexpr float kClipRange = kClipCeiling - 0.5f;

inline double flushNearZero(double x) {
    return std::fabs(x) < kFlushThreshold ? 0.0 : x;
}

std::array<double, 5> makeHighpass(double sampleRate, double cutoffHz, double q) {
    std::array<double, 5> coeffs{};
    const double limited = std::clamp(cutoffHz, 1.0, 0.499 * sampleRate);
    const double w0 = 2.0 * kPi * limited / sampleRate;
    const double cosW0 = std::cos(w0);
    const double sinW0 = std::sin(w0);
    const double alpha = sinW0 / (2.0 * q);
    const double a0 = 1.0 + alpha;
    coeffs[0] = ((1.0 + cosW0) * 0.5) / a0;
    coeffs[1] = (-(1.0 + cosW0)) / a0;
    coeffs[2] = coeffs[0];
    coeffs[3] = (-2.0 * cosW0) / a0;
    coeffs[4] = (1.0 - alpha) / a0;
    return coeffs;
}

double processBiquad(const std::array<double, 5>& coeffs, std::array<double, 2>& state, double x) {
    const double y = coeffs[0] * x + state[0];
    state[0] = coeffs[1] * x - coeffs[3] * y + state[1];
    state[1] = coeffs[2] * x - coeffs[4] * y;
    return y;
}

void flushState(std::array<double, 2>& state) {
    if (std::fabs(state[0]) < kFlushThreshold)
        state[0] = 0.0;
    if (std::fabs(state[1]) < kFlushThreshold)
        state[1] = 0.0;
}

}  // namespace

float softClip(float x) {
    if (std::fabs(x) <= 0.5f)
        return x;

    const float sign = std::signbit(x) ? -1.0f : 1.0f;
    const float magnitude = std::fabs(x);
    if (!std::isfinite(magnitude))
        return sign * kClipCeiling;

    const float shaped = 0.5f + kClipRange * std::tanh((magnitude - 0.5f) / kClipRange);
    return sign * std::min(shaped, kClipCeiling);
}

void OutputStage::prepare(double sampleRate) {
    sampleRate_ = std::max(sampleRate, 1.0);
    dcR_ = std::exp(-2.0 * kPi * kDcHz / sampleRate_);
    hpCoeff_[0] = makeHighpass(sampleRate_, kHighpassHz, kButterworthQ1);
    hpCoeff_[1] = makeHighpass(sampleRate_, kHighpassHz, kButterworthQ2);
    gainSmooth_ = 1.0 - std::exp(-1.0 / (kGainTauSeconds * sampleRate_));
    clipHoldSamples_ = std::max(1, static_cast<int>(std::ceil(kClipHoldSeconds * sampleRate_)));
    reset();
}

void OutputStage::reset() {
    dcX1_ = 0.0;
    dcY1_ = 0.0;
    hpState_[0] = {0.0, 0.0};
    hpState_[1] = {0.0, 0.0};
    masterCurrent_ = masterTarget_;
    clipSamplesRemaining_ = 0;
}

void OutputStage::setParams(const float* values) {
    const double masterDb = static_cast<double>(values[param::MasterGain]);
    masterTarget_ = std::pow(10.0, masterDb / 20.0);
}

float OutputStage::process(float x) {
    double y = static_cast<double>(x) - dcX1_ + dcR_ * dcY1_;
    dcX1_ = static_cast<double>(x);
    dcY1_ = y;
    if (std::fabs(dcX1_) < kFlushThreshold && std::fabs(dcY1_) < kFlushThreshold) {
        dcX1_ = 0.0;
        dcY1_ = 0.0;
        y = 0.0;
    } else {
        y = flushNearZero(y);
    }

    y = processBiquad(hpCoeff_[0], hpState_[0], y);
    y = processBiquad(hpCoeff_[1], hpState_[1], y);
    y = flushNearZero(y);
    flushState(hpState_[0]);
    flushState(hpState_[1]);

    masterCurrent_ += gainSmooth_ * (masterTarget_ - masterCurrent_);
    y *= masterCurrent_;

    if (std::fabs(y) > 1.0)
        clipSamplesRemaining_ = clipHoldSamples_;
    else if (clipSamplesRemaining_ > 0)
        --clipSamplesRemaining_;

    return softClip(static_cast<float>(y));
}

bool OutputStage::clipped() const {
    return clipSamplesRemaining_ > 0;
}

}  // namespace undertow
