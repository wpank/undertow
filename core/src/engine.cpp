#include "undertow/engine.h"

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

    std::atomic<float> peak{0.0f};
    std::atomic<float> rms{0.0f};
    std::atomic<double> meterPpq{0.0};

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
    reset();
}

void Engine::reset() {
    impl_->internalPpq = 0.0;
    impl_->rmsSmooth = 0.0f;
    impl_->peak.store(0.0f, std::memory_order_relaxed);
    impl_->rms.store(0.0f, std::memory_order_relaxed);
    impl_->meterPpq.store(0.0, std::memory_order_relaxed);
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
                     const MidiEvent* /*events*/, int /*numEvents*/,
                     const Transport& transport) {
    // Write silence in chunks of maxBlockSize
    int offset = 0;
    while (offset < numSamples) {
        int chunk = (numSamples - offset < impl_->maxBlockSize)
                     ? (numSamples - offset)
                     : impl_->maxBlockSize;
        for (int i = 0; i < chunk; ++i) {
            left[offset + i] = 0.0f;
            right[offset + i] = 0.0f;
        }
        offset += chunk;
    }

    // Meters: phase 0 outputs silence so peak and block mean-square are 0
    float blockPeak = 0.0f;
    float blockMeanSq = 0.0f;

    // Smoothed RMS with ~50 ms time constant
    double blockDur = static_cast<double>(numSamples) / impl_->sampleRate;
    double alpha = 1.0 - std::exp(-blockDur / 0.05);
    impl_->rmsSmooth = static_cast<float>(alpha * blockMeanSq + (1.0 - alpha) * impl_->rmsSmooth);

    impl_->peak.store(blockPeak, std::memory_order_relaxed);
    impl_->rms.store(std::sqrt(impl_->rmsSmooth), std::memory_order_relaxed);

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
    return m;
}

}  // namespace undertow
