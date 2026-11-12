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
constexpr float kClipCeiling = 0.966f;
constexpr float kClipRange = kClipCeiling - 0.5f;

inline double flushNearZero(double x) {
    return std::fabs(x) < kFlushThreshold ? 0.0 : x;
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
    gainSmooth_ = 1.0 - std::exp(-1.0 / (kGainTauSeconds * sampleRate_));
    clipHoldSamples_ = std::max(1, static_cast<int>(std::ceil(kClipHoldSeconds * sampleRate_)));
    reset();
}

void OutputStage::reset() {
    dcX1_ = 0.0;
    dcY1_ = 0.0;
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
