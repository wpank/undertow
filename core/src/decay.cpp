#include "undertow/decay.h"

#include <algorithm>
#include <cmath>

namespace undertow {

namespace {
constexpr double kMinRate = 1.0;
}

void Decay::prepare(double sampleRate) {
    sampleRate_ = std::max(sampleRate, kMinRate);
    updateDecayCoeff();
}

void Decay::reset() {
    level_ = 0.0;
    start_ = 0.0;
    peak_ = 0.0;
    stageLen_ = 0.0;
    stagePos_ = 0.0;
    decayStart_ = 0.0;
    decayPos_ = 0.0;
    stage_ = Stage::Idle;
    active_ = false;
}

void Decay::setAttack(double ms) {
    attackMs_ = std::max(ms, 0.0);
}

void Decay::setDecay(double ms) {
    decayMs_ = std::max(ms, 0.0);
    updateDecayCoeff();
}

void Decay::trigger(float peak) {
    start_ = level_;
    peak_ = static_cast<double>(peak);
    const double riseSamples = attackSamples(attackMs_);
    const double glideSamples = std::max(riseSamples, kMinGlideMs * sampleRate_ / 1000.0);

    if (peak_ < start_) {
        stageLen_ = glideSamples;
    } else {
        stageLen_ = riseSamples;
    }

    stagePos_ = 0.0;
    decayPos_ = 0.0;
    decayStart_ = peak_;
    active_ = true;

    if (stageLen_ <= 0.0) {
        level_ = peak_;
        stage_ = Stage::Decay;
    } else {
        stage_ = Stage::Attack;
    }
}

float Decay::process() {
    switch (stage_) {
    case Stage::Idle:
        level_ = 0.0;
        active_ = false;
        return 0.0f;

    case Stage::Attack: {
        const double t = std::min(stagePos_, stageLen_) / stageLen_;
        const double shape = 0.5 * (1.0 - std::cos(kPi * t));
        level_ = start_ + (peak_ - start_) * shape;
        if (stagePos_ >= stageLen_) {
            stage_ = Stage::Decay;
            decayStart_ = peak_;
            decayPos_ = 0.0;
        } else {
            ++stagePos_;
        }
        if (std::fabs(level_) < kSilence && stage_ == Stage::Decay && decayStart_ == 0.0) {
            level_ = 0.0;
            stage_ = Stage::Idle;
            active_ = false;
        }
        return static_cast<float>(level_);
    }

    case Stage::Decay: {
        if (std::fabs(level_) < kSilence) {
            level_ = 0.0;
            stage_ = Stage::Idle;
            active_ = false;
            return 0.0f;
        }

        level_ = decayStart_ * std::pow(decayCoeff_, decayPos_);
        ++decayPos_;
        if (std::fabs(level_) < kSilence) {
            level_ = 0.0;
            stage_ = Stage::Idle;
            active_ = false;
        }
        return static_cast<float>(level_);
    }
    }

    return 0.0f;
}

float Decay::level() const {
    return static_cast<float>(level_);
}

bool Decay::active() const {
    return active_;
}

double Decay::attackSamples(double ms) const {
    if (ms <= 0.0)
        return 0.0;
    return ms * sampleRate_ / (1000.0 * kAttackShape);
}

void Decay::updateDecayCoeff() {
    if (decayMs_ <= 0.0) {
        decayCoeff_ = 0.0;
        return;
    }
    decayCoeff_ = std::pow(0.01, 1.0 / (decayMs_ * sampleRate_ / 1000.0));
}

}  // namespace undertow
