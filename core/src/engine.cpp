#include "undertow/engine.h"
#include "undertow/voice.h"
#include "undertow/lfo.h"
#include "undertow/rumble.h"
#include "undertow/output_stage.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>

namespace undertow {

static void seqlockWrite(Pattern& slot, std::atomic<int>& version, const Pattern& p) {
    int v = version.load(std::memory_order_relaxed);
    v = v - (v & 1);
    version.store(v + 1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    slot = p;
    std::atomic_thread_fence(std::memory_order_release);
    version.store(v + 2, std::memory_order_release);
}

static Pattern seqlockRead(const Pattern& slot, const std::atomic<int>& version) {
    for (;;) {
        int v1 = version.load(std::memory_order_acquire);
        if (v1 & 1)
            continue;
        Pattern p = slot;
        std::atomic_thread_fence(std::memory_order_acquire);
        int v2 = version.load(std::memory_order_acquire);
        if (v1 == v2)
            return p;
    }
}

struct Engine::Impl {
    double sampleRate = 48000.0;
    int maxBlockSize = 512;
    float params[kNumParams];
    double internalPpq = 0.0;
    float rmsSmooth = 0.0f;
    float rumbleRmsSmooth = 0.0f;
    MonoVoice voice;
    Lfo lfo1;
    Lfo lfo2;
    Rumble rumble;
    OutputStage output;
    Sequencer sequencer;
    bool prevSeqOn = false;
    int seqTranspose = 0;
    Pattern pendingPattern;
    std::atomic<int> pendingVersion{0};
    Pattern activePattern;
    std::atomic<int> activeVersion{0};
    std::atomic<int> consumedPending{0};
    float scope[kScopeSamples]{};
    std::atomic<int> scopeWrite{0};
    std::atomic<int> scopeCount{0};
    std::atomic<float> peak{0.0f};
    std::atomic<float> rms{0.0f};
    std::atomic<double> meterPpq{0.0};
    std::atomic<int> meterNote{-1};
    std::atomic<float> meterNoteHz{0.0f};
    std::atomic<float> meterRumble{0.0f};
    std::atomic<bool> meterClipping{false};
    std::atomic<int> meterStep{-1};
    std::atomic<bool> meterHostClock{false};
    std::atomic<double> meterBpm{120.0};

    Impl() {
        for (int i = 0; i < kNumParams; ++i)
            params[i] = paramSpec(i).def;
        sequencer.setPattern(defaultPattern());
        activePattern = defaultPattern();
    }
};

Engine::Engine() : impl_(std::make_unique<Impl>()) {}
Engine::~Engine() = default;

void Engine::prepare(double sampleRate, int maxBlockSize) {
    impl_->sampleRate = sampleRate;
    impl_->maxBlockSize = maxBlockSize;
    impl_->voice.prepare(sampleRate);
    impl_->sequencer.prepare(sampleRate);
    impl_->rumble.prepare(sampleRate);
    impl_->output.prepare(sampleRate);
    reset();
}

void Engine::reset() {
    impl_->internalPpq = 0.0;
    impl_->rmsSmooth = 0.0f;
    impl_->rumbleRmsSmooth = 0.0f;
    impl_->peak.store(0.0f, std::memory_order_relaxed);
    impl_->rms.store(0.0f, std::memory_order_relaxed);
    impl_->meterPpq.store(0.0, std::memory_order_relaxed);
    impl_->voice.reset();
    impl_->voice.setParams(impl_->params);
    impl_->sequencer.reset();
    impl_->rumble.setParams(impl_->params);
    impl_->rumble.reset();
    impl_->output.setParams(impl_->params);
    impl_->output.reset();
    impl_->meterNote.store(-1, std::memory_order_relaxed);
    impl_->meterNoteHz.store(0.0f, std::memory_order_relaxed);
    impl_->meterRumble.store(0.0f, std::memory_order_relaxed);
    impl_->meterClipping.store(false, std::memory_order_relaxed);
    impl_->meterStep.store(-1, std::memory_order_relaxed);
    impl_->prevSeqOn = false;
    impl_->seqTranspose = 0;
}

void Engine::setParam(int index, float value) {
    if (index < 0 || index >= kNumParams)
        return;
    const auto& spec = paramSpec(index);
    value = std::clamp(value, spec.min, spec.max);
    if (spec.kind == ParamKind::Int || spec.kind == ParamKind::Choice)
        value = std::round(value);
    if (spec.kind == ParamKind::Bool)
        value = (value >= 0.5f) ? 1.0f : 0.0f;
    impl_->params[index] = value;
}

float Engine::getParam(int index) const {
    if (index < 0 || index >= kNumParams)
        return 0.0f;
    return impl_->params[index];
}

void Engine::setPattern(const Pattern& pattern) { seqlockWrite(impl_->pendingPattern, impl_->pendingVersion, pattern); }
Pattern Engine::pattern() const {
    int pv = impl_->pendingVersion.load(std::memory_order_acquire);
    int cv = impl_->consumedPending.load(std::memory_order_acquire);
    if (pv > cv)
        return seqlockRead(impl_->pendingPattern, impl_->pendingVersion);
    return seqlockRead(impl_->activePattern, impl_->activeVersion);
}

int Engine::copyScope(float* destination, int count) const {
    if (destination == nullptr || count <= 0)
        return 0;
    const int available = std::min(impl_->scopeCount.load(std::memory_order_acquire), kScopeSamples);
    const int toCopy = std::min(count, available);
    if (toCopy <= 0)
        return 0;
    const int write = impl_->scopeWrite.load(std::memory_order_acquire);
    const int start = (write - toCopy + kScopeSamples) % kScopeSamples;
    const int first = std::min(toCopy, kScopeSamples - start);
    std::memcpy(destination, impl_->scope + start, static_cast<std::size_t>(first) * sizeof(float));
    if (first < toCopy)
        std::memcpy(destination + first, impl_->scope, static_cast<std::size_t>(toCopy - first) * sizeof(float));
    return toCopy;
}

void Engine::process(float* left, float* right, int numSamples, const MidiEvent* events, int numEvents, const Transport& transport) {
    if (numSamples <= 0)
        return;
    const auto& p = impl_->params;
    int pv = impl_->pendingVersion.load(std::memory_order_acquire);
    int cv = impl_->consumedPending.load(std::memory_order_relaxed);
    if (pv > cv && !(pv & 1)) {
        impl_->sequencer.setPattern(seqlockRead(impl_->pendingPattern, impl_->pendingVersion));
        impl_->consumedPending.store(pv, std::memory_order_release);
    }
    const int seqSync = static_cast<int>(p[param::SeqSync]);
    const bool useHostClock = (seqSync == 0) && transport.valid;
    const bool running = useHostClock ? transport.playing : true;
    const double tempo = useHostClock ? transport.bpm : static_cast<double>(p[param::SeqBpm]);
    impl_->meterHostClock.store(useHostClock, std::memory_order_relaxed);
    impl_->meterBpm.store(tempo, std::memory_order_relaxed);
    const double beatsPerSample = tempo / (60.0 * impl_->sampleRate);
    double ppq = useHostClock ? transport.ppq : impl_->internalPpq;
    const bool seqOn = p[param::SeqOn] >= 0.5f;
    if (!seqOn && impl_->prevSeqOn) impl_->voice.allNotesOff();
    impl_->prevSeqOn = seqOn;
    impl_->voice.setParams(p);
    impl_->rumble.setParams(p);
    impl_->output.setParams(p);
    impl_->lfo1.setRate(static_cast<int>(p[param::Lfo1Rate]));
    impl_->lfo1.setShape(static_cast<LfoShape>(static_cast<int>(p[param::Lfo1Shape])));
    impl_->lfo1.setPhaseOffset(static_cast<double>(p[param::Lfo1Phase]));
    impl_->lfo1.setSeed(static_cast<std::uint32_t>(2 * static_cast<int>(p[param::SeqSeed]) + 1));
    impl_->lfo2.setRate(static_cast<int>(p[param::Lfo2Rate]));
    impl_->lfo2.setShape(static_cast<LfoShape>(static_cast<int>(p[param::Lfo2Shape])));
    impl_->lfo2.setPhaseOffset(static_cast<double>(p[param::Lfo2Phase]));
    impl_->lfo2.setSeed(static_cast<std::uint32_t>(2 * static_cast<int>(p[param::SeqSeed]) + 2));
    SeqEvent seqEvents[64]; int numSeqEvents = 0;
    if (seqOn) {
        for (int i = 0; i < numEvents; ++i) {
            const auto& ev = events[i];
            if ((ev.status & 0xF0) == 0x90 && ev.data2 > 0)
                impl_->seqTranspose = static_cast<int>(ev.data1) - static_cast<int>(p[param::SeqRoot]);
        }
        impl_->sequencer.setParams(p);
        impl_->sequencer.setTranspose(impl_->seqTranspose);
        numSeqEvents = impl_->sequencer.process(ppq, tempo, running, numSamples, seqEvents, 64);
    }
    const double lfo1Cutoff = static_cast<double>(p[param::Lfo1Cutoff]);
    const double lfo2Depth = static_cast<double>(p[param::Lfo2Depth]);
    const int lfo2Dest = static_cast<int>(p[param::Lfo2Dest]);
    const double rumbleSendBase = static_cast<double>(p[param::RumbleSend]);
    int midiIdx = 0, seqIdx = 0;
    float blockPeak = 0.0f;
    double blockSumSq = 0.0, rumbleSumSq = 0.0;
    for (int i = 0; i < numSamples; ++i) {
        while (midiIdx < numEvents && events[midiIdx].sampleOffset == i) {
            const auto& ev = events[midiIdx];
            const int status = ev.status & 0xF0;
            if (seqOn) {
                if (status == 0xE0) {
                    const int value = (static_cast<int>(ev.data2) << 7) | static_cast<int>(ev.data1);
                    const float bend = static_cast<float>(value - 8192) / 8192.0f;
                    impl_->voice.setPitchBend(bend);
                } else if (status == 0xB0) {
                    if (ev.data1 == 123) impl_->voice.allNotesOff();
                    else if (ev.data1 == 120) { impl_->voice.reset(); impl_->voice.setParams(p); impl_->rumble.reset(); impl_->output.reset(); }
                }
            } else {
                if (status == 0x90 && ev.data2 > 0) impl_->voice.noteOn(static_cast<int>(ev.data1), static_cast<float>(ev.data2) / 127.0f);
                else if (status == 0x80 || (status == 0x90 && ev.data2 == 0)) impl_->voice.noteOff(static_cast<int>(ev.data1));
                else if (status == 0xE0) {
                    const int value = (static_cast<int>(ev.data2) << 7) | static_cast<int>(ev.data1);
                    const float bend = static_cast<float>(value - 8192) / 8192.0f;
                    impl_->voice.setPitchBend(bend);
                } else if (status == 0xB0) {
                    if (ev.data1 == 123) impl_->voice.allNotesOff();
                    else if (ev.data1 == 120) { impl_->voice.reset(); impl_->voice.setParams(p); impl_->rumble.reset(); impl_->output.reset(); }
                }
            }
            ++midiIdx;
        }
        while (seqIdx < numSeqEvents && seqEvents[seqIdx].sampleOffset == i) {
            if (seqEvents[seqIdx].type == SeqEvent::Type::NoteOn) impl_->voice.noteOn(seqEvents[seqIdx].note, seqEvents[seqIdx].accent ? 1.0f : (90.0f / 127.0f));
            else impl_->voice.noteOff(seqEvents[seqIdx].note);
            ++seqIdx;
        }
        const double lfo1Val = impl_->lfo1.valueAt(ppq);
        const double lfo2Val = impl_->lfo2.valueAt(ppq);
        VoiceModulation mod; mod.cutoffOctaves = lfo1Cutoff * lfo1Val;
        switch (lfo2Dest) {
            case 0: mod.cutoffOctaves += 4.0 * lfo2Depth * lfo2Val; break;
            case 1: mod.resonance = 0.5 * lfo2Depth * lfo2Val; break;
            case 2: mod.pulseWidth = 0.45 * lfo2Depth * lfo2Val; break;
            case 4: mod.pitchSemitones = 12.0 * lfo2Depth * lfo2Val; break;
            case 5: mod.gain = 1.0 - lfo2Depth + lfo2Depth * (lfo2Val + 1.0) / 2.0; break;
            default: break;
        }
        impl_->voice.setModulation(mod);
        const float voiceSample = impl_->voice.process();
        double rumbleSend = rumbleSendBase;
        if (lfo2Dest == 3) { rumbleSend = std::clamp(rumbleSend + lfo2Depth * lfo2Val, 0.0, 1.0); }
        const float rumbleIn = voiceSample * static_cast<float>(rumbleSend);
        const float rumbleOut = impl_->rumble.process(rumbleIn, ppq, tempo, running);
        const float mono = voiceSample + rumbleOut;
        float l = 0.0f, r = 0.0f;
        impl_->output.process(mono, l, r);
        left[i] = l; right[i] = r;
        int write = impl_->scopeWrite.load(std::memory_order_relaxed);
        impl_->scope[static_cast<std::size_t>(write)] = 0.5f * (l + r);
        write = (write + 1) % kScopeSamples;
        impl_->scopeWrite.store(write, std::memory_order_release);
        int count = impl_->scopeCount.load(std::memory_order_relaxed);
        if (count < kScopeSamples) impl_->scopeCount.store(count + 1, std::memory_order_release);
        const float absMax = std::max(std::fabs(l), std::fabs(r));
        if (absMax > blockPeak) blockPeak = absMax;
        blockSumSq += 0.5 * (static_cast<double>(l) * static_cast<double>(l) + static_cast<double>(r) * static_cast<double>(r));
        rumbleSumSq += static_cast<double>(rumbleOut) * static_cast<double>(rumbleOut);
        if (running) ppq += beatsPerSample;
    }
    if (!useHostClock) impl_->internalPpq = ppq;
    const double blockMeanSq = blockSumSq / static_cast<double>(numSamples);
    const double blockDur = static_cast<double>(numSamples) / impl_->sampleRate;
    const double alpha = 1.0 - std::exp(-blockDur / 0.05);
    impl_->rmsSmooth = static_cast<float>(alpha * blockMeanSq + (1.0 - alpha) * impl_->rmsSmooth);
    const double rumbleMeanSq = rumbleSumSq / static_cast<double>(numSamples);
    impl_->rumbleRmsSmooth = static_cast<float>(alpha * rumbleMeanSq + (1.0 - alpha) * impl_->rumbleRmsSmooth);
    impl_->peak.store(blockPeak, std::memory_order_relaxed);
    impl_->rms.store(std::sqrt(impl_->rmsSmooth), std::memory_order_relaxed);
    impl_->meterRumble.store(std::sqrt(impl_->rumbleRmsSmooth), std::memory_order_relaxed);
    impl_->meterClipping.store(impl_->output.clipped(), std::memory_order_relaxed);
    impl_->meterNote.store(impl_->voice.isActive() ? impl_->voice.currentNote() : -1, std::memory_order_relaxed);
    impl_->meterNoteHz.store(impl_->voice.isActive() ? static_cast<float>(impl_->voice.currentHz()) : 0.0f, std::memory_order_relaxed);
    impl_->meterStep.store(seqOn ? impl_->sequencer.currentStep() : -1, std::memory_order_relaxed);
    impl_->meterPpq.store(useHostClock ? (transport.playing ? ppq : transport.ppq) : impl_->internalPpq, std::memory_order_relaxed);
    seqlockWrite(impl_->activePattern, impl_->activeVersion, impl_->sequencer.pattern());
}

Meters Engine::meters() const {
    Meters m;
    m.peak = impl_->peak.load(std::memory_order_relaxed);
    m.rms = impl_->rms.load(std::memory_order_relaxed);
    m.ppq = impl_->meterPpq.load(std::memory_order_relaxed);
    m.note = impl_->meterNote.load(std::memory_order_relaxed);
    m.noteHz = impl_->meterNoteHz.load(std::memory_order_relaxed);
    m.clipping = impl_->meterClipping.load(std::memory_order_relaxed);
    m.rumble = impl_->meterRumble.load(std::memory_order_relaxed);
    m.step = impl_->meterStep.load(std::memory_order_relaxed);
    m.hostClock = impl_->meterHostClock.load(std::memory_order_relaxed);
    m.bpm = impl_->meterBpm.load(std::memory_order_relaxed);
    return m;
}

}  // namespace undertow
