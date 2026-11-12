#include "undertow/voice.h"

#include "undertow/params.h"

#include <algorithm>
#include <cmath>

namespace undertow {
namespace {

Wave toWave(float value) {
    return value >= 0.5f ? Wave::Square : Wave::Triangle;
}

bool toSlowAttack(float value) {
    return value >= 0.5f;
}

int toSubOctaves(float value) {
    return value >= 0.5f ? -1 : 0;
}

}  // namespace

void Voice::prepare(double sampleRate) {
    sampleRate_ = std::max(sampleRate, 1.0);
    smoothStep_ = 1.0 - std::exp(-1.0 / (kSmoothMs * 0.001 * sampleRate_));

    vco1_.prepare(sampleRate_);
    vco2_.prepare(sampleRate_);
    ladder_.prepare(sampleRate_);
    pitchEnv_.prepare(sampleRate_);
    filterEnv_.prepare(sampleRate_);
    levelEnv_.prepare(sampleRate_);

    pitchEnv_.setAttack(0.0);
    reset();
}

void Voice::reset() {
    vco1_.reset();
    vco2_.reset();
    noise_.reset();
    ladder_.reset();
    pitchEnv_.reset();
    filterEnv_.reset();
    levelEnv_.reset();

    note_ = -1;
    noteHz_ = 0.0;
    lastVco1Hz_ = 0.0;
    lastCutoffHz_ = 0.0;
    lastLevel_ = 0.0f;
    smoothReady_ = false;
    idle_ = true;
}

void Voice::setParams(const float* values) {
    using namespace param;

    vco1Wave_ = toWave(values[Vco1Wave]);
    vco2Wave_ = toWave(values[Vco2Wave]);
    subOctaves_ = toSubOctaves(values[SubOctave]);
    pitchAmount_ = values[PitchEnv];
    filterAmount_ = values[VcfEnv];

    vco1LevelTarget_ = values[Vco1Level];
    vco2LevelTarget_ = values[Vco2Level];
    noiseLevelTarget_ = values[NoiseLevel];
    subLevelTarget_ = values[SubLevel];
    vco2TuneTarget_ = values[Vco2Tune];
    vcoFmTarget_ = values[VcoFm];
    cutoffOctavesTarget_ = std::log2(std::max(static_cast<double>(values[VcfCutoff]), 1.0));
    resonanceTarget_ = values[VcfRes];
    driveTarget_ = values[VcfDrive];

    pitchEnv_.setDecay(values[PitchDecay]);
    filterEnv_.setDecay(values[VcfDecay]);
    levelEnv_.setDecay(values[VcaDecay]);
    setAttackMode(toSlowAttack(values[VcaAttack]));

    vco1_.setWave(vco1Wave_);
    vco2_.setWave(vco2Wave_);

    if (!smoothReady_ || !active()) {
        setImmediateTargets();
        smoothReady_ = true;
    }
}

void Voice::hit(int note, float velocity) {
    if (velocity <= 0.0f)
        return;

    if (levelEnv_.level() < kRestartLevel) {
        vco1_.reset();
        vco2_.reset();
    }

    note_ = note;
    noteHz_ = midiToHz(note);
    pitchEnv_.trigger(1.0f);
    filterEnv_.trigger(velocity);
    levelEnv_.trigger(velocity);
    idle_ = false;
}

float Voice::process() {
    if (!levelEnv_.active()) {
        if (!idle_) {
            ladder_.reset();
            idle_ = true;
        }
        note_ = -1;
        noteHz_ = 0.0;
        lastVco1Hz_ = 0.0;
        lastCutoffHz_ = 0.0;
        lastLevel_ = 0.0f;
        return 0.0f;
    }

    smoothParams();

    vco1_.setWave(vco1Wave_);
    vco2_.setWave(vco2Wave_);

    const double pitch = pitchEnv_.process();
    lastVco1Hz_ = noteHz_ * std::pow(2.0, static_cast<double>(pitchAmount_) * pitch / kSemitonesPerOctave);
    vco1_.setFrequency(lastVco1Hz_);
    const float vco1 = vco1_.process();
    const float sub = vco1_.sub(subOctaves_);

    const double tuneRatio = std::pow(2.0, vco2Tune_ / kSemitonesPerOctave);
    const double fmRatio = 1.0 + vcoFm_ * static_cast<double>(vco1_.triangle());
    vco2_.setFrequency(lastVco1Hz_ * tuneRatio * fmRatio);
    const float vco2 = vco2_.process();

    const float mix = vco1Level_ * vco1 + vco2Level_ * vco2 + noiseLevel_ * noise_.process() + subLevel_ * sub;

    const double filter = filterEnv_.process();
    lastCutoffHz_ = std::pow(2.0, cutoffOctaves_ + static_cast<double>(filterAmount_) * filter);
    ladder_.setCutoff(lastCutoffHz_);
    ladder_.setResonance(resonance_);
    ladder_.setDrive(drive_);
    const float filtered = ladder_.process(mix);

    lastLevel_ = levelEnv_.process();
    const float out = filtered * lastLevel_;

    if (!levelEnv_.active()) {
        ladder_.reset();
        note_ = -1;
        noteHz_ = 0.0;
        lastVco1Hz_ = 0.0;
        lastCutoffHz_ = 0.0;
        lastLevel_ = 0.0f;
        idle_ = true;
        return 0.0f;
    }

    return out;
}

bool Voice::active() const {
    return levelEnv_.active();
}

int Voice::note() const {
    return active() ? note_ : -1;
}

double Voice::noteHz() const {
    return active() ? noteHz_ : 0.0;
}

double Voice::vco1Hz() const {
    return lastVco1Hz_;
}

double Voice::cutoffHz() const {
    return lastCutoffHz_;
}

float Voice::level() const {
    return lastLevel_;
}

void Voice::setAttackMode(bool slow) {
    filterEnv_.setAttack(slow ? kSlowFilterAttackMs : kFastFilterAttackMs);
    levelEnv_.setAttack(slow ? kSlowLevelAttackMs : kFastLevelAttackMs);
}

void Voice::setImmediateTargets() {
    vco1Level_ = vco1LevelTarget_;
    vco2Level_ = vco2LevelTarget_;
    noiseLevel_ = noiseLevelTarget_;
    subLevel_ = subLevelTarget_;
    vco2Tune_ = vco2TuneTarget_;
    vcoFm_ = vcoFmTarget_;
    cutoffOctaves_ = cutoffOctavesTarget_;
    resonance_ = resonanceTarget_;
    drive_ = driveTarget_;

    ladder_.setCutoff(std::pow(2.0, cutoffOctaves_));
    ladder_.setResonance(resonance_);
    ladder_.setDrive(drive_);
}

void Voice::smoothParams() {
    vco1Level_ += (vco1LevelTarget_ - vco1Level_) * static_cast<float>(smoothStep_);
    vco2Level_ += (vco2LevelTarget_ - vco2Level_) * static_cast<float>(smoothStep_);
    noiseLevel_ += (noiseLevelTarget_ - noiseLevel_) * static_cast<float>(smoothStep_);
    subLevel_ += (subLevelTarget_ - subLevel_) * static_cast<float>(smoothStep_);
    vco2Tune_ += (vco2TuneTarget_ - vco2Tune_) * smoothStep_;
    vcoFm_ += (vcoFmTarget_ - vcoFm_) * smoothStep_;
    cutoffOctaves_ += (cutoffOctavesTarget_ - cutoffOctaves_) * smoothStep_;
    resonance_ += (resonanceTarget_ - resonance_) * smoothStep_;
    drive_ += (driveTarget_ - drive_) * smoothStep_;
}

double Voice::midiToHz(int note) {
    return kA4Hz * std::pow(2.0, (static_cast<double>(note) - kA4Note) / kSemitonesPerOctave);
}

}  // namespace undertow
