#include "undertow/envelope.h"

#include <algorithm>
#include <cmath>

namespace undertow {

void Envelope::prepare(double sampleRate) {
    sampleRate_ = sampleRate;
    recomputeAttack();
    recomputeDecay();
    recomputeRelease();
}

void Envelope::reset() {
    stage_ = Stage::Idle;
    level_ = 0.0;
}

void Envelope::setAttackMs(double ms) {
    attackMs_ = std::clamp(ms, kMinAttackMs, kMaxAttackMs);
    recomputeAttack();
}

void Envelope::setDecayMs(double ms) {
    decayMs_ = std::clamp(ms, kMinDecayMs, kMaxDecayMs);
    recomputeDecay();
}

void Envelope::setSustain(double level) {
    sustain_ = std::clamp(level, 0.0, 1.0);
}

void Envelope::setReleaseMs(double ms) {
    releaseMs_ = std::clamp(ms, kMinReleaseMs, kMaxReleaseMs);
    recomputeRelease();
}

void Envelope::noteOn() {
    stage_ = Stage::Attack;
}

void Envelope::noteOff() {
    if (stage_ != Stage::Idle)
        stage_ = Stage::Release;
}

float Envelope::process() {
    switch (stage_) {
        case Stage::Idle:
            break;

        case Stage::Attack:
            level_ += attackInc_;
            if (level_ >= 1.0) {
                level_ = 1.0;
                stage_ = Stage::Decay;
            }
            break;

        case Stage::Decay:
            level_ = sustain_ + (level_ - sustain_) * decayMul_;
            if (std::fabs(level_ - sustain_) <= kIdleThreshold) {
                level_ = sustain_;
                stage_ = Stage::Sustain;
            }
            break;

        case Stage::Sustain:
            level_ = sustain_;
            break;

        case Stage::Release:
            level_ *= releaseMul_;
            if (level_ <= kIdleThreshold) {
                level_ = 0.0;
                stage_ = Stage::Idle;
            }
            break;
    }

    return static_cast<float>(level_);
}

float Envelope::level() const { return static_cast<float>(level_); }

Envelope::Stage Envelope::stage() const { return stage_; }

void Envelope::recomputeAttack() {
    const double samples = attackMs_ * sampleRate_ / 1000.0;
    attackInc_ = 1.0 / samples;
}

void Envelope::recomputeDecay() {
    const double samples = decayMs_ * sampleRate_ / 1000.0;
    decayMul_ = std::exp(std::log(0.01) / samples);
}

void Envelope::recomputeRelease() {
    const double samples = releaseMs_ * sampleRate_ / 1000.0;
    releaseMul_ = std::exp(std::log(0.01) / samples);
}

} // namespace undertow
