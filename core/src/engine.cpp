#include "undertow/engine.h"
#include "undertow/output_stage.h"
#include "undertow/rumble.h"
#include "undertow/sequencer.h"
#include "undertow/voice.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>

namespace undertow {
namespace {

constexpr double kRmsTauSeconds = 0.05;
constexpr std::uint8_t kMidiNoteOff = 0x80;
constexpr std::uint8_t kMidiNoteOn = 0x90;
constexpr std::uint8_t kMidiController = 0xB0;
constexpr std::uint8_t kAllSoundOff = 120;

float clampParamValue(int index, float value) {
    const auto& spec = paramSpec(index);
    value = std::clamp(value, spec.min, spec.max);
    if (spec.kind == ParamKind::Int || spec.kind == ParamKind::Choice)
        value = std::round(value);
    else if (spec.kind == ParamKind::Bool)
        value = value >= 0.5f ? 1.0f : 0.0f;
    return value;
}

bool isNotesMode(float seqMode) {
    return static_cast<int>(seqMode) == 2;
}

}  // namespace

struct Engine::Impl {
    double sampleRate = 48000.0;
    int maxBlockSize = 0;
    float params[kNumParams]{};
    Voice voice;
    Rumble rumble;
    Sequencer sequencer;
    OutputStage output;
    double ownPpq = 0.0;
    float scope[kScopeSamples]{};
    int scopeWrite = 0;
    int scopeCount = 0;
    float rmsState = 0.0f;
    float rumbleRmsState = 0.0f;
    std::atomic<float> peak{0.0f};
    std::atomic<float> rms{0.0f};
    std::atomic<bool> clipping{false};
    std::atomic<float> rumbleMeter{0.0f};
    std::atomic<double> ppq{0.0};
    std::atomic<bool> hostClock{false};
    std::atomic<double> bpm{120.0};
    std::atomic<int> step{-1};
    std::atomic<long long> count{0};
    std::atomic<int> root{-1};
    std::array<Hit, kMaxHits> hits{};
    int hitCount = 0;

    Impl() {
        for (int i = 0; i < kNumParams; ++i)
            params[i] = paramSpec(i).def;
    }
};

Engine::Engine() : impl_(std::make_unique<Impl>()) {}
Engine::~Engine() = default;

void Engine::prepare(double sampleRate, int maxBlockSize) {
    impl_->sampleRate = std::max(sampleRate, 1.0);
    impl_->maxBlockSize = std::max(maxBlockSize, 0);
    impl_->voice.prepare(impl_->sampleRate);
    impl_->rumble.prepare(impl_->sampleRate);
    impl_->sequencer.prepare(impl_->sampleRate);
    impl_->output.prepare(impl_->sampleRate);
    reset();
}

void Engine::reset() {
    impl_->ownPpq = 0.0;
    impl_->scopeWrite = 0;
    impl_->scopeCount = 0;
    std::fill(std::begin(impl_->scope), std::end(impl_->scope), 0.0f);
    impl_->rmsState = 0.0f;
    impl_->rumbleRmsState = 0.0f;
    impl_->voice.reset();
    impl_->voice.setParams(impl_->params);
    impl_->sequencer.reset();
    impl_->sequencer.setParams(impl_->params);
    impl_->rumble.setParams(impl_->params);
    impl_->rumble.reset();
    impl_->output.setParams(impl_->params);
    impl_->output.reset();
    impl_->peak.store(0.0f, std::memory_order_relaxed);
    impl_->rms.store(0.0f, std::memory_order_relaxed);
    impl_->clipping.store(false, std::memory_order_relaxed);
    impl_->rumbleMeter.store(0.0f, std::memory_order_relaxed);
    impl_->ppq.store(0.0, std::memory_order_relaxed);
    impl_->hostClock.store(false, std::memory_order_relaxed);
    impl_->bpm.store(static_cast<double>(impl_->params[param::SeqBpm]), std::memory_order_relaxed);
    impl_->step.store(-1, std::memory_order_relaxed);
    impl_->count.store(0, std::memory_order_relaxed);
    impl_->root.store(-1, std::memory_order_relaxed);
    impl_->hitCount = 0;
}

void Engine::setParam(int index, float value) {
    if (index < 0 || index >= kNumParams)
        return;
    impl_->params[index] = clampParamValue(index, value);
}

float Engine::getParam(int index) const {
    if (index < 0 || index >= kNumParams)
        return 0.0f;
    return impl_->params[index];
}

void Engine::process(float* left, float* right, int numSamples,
                     const MidiEvent* events, int numEvents, const Transport& transport) {
    impl_->hitCount = 0;
    if (numSamples <= 0)
        return;

    impl_->voice.setParams(impl_->params);
    impl_->sequencer.setParams(impl_->params);
    impl_->rumble.setParams(impl_->params);
    impl_->output.setParams(impl_->params);

    const bool useHostClock = transport.valid;
    const double bpm = useHostClock ? transport.bpm : static_cast<double>(impl_->params[param::SeqBpm]);
    const double beatsPerSample = bpm / (60.0 * impl_->sampleRate);
    double ppq = useHostClock ? transport.ppq : impl_->ownPpq;
    float peak = 0.0f;
    double sumSquares = 0.0;
    double rumbleSumSquares = 0.0;
    int eventIndex = 0;
    const float rumbleSend = impl_->params[param::RumbleSend];
    const int seqMode = static_cast<int>(impl_->params[param::SeqMode]);

    auto recordHit = [&](int sampleOffset, int step, int note, float velocity) {
        if (impl_->hitCount >= kMaxHits)
            return;
        impl_->hits[static_cast<std::size_t>(impl_->hitCount++)] = Hit{sampleOffset, step, note, velocity};
    };

    for (int i = 0; i < numSamples; ++i) {
        while (eventIndex < numEvents && events[eventIndex].sampleOffset == i) {
            const MidiEvent& event = events[eventIndex++];
            const std::uint8_t status = static_cast<std::uint8_t>(event.status & 0xF0);
            if (status == kMidiController) {
                if (event.data1 == 123)
                    impl_->sequencer.allKeysOff();
                if (event.data1 == kAllSoundOff) {
                    impl_->sequencer.allKeysOff();
                    impl_->voice.reset();
                    impl_->rumble.reset();
                    impl_->output.reset();
                }
                continue;
            }
            if (status == kMidiNoteOn && event.data2 > 0) {
                const int note = static_cast<int>(event.data1);
                impl_->sequencer.keyOn(note);
                if (isNotesMode(impl_->params[param::SeqMode])) {
                    const float velocity = static_cast<float>(event.data2) / 127.0f;
                    impl_->voice.hit(note, velocity);
                    recordHit(i, -1, note, velocity);
                }
            } else if (status == kMidiNoteOff || (status == kMidiNoteOn && event.data2 == 0)) {
                impl_->sequencer.keyOff(static_cast<int>(event.data1));
            }
        }

        const bool useOwnClock = !useHostClock && (seqMode == 1 || impl_->sequencer.anyKeyHeld());
        const bool running = useHostClock ? transport.playing : useOwnClock;
        if (!useHostClock)
            ppq = running ? impl_->ownPpq : 0.0;

        SeqStep seqStep;
        if (impl_->sequencer.tick(ppq, running, bpm, seqStep)) {
            if (seqStep.velocity > 0.0f) {
                impl_->voice.hit(seqStep.note, seqStep.velocity);
                recordHit(i, seqStep.step, seqStep.note, seqStep.velocity);
            }
        }

        const float voiceOut = impl_->voice.process();
        const float rumbleOut = impl_->rumble.process(voiceOut * rumbleSend, ppq, bpm, running);
        const float mono = impl_->output.process(voiceOut + rumbleOut);
        left[i] = mono;
        right[i] = mono;

        impl_->scope[impl_->scopeWrite] = mono;
        impl_->scopeWrite = (impl_->scopeWrite + 1) % kScopeSamples;
        if (impl_->scopeCount < kScopeSamples)
            ++impl_->scopeCount;

        peak = std::max(peak, std::fabs(mono));
        sumSquares += static_cast<double>(mono) * static_cast<double>(mono);
        rumbleSumSquares += static_cast<double>(rumbleOut) * static_cast<double>(rumbleOut);
        if (!useHostClock)
            impl_->ownPpq = running ? ppq + beatsPerSample : 0.0;
        if (running)
            ppq += beatsPerSample;
    }

    if (!useHostClock)
        ppq = impl_->ownPpq;

    const double alpha = 1.0 - std::exp(-static_cast<double>(numSamples) / (impl_->sampleRate * kRmsTauSeconds));
    const float blockMeanSquare = static_cast<float>(sumSquares / static_cast<double>(numSamples));
    const float rumbleMeanSquare = static_cast<float>(rumbleSumSquares / static_cast<double>(numSamples));
    impl_->rmsState += static_cast<float>(alpha) * (blockMeanSquare - impl_->rmsState);
    impl_->rumbleRmsState += static_cast<float>(alpha) * (rumbleMeanSquare - impl_->rumbleRmsState);

    impl_->peak.store(peak, std::memory_order_relaxed);
    impl_->rms.store(std::sqrt(impl_->rmsState), std::memory_order_relaxed);
    impl_->clipping.store(impl_->output.clipped(), std::memory_order_relaxed);
    impl_->rumbleMeter.store(std::sqrt(impl_->rumbleRmsState), std::memory_order_relaxed);
    impl_->ppq.store(useHostClock ? (transport.playing ? ppq : transport.ppq) : impl_->ownPpq, std::memory_order_relaxed);
    impl_->hostClock.store(useHostClock, std::memory_order_relaxed);
    impl_->bpm.store(bpm, std::memory_order_relaxed);
    impl_->step.store(impl_->sequencer.step(), std::memory_order_relaxed);
    impl_->count.store(impl_->sequencer.count(), std::memory_order_relaxed);
    impl_->root.store(impl_->sequencer.root(), std::memory_order_relaxed);
}

int Engine::hits(Hit* destination, int count) const {
    if (destination == nullptr || count <= 0 || impl_->hitCount <= 0)
        return 0;
    const int toCopy = std::min(count, impl_->hitCount);
    std::copy_n(impl_->hits.data(), toCopy, destination);
    return toCopy;
}

Meters Engine::meters() const {
    Meters m;
    m.peak = impl_->peak.load(std::memory_order_relaxed);
    m.rms = impl_->rms.load(std::memory_order_relaxed);
    m.clipping = impl_->clipping.load(std::memory_order_relaxed);
    m.note = impl_->voice.note();
    m.noteHz = static_cast<float>(impl_->voice.noteHz());
    m.step = impl_->step.load(std::memory_order_relaxed);
    m.count = impl_->count.load(std::memory_order_relaxed);
    m.root = impl_->root.load(std::memory_order_relaxed);
    m.rumble = impl_->rumbleMeter.load(std::memory_order_relaxed);
    m.ppq = impl_->ppq.load(std::memory_order_relaxed);
    m.hostClock = impl_->hostClock.load(std::memory_order_relaxed);
    m.bpm = impl_->bpm.load(std::memory_order_relaxed);
    return m;
}

int Engine::copyScope(float* destination, int count) const {
    if (destination == nullptr || count <= 0 || impl_->scopeCount <= 0)
        return 0;
    const int toCopy = std::min(count, impl_->scopeCount);
    const int start = (impl_->scopeWrite - toCopy + kScopeSamples) % kScopeSamples;
    const int first = std::min(toCopy, kScopeSamples - start);
    std::memcpy(destination, impl_->scope + start, static_cast<std::size_t>(first) * sizeof(float));
    if (first < toCopy) {
        std::memcpy(destination + first, impl_->scope, static_cast<std::size_t>(toCopy - first) * sizeof(float));
    }
    return toCopy;
}

}  // namespace undertow
