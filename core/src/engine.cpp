#include "undertow/engine.h"
#include "undertow/voice.h"
#include "undertow/lfo.h"
#include "undertow/rumble.h"
#include "undertow/output_stage.h"

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
    float rumbleRmsSmooth = 0.0f;
    MonoVoice voice;
    Lfo lfo1;
    Lfo lfo2;
    Rumble rumble;
    OutputStage output;

    std::atomic<float> peak{0.0f};
    std::atomic<float> rms{0.0f};
    std::atomic<double> meterPpq{0.0};
    std::atomic<int> meterNote{-1};
    std::atomic<float> meterNoteHz{0.0f};
    std::atomic<float> meterRumble{0.0f};
    std::atomic<bool> meterClipping{false};

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
    impl_->rumble.setParams(impl_->params);
    impl_->rumble.reset();
    impl_->output.setParams(impl_->params);
    impl_->output.reset();
    impl_->meterNote.store(-1, std::memory_order_relaxed);
    impl_->meterNoteHz.store(0.0f, std::memory_order_relaxed);
    impl_->meterRumble.store(0.0f, std::memory_order_relaxed);
    impl_->meterClipping.store(false, std::memory_order_relaxed);
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
    if (numSamples <= 0)
        return;

    const auto& p = impl_->params;

    // Apply parameters to voice, rumble, output stage
    impl_->voice.setParams(p);
    impl_->rumble.setParams(p);
    impl_->output.setParams(p);

    // Configure LFOs (stateless — read params once per block)
    impl_->lfo1.setRate(static_cast<int>(p[param::Lfo1Rate]));
    impl_->lfo1.setShape(static_cast<LfoShape>(static_cast<int>(p[param::Lfo1Shape])));
    impl_->lfo1.setPhaseOffset(static_cast<double>(p[param::Lfo1Phase]));
    impl_->lfo1.setSeed(static_cast<std::uint32_t>(2 * static_cast<int>(p[param::SeqSeed]) + 1));

    impl_->lfo2.setRate(static_cast<int>(p[param::Lfo2Rate]));
    impl_->lfo2.setShape(static_cast<LfoShape>(static_cast<int>(p[param::Lfo2Shape])));
    impl_->lfo2.setPhaseOffset(static_cast<double>(p[param::Lfo2Phase]));
    impl_->lfo2.setSeed(static_cast<std::uint32_t>(2 * static_cast<int>(p[param::SeqSeed]) + 2));

    // Transport-derived values
    const bool running = transport.valid ? transport.playing : true;
    const double tempo = transport.valid ? transport.bpm : static_cast<double>(p[param::SeqBpm]);
    const double beatsPerSample = tempo / (60.0 * impl_->sampleRate);

    // Starting ppq for this block
    double ppq;
    if (transport.valid) {
        ppq = transport.ppq;
    } else {
        ppq = impl_->internalPpq;
    }

    // LFO depth / destination (constant for the block)
    const double lfo1Cutoff = static_cast<double>(p[param::Lfo1Cutoff]);
    const double lfo2Depth = static_cast<double>(p[param::Lfo2Depth]);
    const int lfo2Dest = static_cast<int>(p[param::Lfo2Dest]);
    const double rumbleSendBase = static_cast<double>(p[param::RumbleSend]);

    // Process sample by sample
    int eventIdx = 0;
    float blockPeak = 0.0f;
    double blockSumSq = 0.0;
    double rumbleSumSq = 0.0;

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
                    impl_->voice.setParams(p);
                    impl_->rumble.reset();
                    impl_->output.reset();
                }
            }
            ++eventIdx;
        }

        // Compute LFO values at current beat position
        const double lfo1Val = impl_->lfo1.valueAt(ppq);
        const double lfo2Val = impl_->lfo2.valueAt(ppq);

        // Build voice modulation from LFOs
        VoiceModulation mod;
        mod.cutoffOctaves = lfo1Cutoff * lfo1Val;

        // LFO 2 destination
        switch (lfo2Dest) {
            case 0: // Cutoff: 4 * depth * value octaves
                mod.cutoffOctaves += 4.0 * lfo2Depth * lfo2Val;
                break;
            case 1: // Resonance: 0.5 * depth * value
                mod.resonance = 0.5 * lfo2Depth * lfo2Val;
                break;
            case 2: // Pulse Width: 0.45 * depth * value
                mod.pulseWidth = 0.45 * lfo2Depth * lfo2Val;
                break;
            case 3: // Rumble Send (handled below)
                break;
            case 4: // Pitch: 12 * depth * value semitones
                mod.pitchSemitones = 12.0 * lfo2Depth * lfo2Val;
                break;
            case 5: // Amp: gain = 1 - depth + depth * (value + 1) / 2
                mod.gain = 1.0 - lfo2Depth + lfo2Depth * (lfo2Val + 1.0) / 2.0;
                break;
        }

        impl_->voice.setModulation(mod);

        // Render voice
        const float voiceSample = impl_->voice.process();

        // Rumble send (with LFO 2 modulation if dest == Rumble Send)
        double rumbleSend = rumbleSendBase;
        if (lfo2Dest == 3) {
            rumbleSend += lfo2Depth * lfo2Val;
            rumbleSend = std::clamp(rumbleSend, 0.0, 1.0);
        }
        const float rumbleIn = voiceSample * static_cast<float>(rumbleSend);

        // Rumble
        const float rumbleOut = impl_->rumble.process(rumbleIn, ppq, tempo, running);

        // Mono mix
        const float mono = voiceSample + rumbleOut;

        // Output stage (DC blocker → saturation → master gain → crossover → width → soft clip)
        float l = 0.0f, r = 0.0f;
        impl_->output.process(mono, l, r);

        left[i] = l;
        right[i] = r;

        // Accumulate meters
        const float absMax = std::max(std::fabs(l), std::fabs(r));
        if (absMax > blockPeak) blockPeak = absMax;
        blockSumSq += 0.5 * (static_cast<double>(l) * static_cast<double>(l)
                           + static_cast<double>(r) * static_cast<double>(r));
        rumbleSumSq += static_cast<double>(rumbleOut) * static_cast<double>(rumbleOut);

        // Advance beat position
        if (running) {
            ppq += beatsPerSample;
        }
    }

    // Update internal clock
    if (!transport.valid) {
        impl_->internalPpq = ppq;
    }

    // Meters
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

    // Voice meters
    impl_->meterNote.store(impl_->voice.isActive() ? impl_->voice.currentNote() : -1,
                           std::memory_order_relaxed);
    impl_->meterNoteHz.store(impl_->voice.isActive() ? static_cast<float>(impl_->voice.currentHz()) : 0.0f,
                             std::memory_order_relaxed);

    // Beat clock
    if (transport.valid) {
        if (transport.playing) {
            impl_->meterPpq.store(ppq, std::memory_order_relaxed);
        } else {
            impl_->meterPpq.store(transport.ppq, std::memory_order_relaxed);
        }
    } else {
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
    m.clipping = impl_->meterClipping.load(std::memory_order_relaxed);
    m.rumble = impl_->meterRumble.load(std::memory_order_relaxed);
    return m;
}

}  // namespace undertow
