#include "undertow/engine.h"
#include "undertow/voice.h"

#include <algorithm>
#include <atomic>
#include <cmath>

namespace undertow {

struct Engine::Impl {
    double sampleRate = 48000.0;
    int maxBlockSize = 512;
    float params[kNumParams];
    double internalPpq = 0.0;
    float rmsSmooth = 0.0f;
    MonoVoice voice;

    std::atomic<float> peak{0.0f};
    std::atomic<float> rms{0.0f};
    std::atomic<double> meterPpq{0.0};
    std::atomic<int> meterNote{-1};
    std::atomic<float> meterNoteHz{0.0f};

    Impl() {
        for (int i = 0; i < kNumParams; ++i)
            params[i] = paramSpec(i).def;
    }
};

Engine::Engine() : impl_(std::make_unique<Impl>()) {}

Engine::~Engine() = default;

void Engine::prepare(double sampleRate, int maxBlockSize) {
    impl_->sampleRate = sampleRate;
    impl_->maxBlockSize = maxBlockSize;
    impl_->voice.prepare(sampleRate);
    reset();
}

void Engine::reset() {
    impl_->internalPpq = 0.0;
    impl_->rmsSmooth = 0.0f;
    impl_->peak.store(0.0f, std::memory_order_relaxed);
    impl_->rms.store(0.0f, std::memory_order_relaxed);
    impl_->meterPpq.store(0.0, std::memory_order_relaxed);
    impl_->voice.reset();
    impl_->voice.setParams(impl_->params);
    impl_->meterNote.store(-1, std::memory_order_relaxed);
    impl_->meterNoteHz.store(0.0f, std::memory_order_relaxed);
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

void Engine::process(float* left, float* right, int numSamples,
                     const MidiEvent* events, int numEvents,
                     const Transport& transport) {
    // Apply parameters to voice
    impl_->voice.setParams(impl_->params);

    // Master gain (dB to linear) — constant for the whole block
    const double masterDb = static_cast<double>(impl_->params[param::MasterGain]);
    const double masterLin = std::pow(10.0, masterDb / 20.0);

    // Process sample by sample, applying MIDI events at their offsets
    int eventIdx = 0;
    float blockPeak = 0.0f;
    double blockSumSq = 0.0;

    for (int i = 0; i < numSamples; ++i) {
        // Apply MIDI events at this sample
        while (eventIdx < numEvents && events[eventIdx].sampleOffset == i) {
            const auto& ev = events[eventIdx];
            const int status = ev.status & 0xF0;

            if (status == 0x90 && ev.data2 > 0) {
                // Note on (any channel)
                impl_->voice.noteOn(static_cast<int>(ev.data1),
                                    static_cast<float>(ev.data2) / 127.0f);
            } else if (status == 0x80 || (status == 0x90 && ev.data2 == 0)) {
                // Note off (any channel, including velocity-0 note-on)
                impl_->voice.noteOff(static_cast<int>(ev.data1));
            } else if (status == 0xE0) {
                // Pitch bend (any channel): 14-bit value, centre 8192
                const int value = (static_cast<int>(ev.data2) << 7) | static_cast<int>(ev.data1);
                const float bend = static_cast<float>(value - 8192) / 8192.0f;
                impl_->voice.setPitchBend(bend);
            } else if (status == 0xB0) {
                // Controller (any channel)
                if (ev.data1 == 123) {
                    // All notes off
                    impl_->voice.allNotesOff();
                } else if (ev.data1 == 120) {
                    // All sound off — silence at once
                    impl_->voice.reset();
                    impl_->voice.setParams(impl_->params);
                }
            }
            ++eventIdx;
        }

        // Render voice
        const float sample = impl_->voice.process();
        const float out = static_cast<float>(sample * masterLin);

        left[i] = out;
        right[i] = out;

        const float absOut = std::fabs(out);
        if (absOut > blockPeak) blockPeak = absOut;
        blockSumSq += static_cast<double>(out) * static_cast<double>(out);
    }

    // Meters
    double blockMeanSq = blockSumSq / static_cast<double>(numSamples);
    double blockDur = static_cast<double>(numSamples) / impl_->sampleRate;
    double alpha = 1.0 - std::exp(-blockDur / 0.05);
    impl_->rmsSmooth = static_cast<float>(alpha * blockMeanSq + (1.0 - alpha) * impl_->rmsSmooth);

    impl_->peak.store(blockPeak, std::memory_order_relaxed);
    impl_->rms.store(std::sqrt(impl_->rmsSmooth), std::memory_order_relaxed);

    // Voice meters
    impl_->meterNote.store(impl_->voice.isActive() ? impl_->voice.currentNote() : -1,
                           std::memory_order_relaxed);
    impl_->meterNoteHz.store(impl_->voice.isActive() ? static_cast<float>(impl_->voice.currentHz()) : 0.0f,
                             std::memory_order_relaxed);

    // Beat clock (PRD 3.4)
    if (transport.valid) {
        if (transport.playing) {
            double ppqEnd = transport.ppq
                + static_cast<double>(numSamples) / impl_->sampleRate * transport.bpm / 60.0;
            impl_->meterPpq.store(ppqEnd, std::memory_order_relaxed);
        } else {
            impl_->meterPpq.store(transport.ppq, std::memory_order_relaxed);
        }
    } else {
        double seqBpm = static_cast<double>(impl_->params[param::SeqBpm]);
        impl_->internalPpq += static_cast<double>(numSamples) / impl_->sampleRate * seqBpm / 60.0;
        impl_->meterPpq.store(impl_->internalPpq, std::memory_order_relaxed);
    }
}

Meters Engine::meters() const {
    Meters m;
    m.peak = impl_->peak.load(std::memory_order_relaxed);
    m.rms = impl_->rms.load(std::memory_order_relaxed);
    m.ppq = impl_->meterPpq.load(std::memory_order_relaxed);
    m.note = impl_->meterNote.load(std::memory_order_relaxed);
    m.noteHz = impl_->meterNoteHz.load(std::memory_order_relaxed);
    return m;
}

}  // namespace undertow
