#include "undertow/ladder.h"

#include <algorithm>
#include <cmath>

namespace undertow {
namespace {

constexpr double kPi = 3.14159265358979323846;

constexpr double kMinCutoff = 20.0;
constexpr double kMaxCutoffRatio = 0.45;
constexpr double kDenorm = 1e-20;

inline double clampCutoff(double hz, double sampleRate) {
    const double upper = kMaxCutoffRatio * sampleRate;
    if (upper <= kMinCutoff)
        return upper;
    return std::clamp(hz, kMinCutoff, upper);
}

}  // namespace

double Ladder::shape(double x) {
    const double ax = std::fabs(x);
    if (ax < 1e-8) {
        const double x2 = x * x;
        return 1.0 - x2 / 3.0 + (2.0 * x2 * x2) / 15.0;
    }
    return std::tanh(x) / x;
}

double Ladder::flush(double x) {
    return std::fabs(x) < kDenorm ? 0.0 : x;
}

void Ladder::prepare(double sampleRate) {
    sampleRate_ = std::max(sampleRate, 1.0);
    cutoff_ = clampCutoff(cutoff_, sampleRate_);
    driveGain_ = 0.5;
    reset();
}

void Ladder::reset() {
    for (double& s : s_)
        s = 0.0;
    prevU_ = 0.0;
}

void Ladder::setCutoff(double hz) {
    cutoff_ = clampCutoff(hz, sampleRate_);
}

void Ladder::setResonance(double r) {
    resonance_ = std::clamp(r, 0.0, kMaxResonance);
}

void Ladder::setDrive(double d) {
    drive_ = std::clamp(d, 0.0, kMaxDrive);
    driveGain_ = 0.5 * std::pow(6.0, drive_);
}

float Ladder::process(float x) {
    const double k = 2.5 * resonance_;
    const double gain = driveGain_;
    const double f = std::tan(kPi * cutoff_ / sampleRate_);
    const double u = static_cast<double>(x) * gain * (1.0 + k);
    const double h = 0.5 * (u + prevU_);
    prevU_ = u;

    const double t0 = shape(h - k * s_[3]);
    const double t1 = shape(s_[0]);
    const double t2 = shape(s_[1]);
    const double t3 = shape(s_[2]);
    const double t4 = shape(s_[3]);

    const double g0 = 1.0 / (1.0 + f * t1);
    const double g1 = 1.0 / (1.0 + f * t2);
    const double g2 = 1.0 / (1.0 + f * t3);
    const double g3 = 1.0 / (1.0 + f * t4);

    const double f3 = f * t3 * g3;
    const double f2 = f * t2 * g2 * f3;
    const double f1 = f * t1 * g1 * f2;
    const double f0 = f * t0 * g0 * f1;

    const double y3 = (g3 * s_[3] + f3 * g2 * s_[2] + f2 * g1 * s_[1] + f1 * g0 * s_[0] + f0 * u) /
                      (1.0 + k * f0);
    const double xx = t0 * (u - k * y3);
    const double y0 = t1 * g0 * (s_[0] + f * xx);
    const double y1 = t2 * g1 * (s_[1] + f * y0);
    const double y2 = t3 * g2 * (s_[2] + f * y1);

    s_[0] += 2.0 * f * (xx - y0);
    s_[1] += 2.0 * f * (y0 - y1);
    s_[2] += 2.0 * f * (y1 - y2);
    s_[3] += 2.0 * f * (y2 - t4 * y3);

    if (std::fabs(s_[0]) < kDenorm && std::fabs(s_[1]) < kDenorm && std::fabs(s_[2]) < kDenorm &&
        std::fabs(s_[3]) < kDenorm && std::fabs(prevU_) < kDenorm) {
        reset();
        return 0.0f;
    }

    s_[0] = flush(s_[0]);
    s_[1] = flush(s_[1]);
    s_[2] = flush(s_[2]);
    s_[3] = flush(s_[3]);
    prevU_ = flush(prevU_);

    return static_cast<float>(y3 / gain);
}

}  // namespace undertow
