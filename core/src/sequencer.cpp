#include "undertow/sequencer.h"

#include <algorithm>
#include <cmath>

namespace undertow {
namespace {

constexpr double kBeatEpsilon = 1e-9;

}  // namespace

long long Sequencer::floorDiv(long long a, long long b) {
    return a >= 0 ? a / b : -((-a + b - 1) / b);
}

long long Sequencer::countFromBeat(double beat, double stepBeats) {
    return static_cast<long long>(std::floor(beat / stepBeats + kBeatEpsilon));
}

int Sequencer::positiveMod(long long count, int length) {
    return static_cast<int>(count - floorDiv(count, length) * length);
}

void Sequencer::prepare(double sampleRate) {
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 1.0;
}

void Sequencer::reset() {
    heldCount_ = 0;
    hasLastPressed_ = false;
    lastPressed_ = 0;
    playing_ = false;
    source_ = Source::None;
    count_ = 0;
    step_ = -1;
    root_ = -1;
    localBeat_ = 0.0;
    hostBeat_ = 0.0;
    haveHostBeat_ = false;
}

void Sequencer::setParams(const float* values) {
    for (int i = 0; i < kNumParams; ++i)
        params_[i] = values[i];
    pattern_.setParams(values);
}

void Sequencer::keyOn(int note) {
    const int heldIndex = [&]() -> int {
        for (int i = 0; i < heldCount_; ++i) {
            if (held_[static_cast<std::size_t>(i)] == note)
                return i;
        }
        return -1;
    }();

    if (heldIndex >= 0) {
        for (int i = heldIndex; i + 1 < heldCount_; ++i)
            held_[static_cast<std::size_t>(i)] = held_[static_cast<std::size_t>(i + 1)];
        held_[static_cast<std::size_t>(heldCount_ - 1)] = note;
    } else if (heldCount_ < static_cast<int>(held_.size())) {
        held_[static_cast<std::size_t>(heldCount_++)] = note;
    } else {
        for (int i = 0; i + 1 < heldCount_; ++i)
            held_[static_cast<std::size_t>(i)] = held_[static_cast<std::size_t>(i + 1)];
        held_[static_cast<std::size_t>(heldCount_ - 1)] = note;
    }

    hasLastPressed_ = true;
    lastPressed_ = note;
}

void Sequencer::keyOff(int note) {
    for (int i = 0; i < heldCount_; ++i) {
        if (held_[static_cast<std::size_t>(i)] != note)
            continue;
        for (int j = i; j + 1 < heldCount_; ++j)
            held_[static_cast<std::size_t>(j)] = held_[static_cast<std::size_t>(j + 1)];
        --heldCount_;
        return;
    }
}

void Sequencer::allKeysOff() {
    heldCount_ = 0;
}

int Sequencer::latestHeld() const {
    return heldCount_ > 0 ? held_[static_cast<std::size_t>(heldCount_ - 1)] : -1;
}

int Sequencer::computeRoot(int mode) const {
    switch (mode) {
        case 0:
            return latestHeld();
        case 1:
            return hasLastPressed_ ? lastPressed_ : static_cast<int>(params_[param::SeqRoot]);
        default:
            return -1;
    }
}

long long Sequencer::computeCount(Source source, double beat, double stepBeats) const {
    if (source == Source::Host)
        return countFromBeat(beat, stepBeats);
    return countFromBeat(localBeat_, stepBeats);
}

void Sequencer::startStep(long long count, int root, SeqStep& out) {
    const int length = pattern_.length();
    const Step step = pattern_.at(count, root);
    step_ = positiveMod(count, length);
    count_ = count;
    root_ = root;
    out.step = step_;
    out.note = std::clamp(root + step.pitch, 0, 127);
    out.velocity = step.velocity;
    playing_ = true;
}

bool Sequencer::tick(double beat, bool running, double bpm, SeqStep& out) {
    const int mode = static_cast<int>(params_[param::SeqMode]);
    const double stepBeats = [&]() {
        switch (static_cast<int>(params_[param::SeqRate])) {
            case 0: return 4.0;
            case 1: return 2.0;
            case 2: return 1.0;
            case 3: return 0.5;
            case 4: return 0.25;
            case 5: return 0.125;
            case 6: return 1.0 / 3.0;
            default: return 1.0 / 6.0;
        }
    }();

    const bool keyHeld = heldCount_ > 0;
    const bool desiredPlay = mode == 0 ? keyHeld : mode == 1 ? (running || keyHeld) : false;
    const Source desiredSource = !desiredPlay ? Source::None : running ? Source::Host : Source::Local;

    if (desiredSource == Source::None) {
        playing_ = false;
        source_ = Source::None;
        count_ = 0;
        step_ = -1;
        root_ = -1;
        localBeat_ = 0.0;
        return false;
    }

    if (desiredSource == Source::Local && source_ != Source::Local)
        localBeat_ = 0.0;

    const long long desiredCount = computeCount(desiredSource, beat, stepBeats);
    const int desiredRoot = computeRoot(mode);
    const bool hostLooped = desiredSource == Source::Host && haveHostBeat_ && beat < hostBeat_ - kBeatEpsilon;
    const bool startNow = !playing_ || source_ != desiredSource || desiredCount != count_ || hostLooped;
    if (startNow) {
        source_ = desiredSource;
        startStep(desiredCount, desiredRoot, out);
    }

    if (source_ == Source::Local)
        localBeat_ += bpm / (60.0 * sampleRate_);
    else
        localBeat_ = 0.0;

    if (source_ == Source::Host) {
        hostBeat_ = beat;
        haveHostBeat_ = true;
    }

    return startNow;
}

int Sequencer::step() const {
    return playing_ ? step_ : -1;
}

long long Sequencer::count() const {
    return playing_ ? count_ : 0;
}

int Sequencer::root() const {
    return playing_ ? root_ : -1;
}

}  // namespace undertow
