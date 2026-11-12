#include "undertow/spectrum.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace undertow {
namespace {

constexpr int kFftOrder = 14;
constexpr int kFftSize = 1 << kFftOrder;
constexpr int kPositiveBins = kFftSize / 2 + 1;
constexpr float kFloorDb = -96.0f;
constexpr double kPi = 3.14159265358979323846;
constexpr double kBandHalfOctave = 1.0 / 12.0;
constexpr double kMinimumBandHalfWidthBins = 1.5;
constexpr double kRiseTauSeconds = 0.025;
constexpr double kFallTauSeconds = 0.300;
constexpr double kShareTauSeconds = 0.500;
constexpr double kBlackmanHarrisA0 = 0.35875;
constexpr double kBlackmanHarrisA1 = 0.48829;
constexpr double kBlackmanHarrisA2 = 0.14128;
constexpr double kBlackmanHarrisA3 = 0.01168;

static_assert(kSpectrumSize == kFftSize, "Spectrum FFT size must match the scope length");

int reverseBits(int value, int bits) {
    int reversed = 0;
    for (int i = 0; i < bits; ++i) {
        reversed = (reversed << 1) | (value & 1);
        value >>= 1;
    }
    return reversed;
}

float smoothLevel(float current, float target, double seconds, double riseTau, double fallTau) {
    if (!(seconds > 0.0) || current == target)
        return target;
    const double tau = target > current ? riseTau : fallTau;
    const double alpha = 1.0 - std::exp(-seconds / tau);
    return static_cast<float>(current + static_cast<float>(alpha) * (target - current));
}

float floorDb(double power) {
    if (!(power > 0.0))
        return kFloorDb;
    return static_cast<float>(std::max<double>(kFloorDb, 10.0 * std::log10(power)));
}

}  // namespace

struct Spectrum::Impl {
    double sampleRate = 48000.0;
    double windowSum = 0.0;
    double windowEquivalentNoiseBandwidth = 0.0;
    double powerScale = 1.0;
    double shareLowStartBin = 0.0;
    double shareLowEndBin = 0.0;
    double shareTotalStartBin = 0.0;
    double shareTotalEndBin = 0.0;
    float lowShare = 0.0f;
    std::array<float, kFftSize> window{};
    std::array<float, kFftSize> real{};
    std::array<float, kFftSize> imag{};
    std::array<float, kPositiveBins> power{};
    std::array<float, kSpectrumPoints> frameLevels{};
    std::array<float, kSpectrumPoints> smoothedLevels{};
    std::array<double, kSpectrumPoints> bandStartBins{};
    std::array<double, kSpectrumPoints> bandEndBins{};
    std::array<std::uint16_t, kFftSize> bitReverse{};
    std::array<float, kFftSize / 2> twiddleReal{};
    std::array<float, kFftSize / 2> twiddleImag{};

    Impl() {
        for (int i = 0; i < kFftSize; ++i) {
            const double phase = 2.0 * kPi * static_cast<double>(i) / static_cast<double>(kFftSize - 1);
            const double value = kBlackmanHarrisA0 - kBlackmanHarrisA1 * std::cos(phase)
                                 + kBlackmanHarrisA2 * std::cos(2.0 * phase) - kBlackmanHarrisA3 * std::cos(3.0 * phase);
            window[static_cast<std::size_t>(i)] = static_cast<float>(value);
            windowSum += value;
            bitReverse[static_cast<std::size_t>(i)] = static_cast<std::uint16_t>(reverseBits(i, kFftOrder));
        }

        double windowSquares = 0.0;
        for (const float value : window)
            windowSquares += static_cast<double>(value) * static_cast<double>(value);
        windowEquivalentNoiseBandwidth = static_cast<double>(kFftSize) * windowSquares / (windowSum * windowSum);
        const double magnitudeScale = 2.0 / windowSum;
        powerScale = magnitudeScale * magnitudeScale;

        for (int i = 0; i < kFftSize / 2; ++i) {
            const double angle = 2.0 * kPi * static_cast<double>(i) / static_cast<double>(kFftSize);
            twiddleReal[static_cast<std::size_t>(i)] = static_cast<float>(std::cos(angle));
            twiddleImag[static_cast<std::size_t>(i)] = static_cast<float>(-std::sin(angle));
        }

        reset();
        updateBands(sampleRate);
    }

    void reset() {
        frameLevels.fill(kFloorDb);
        smoothedLevels.fill(kFloorDb);
        lowShare = 0.0f;
    }

    void updateBands(double newSampleRate) {
        sampleRate = std::max(newSampleRate, 1.0);
        const double binScale = static_cast<double>(kFftSize) / sampleRate;
        const double minEdge = -0.5;
        const double maxEdge = static_cast<double>(kPositiveBins) - 0.5;
        const double octaveFactor = std::pow(2.0, kBandHalfOctave);
        for (int i = 0; i < kSpectrumPoints; ++i) {
            const double hz = Spectrum::pointHz(i);
            const double centerBin = hz * binScale;
            double startBin = (hz / octaveFactor) * binScale;
            double endBin = (hz * octaveFactor) * binScale;
            startBin = std::min(startBin, centerBin - kMinimumBandHalfWidthBins);
            endBin = std::max(endBin, centerBin + kMinimumBandHalfWidthBins);
            bandStartBins[static_cast<std::size_t>(i)] = std::clamp(startBin, minEdge, maxEdge);
            bandEndBins[static_cast<std::size_t>(i)] = std::clamp(endBin, bandStartBins[static_cast<std::size_t>(i)], maxEdge);
        }
        shareLowStartBin = std::clamp(20.0 * binScale, minEdge, maxEdge);
        shareLowEndBin = std::clamp(150.0 * binScale, shareLowStartBin, maxEdge);
        shareTotalStartBin = shareLowStartBin;
        shareTotalEndBin = maxEdge;
    }

    void fft() {
        for (int size = 2; size <= kFftSize; size <<= 1) {
            const int halfSize = size >> 1;
            const int twiddleStride = kFftSize / size;
            for (int start = 0; start < kFftSize; start += size) {
                for (int i = 0; i < halfSize; ++i) {
                    const int even = start + i;
                    const int odd = even + halfSize;
                    const int twiddleIndex = i * twiddleStride;
                    const float wr = twiddleReal[static_cast<std::size_t>(twiddleIndex)];
                    const float wi = twiddleImag[static_cast<std::size_t>(twiddleIndex)];
                    const float oddReal = real[static_cast<std::size_t>(odd)];
                    const float oddImag = imag[static_cast<std::size_t>(odd)];
                    const float tReal = wr * oddReal - wi * oddImag;
                    const float tImag = wr * oddImag + wi * oddReal;
                    const float evenReal = real[static_cast<std::size_t>(even)];
                    const float evenImag = imag[static_cast<std::size_t>(even)];
                    real[static_cast<std::size_t>(even)] = evenReal + tReal;
                    imag[static_cast<std::size_t>(even)] = evenImag + tImag;
                    real[static_cast<std::size_t>(odd)] = evenReal - tReal;
                    imag[static_cast<std::size_t>(odd)] = evenImag - tImag;
                }
            }
        }
    }

    double integratePower(double startBin, double endBin) const {
        if (!(endBin > startBin))
            return 0.0;
        const int firstBin = std::max(0, static_cast<int>(std::floor(startBin + 0.5)));
        const int lastBin = std::min(kPositiveBins - 1, static_cast<int>(std::ceil(endBin + 0.5) - 1.0));
        double sum = 0.0;
        for (int bin = firstBin; bin <= lastBin; ++bin) {
            const double binStart = static_cast<double>(bin) - 0.5;
            const double binEnd = static_cast<double>(bin) + 0.5;
            const double overlap = std::min(endBin, binEnd) - std::max(startBin, binStart);
            if (overlap > 0.0)
                sum += static_cast<double>(power[static_cast<std::size_t>(bin)]) * overlap;
        }
        return sum;
    }
};

Spectrum::Spectrum() : impl_(std::make_unique<Impl>()) {}
Spectrum::~Spectrum() = default;

void Spectrum::setSampleRate(double sampleRate) {
    impl_->updateBands(sampleRate);
    reset();
}

void Spectrum::reset() {
    impl_->reset();
}

void Spectrum::analyse(const float* samples, double seconds) {
    for (int i = 0; i < kFftSize; ++i) {
        const std::size_t sourceIndex = static_cast<std::size_t>(impl_->bitReverse[static_cast<std::size_t>(i)]);
        const float sample = samples == nullptr ? 0.0f : samples[sourceIndex];
        impl_->real[static_cast<std::size_t>(i)] = sample * impl_->window[sourceIndex];
        impl_->imag[static_cast<std::size_t>(i)] = 0.0f;
    }

    impl_->fft();

    for (int bin = 0; bin < kPositiveBins; ++bin) {
        const double real = impl_->real[static_cast<std::size_t>(bin)];
        const double imag = impl_->imag[static_cast<std::size_t>(bin)];
        impl_->power[static_cast<std::size_t>(bin)] = static_cast<float>((real * real + imag * imag) * impl_->powerScale);
    }

    for (int i = 0; i < kSpectrumPoints; ++i) {
        const double bandPower = impl_->integratePower(impl_->bandStartBins[static_cast<std::size_t>(i)],
                                                       impl_->bandEndBins[static_cast<std::size_t>(i)])
                                 / impl_->windowEquivalentNoiseBandwidth;
        const float frame = floorDb(bandPower);
        impl_->frameLevels[static_cast<std::size_t>(i)] = frame;
        impl_->smoothedLevels[static_cast<std::size_t>(i)] = smoothLevel(
            impl_->smoothedLevels[static_cast<std::size_t>(i)], frame, seconds, kRiseTauSeconds, kFallTauSeconds);
    }

    const double lowPower = impl_->integratePower(impl_->shareLowStartBin, impl_->shareLowEndBin);
    const double totalPower = impl_->integratePower(impl_->shareTotalStartBin, impl_->shareTotalEndBin);
    const float targetShare = totalPower > 0.0 ? static_cast<float>(lowPower / totalPower) : 0.0f;
    impl_->lowShare = smoothLevel(impl_->lowShare, targetShare, seconds, kShareTauSeconds, kShareTauSeconds);
}

double Spectrum::pointHz(int index) {
    return 20.0 * std::pow(100.0, static_cast<double>(index) / static_cast<double>(kSpectrumPoints - 1));
}

float Spectrum::frameLevel(int index) const {
    if (index < 0 || index >= kSpectrumPoints)
        return kFloorDb;
    return impl_->frameLevels[static_cast<std::size_t>(index)];
}

float Spectrum::level(int index) const {
    if (index < 0 || index >= kSpectrumPoints)
        return kFloorDb;
    return impl_->smoothedLevels[static_cast<std::size_t>(index)];
}

float Spectrum::share() const {
    return impl_->lowShare;
}

}  // namespace undertow
