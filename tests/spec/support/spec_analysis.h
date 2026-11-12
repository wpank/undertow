#pragma once

// Signal measurements for the spec tests: levels, spectra, pitch, correlation and band splitting.
// Every function is deterministic and plain C++ (no JUCE), so a test reads the same on every run.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace spec {

using Signal = std::vector<float>;
constexpr std::size_t kEnd = SIZE_MAX;

double rms(const Signal& x, std::size_t begin = 0, std::size_t end = kEnd);
double peakAbs(const Signal& x, std::size_t begin = 0, std::size_t end = kEnd);
double mean(const Signal& x, std::size_t begin = 0, std::size_t end = kEnd);
bool allFinite(const Signal& x);
std::size_t countSubnormal(const Signal& x);
double toDb(double ratio); // 20 log10(ratio), -300 for 0

// The spectrum of x[offset, offset + size) under a Blackman-Harris window; size is a power of two.
// amplitude[k] is calibrated so a sine centred on bin k reads its amplitude (a full-scale sine reads 1).
struct Spectrum {
    std::vector<double> amplitude; // bins 0 .. size / 2
    double sampleRate = 0;
    std::size_t size = 0;
    double binHz() const { return sampleRate / static_cast<double>(size); }
};
Spectrum spectrum(const Signal& x, std::size_t offset, std::size_t size, double sampleRate);
// The same under a Hann window (periodic), as the frame-by-frame measures use; calibrated the same way.
Spectrum hannSpectrum(const Signal& x, std::size_t offset, std::size_t size, double sampleRate);

double peakHz(const Spectrum& s, double loHz, double hiHz);     // the strongest bin, interpolated
double amplitudeAt(const Spectrum& s, double hz);               // the peak within 3 bins of hz
double bandPower(const Spectrum& s, double loHz, double hiHz);  // sum of amplitude^2 in [loHz, hiHz]
// Power in [loHz, hiHz] outside +-guardBins of every harmonic of f0: what aliasing and noise add.
double powerAwayFromHarmonics(const Spectrum& s, double f0, double loHz, double hiHz, double guardBins);
// sqrt(sum of harmonic 2..harmonics amplitudes squared) / fundamental amplitude.
double thd(const Spectrum& s, double f0, int harmonics);

// Rising zero crossings in [begin, end), as interpolated sample positions.
std::vector<double> risingZeroCrossings(const Signal& x, std::size_t begin = 0, std::size_t end = kEnd);
// The mean frequency of the rising zero crossings in [begin, end); 0 with fewer than two crossings.
double zeroCrossingHz(const Signal& x, std::size_t begin, std::size_t end, double sampleRate);
// Pearson correlation of a and b over [begin, end); 1 for two identical non-constant signals.
double correlation(const Signal& a, const Signal& b, std::size_t begin = 0, std::size_t end = kEnd);

// Butterworth low- and high-pass filters (order 2 or 4), run forward from rest, for splitting bands.
Signal lowpass(const Signal& x, double hz, double sampleRate, int order);
Signal highpass(const Signal& x, double hz, double sampleRate, int order);

// RMS over windows of `window` samples, one value every `hop` samples.
std::vector<double> rmsEnvelope(const Signal& x, std::size_t window, std::size_t hop);

Signal sine(double hz, double amplitude, std::size_t size, double sampleRate, double phase = 0);

// The steady-state gain in dB of a mono per-sample processor `process(float) -> float` at hz: drive it
// with a sine for settleSeconds, then compare output and input RMS over measureSeconds.
template <class Process>
double gainDb(Process&& process, double hz, double amplitude, double sampleRate, double settleSeconds,
              double measureSeconds) {
    const auto settle = static_cast<std::size_t>(settleSeconds * sampleRate);
    const auto measure = static_cast<std::size_t>(measureSeconds * sampleRate);
    double inPower = 0, outPower = 0;
    const double step = 2.0 * 3.14159265358979323846 * hz / sampleRate;
    for (std::size_t n = 0; n < settle + measure; ++n) {
        const float in = static_cast<float>(amplitude * std::sin(step * static_cast<double>(n)));
        const float out = process(in);
        if (n >= settle) {
            inPower += static_cast<double>(in) * in;
            outPower += static_cast<double>(out) * out;
        }
    }
    if (inPower <= 0)
        return -300;
    return 10.0 * std::log10(std::max(outPower, 1e-300) / inPower);
}

} // namespace spec
