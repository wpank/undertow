#include "spec_analysis.h"

#include <algorithm>
#include <complex>
#include <limits>

namespace spec {

namespace {

constexpr double kPi = 3.14159265358979323846;

std::size_t clampEnd(const Signal& x, std::size_t end) { return std::min(end, x.size()); }

void fft(std::vector<std::complex<double>>& data) {
    const std::size_t n = data.size();
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(data[i], data[j]);
    }
    for (std::size_t length = 2; length <= n; length <<= 1) {
        const double angle = -2.0 * kPi / static_cast<double>(length);
        const std::complex<double> unit(std::cos(angle), std::sin(angle));
        for (std::size_t start = 0; start < n; start += length) {
            std::complex<double> w(1.0, 0.0);
            for (std::size_t k = 0; k < length / 2; ++k) {
                const auto even = data[start + k];
                const auto odd = data[start + k + length / 2] * w;
                data[start + k] = even + odd;
                data[start + k + length / 2] = even - odd;
                w *= unit;
            }
        }
    }
}

struct Biquad {
    double b0, b1, b2, a1, a2;
    double z1 = 0, z2 = 0;
    float process(float in) {
        const double out = b0 * in + z1;
        z1 = b1 * in - a1 * out + z2;
        z2 = b2 * in - a2 * out;
        return static_cast<float>(out);
    }
};

Biquad makeBiquad(bool high, double hz, double q, double sampleRate) {
    const double w = 2.0 * kPi * hz / sampleRate;
    const double alpha = std::sin(w) / (2.0 * q);
    const double c = std::cos(w);
    const double a0 = 1.0 + alpha;
    Biquad f{};
    if (high) {
        f.b0 = (1.0 + c) / 2.0 / a0;
        f.b1 = -(1.0 + c) / a0;
        f.b2 = f.b0;
    } else {
        f.b0 = (1.0 - c) / 2.0 / a0;
        f.b1 = (1.0 - c) / a0;
        f.b2 = f.b0;
    }
    f.a1 = -2.0 * c / a0;
    f.a2 = (1.0 - alpha) / a0;
    return f;
}

Signal butterworth(const Signal& x, bool high, double hz, double sampleRate, int order) {
    std::vector<double> qs = order >= 4 ? std::vector<double>{0.54119610, 1.30656296}
                                        : std::vector<double>{0.70710678};
    Signal y = x;
    for (double q : qs) {
        Biquad f = makeBiquad(high, hz, q, sampleRate);
        for (auto& v : y)
            v = f.process(v);
    }
    return y;
}

} // namespace

double rms(const Signal& x, std::size_t begin, std::size_t end) {
    end = clampEnd(x, end);
    if (begin >= end)
        return 0;
    double sum = 0;
    for (std::size_t n = begin; n < end; ++n)
        sum += static_cast<double>(x[n]) * x[n];
    return std::sqrt(sum / static_cast<double>(end - begin));
}

double peakAbs(const Signal& x, std::size_t begin, std::size_t end) {
    end = clampEnd(x, end);
    double peak = 0;
    for (std::size_t n = begin; n < end; ++n)
        peak = std::max(peak, std::fabs(static_cast<double>(x[n])));
    return peak;
}

double mean(const Signal& x, std::size_t begin, std::size_t end) {
    end = clampEnd(x, end);
    if (begin >= end)
        return 0;
    double sum = 0;
    for (std::size_t n = begin; n < end; ++n)
        sum += x[n];
    return sum / static_cast<double>(end - begin);
}

bool allFinite(const Signal& x) {
    return std::all_of(x.begin(), x.end(), [](float v) { return std::isfinite(v); });
}

std::size_t countSubnormal(const Signal& x) {
    return static_cast<std::size_t>(
        std::count_if(x.begin(), x.end(), [](float v) { return std::fpclassify(v) == FP_SUBNORMAL; }));
}

double toDb(double ratio) { return ratio > 0 ? 20.0 * std::log10(ratio) : -300.0; }

Spectrum spectrum(const Signal& x, std::size_t offset, std::size_t size, double sampleRate) {
    constexpr double a0 = 0.35875, a1 = 0.48829, a2 = 0.14128, a3 = 0.01168;
    std::vector<std::complex<double>> data(size);
    for (std::size_t n = 0; n < size; ++n) {
        const double phase = 2.0 * kPi * static_cast<double>(n) / static_cast<double>(size);
        const double window = a0 - a1 * std::cos(phase) + a2 * std::cos(2 * phase) - a3 * std::cos(3 * phase);
        const double value = offset + n < x.size() ? static_cast<double>(x[offset + n]) : 0.0;
        data[n] = value * window;
    }
    fft(data);
    Spectrum s;
    s.sampleRate = sampleRate;
    s.size = size;
    s.amplitude.resize(size / 2 + 1);
    const double scale = 2.0 / (static_cast<double>(size) * a0);
    for (std::size_t k = 0; k <= size / 2; ++k)
        s.amplitude[k] = std::abs(data[k]) * scale;
    return s;
}

Spectrum hannSpectrum(const Signal& x, std::size_t offset, std::size_t size, double sampleRate) {
    std::vector<std::complex<double>> data(size);
    for (std::size_t n = 0; n < size; ++n) {
        const double window = 0.5 - 0.5 * std::cos(2.0 * kPi * static_cast<double>(n) / static_cast<double>(size));
        const double value = offset + n < x.size() ? static_cast<double>(x[offset + n]) : 0.0;
        data[n] = value * window;
    }
    fft(data);
    Spectrum s;
    s.sampleRate = sampleRate;
    s.size = size;
    s.amplitude.resize(size / 2 + 1);
    const double scale = 2.0 / (static_cast<double>(size) * 0.5);
    for (std::size_t k = 0; k <= size / 2; ++k)
        s.amplitude[k] = std::abs(data[k]) * scale;
    return s;
}

double peakHz(const Spectrum& s, double loHz, double hiHz) {
    const auto lo = static_cast<std::size_t>(std::max(1.0, std::floor(loHz / s.binHz())));
    const auto hi = static_cast<std::size_t>(
        std::min(static_cast<double>(s.amplitude.size() - 2), std::ceil(hiHz / s.binHz())));
    std::size_t best = lo;
    for (std::size_t k = lo; k <= hi; ++k)
        if (s.amplitude[k] > s.amplitude[best])
            best = k;
    const double alpha = toDb(s.amplitude[best - 1]);
    const double beta = toDb(s.amplitude[best]);
    const double gamma = toDb(s.amplitude[best + 1]);
    const double denominator = alpha - 2 * beta + gamma;
    const double delta = denominator != 0 ? 0.5 * (alpha - gamma) / denominator : 0.0;
    return (static_cast<double>(best) + std::clamp(delta, -0.5, 0.5)) * s.binHz();
}

double amplitudeAt(const Spectrum& s, double hz) {
    const auto centre = static_cast<long>(std::lround(hz / s.binHz()));
    double best = 0;
    for (long k = centre - 3; k <= centre + 3; ++k)
        if (k >= 0 && k < static_cast<long>(s.amplitude.size()))
            best = std::max(best, s.amplitude[static_cast<std::size_t>(k)]);
    return best;
}

double bandPower(const Spectrum& s, double loHz, double hiHz) {
    double sum = 0;
    for (std::size_t k = 0; k < s.amplitude.size(); ++k) {
        const double hz = static_cast<double>(k) * s.binHz();
        if (hz >= loHz && hz <= hiHz)
            sum += s.amplitude[k] * s.amplitude[k];
    }
    return sum;
}

double powerAwayFromHarmonics(const Spectrum& s, double f0, double loHz, double hiHz, double guardBins) {
    double sum = 0;
    for (std::size_t k = 0; k < s.amplitude.size(); ++k) {
        const double hz = static_cast<double>(k) * s.binHz();
        if (hz < loHz || hz > hiHz)
            continue;
        const double harmonic = std::max(1.0, std::round(hz / f0));
        if (std::fabs(hz - harmonic * f0) <= guardBins * s.binHz())
            continue;
        sum += s.amplitude[k] * s.amplitude[k];
    }
    return sum;
}

double thd(const Spectrum& s, double f0, int harmonics) {
    const double fundamental = amplitudeAt(s, f0);
    double sum = 0;
    for (int h = 2; h <= harmonics; ++h) {
        const double hz = f0 * h;
        if (hz >= s.sampleRate / 2 - 4 * s.binHz())
            break;
        const double a = amplitudeAt(s, hz);
        sum += a * a;
    }
    return fundamental > 0 ? std::sqrt(sum) / fundamental : std::numeric_limits<double>::infinity();
}

std::vector<double> risingZeroCrossings(const Signal& x, std::size_t begin, std::size_t end) {
    end = clampEnd(x, end);
    std::vector<double> crossings;
    for (std::size_t n = std::max<std::size_t>(begin, 1); n < end; ++n) {
        const double a = x[n - 1], b = x[n];
        if (a < 0.0 && b >= 0.0)
            crossings.push_back(static_cast<double>(n - 1) + a / (a - b));
    }
    return crossings;
}

double zeroCrossingHz(const Signal& x, std::size_t begin, std::size_t end, double sampleRate) {
    const auto crossings = risingZeroCrossings(x, begin, end);
    if (crossings.size() < 2)
        return 0;
    const double periods = static_cast<double>(crossings.size() - 1);
    return periods * sampleRate / (crossings.back() - crossings.front());
}

double correlation(const Signal& a, const Signal& b, std::size_t begin, std::size_t end) {
    end = std::min(clampEnd(a, end), b.size());
    if (begin >= end)
        return 0;
    const double meanA = mean(a, begin, end), meanB = mean(b, begin, end);
    double ab = 0, aa = 0, bb = 0;
    for (std::size_t n = begin; n < end; ++n) {
        const double da = a[n] - meanA, db = b[n] - meanB;
        ab += da * db;
        aa += da * da;
        bb += db * db;
    }
    if (aa <= 0 || bb <= 0)
        return aa == bb ? 1.0 : 0.0;
    return ab / std::sqrt(aa * bb);
}

Signal lowpass(const Signal& x, double hz, double sampleRate, int order) {
    return butterworth(x, false, hz, sampleRate, order);
}

Signal highpass(const Signal& x, double hz, double sampleRate, int order) {
    return butterworth(x, true, hz, sampleRate, order);
}

std::vector<double> rmsEnvelope(const Signal& x, std::size_t window, std::size_t hop) {
    std::vector<double> envelope;
    for (std::size_t start = 0; start + window <= x.size(); start += hop)
        envelope.push_back(rms(x, start, start + window));
    return envelope;
}

Signal sine(double hz, double amplitude, std::size_t size, double sampleRate, double phase) {
    Signal y(size);
    const double step = 2.0 * kPi * hz / sampleRate;
    for (std::size_t n = 0; n < size; ++n)
        y[n] = static_cast<float>(amplitude * std::sin(phase + step * static_cast<double>(n)));
    return y;
}

} // namespace spec
