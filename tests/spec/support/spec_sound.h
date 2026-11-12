// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

// The sound measures of docs/PRD.md section 1, as the spec tests compute them: a render's long-term spectrum and its
// band shares, its centroid, its brightest moments, its 20 ms envelope, and the highs around each hit. They follow
// the definitions the PRD's numbers were measured with (Welch averages of 32768-point Blackman-Harris segments with
// 50% overlap; 4096-point Hann frames every 1024 samples; a 20 ms mean-square envelope; a zero-phase 6 kHz high-pass).

#include "spec_analysis.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace spec {

// The long-term power spectrum: power per bin, averaged over 32768-point segments with 50% overlap, each segment's
// mean removed first.
struct Ltas {
    std::vector<double> power;
    double binHz = 0;
};

inline Ltas ltas(const Signal& x, double sampleRate, std::size_t size = 32768) {
    Ltas l;
    l.power.assign(size / 2 + 1, 0.0);
    l.binHz = sampleRate / static_cast<double>(size);
    int segments = 0;
    for (std::size_t start = 0; start + size <= x.size(); start += size / 2) {
        Signal segment(x.begin() + static_cast<long>(start), x.begin() + static_cast<long>(start + size));
        const double m = mean(segment);
        for (auto& v : segment)
            v = static_cast<float>(v - m);
        const auto s = spectrum(segment, 0, size, sampleRate);
        for (std::size_t k = 0; k < l.power.size(); ++k)
            l.power[k] += s.amplitude[k] * s.amplitude[k];
        ++segments;
    }
    for (auto& p : l.power)
        p /= std::max(segments, 1);
    return l;
}

// The share of the power in [loHz, hiHz).
inline double share(const Ltas& l, double loHz, double hiHz) {
    double band = 0, total = 0;
    for (std::size_t k = 0; k < l.power.size(); ++k) {
        const double hz = static_cast<double>(k) * l.binHz;
        total += l.power[k];
        if (hz >= loHz && hz < hiHz)
            band += l.power[k];
    }
    return total > 0 ? band / total : 0.0;
}

// The power-weighted mean frequency over [loHz, hiHz].
inline double centroidHz(const Ltas& l, double loHz = 20.0, double hiHz = 20000.0) {
    double weighted = 0, total = 0;
    for (std::size_t k = 0; k < l.power.size(); ++k) {
        const double hz = static_cast<double>(k) * l.binHz;
        if (hz < loHz || hz > hiHz)
            continue;
        weighted += hz * l.power[k];
        total += l.power[k];
    }
    return total > 0 ? weighted / total : 0.0;
}

// The brightest moments: the 95th percentile of the centroid of 4096-point Hann frames taken every 1024 samples, over
// the frames within 40 dB of the loudest.
inline double brightnessP95(const Signal& x, double sampleRate) {
    std::vector<double> centroids, powers;
    for (std::size_t start = 0; start + 4096 <= x.size(); start += 1024) {
        const auto s = hannSpectrum(x, start, 4096, sampleRate);
        double weighted = 0, total = 0;
        for (std::size_t k = 0; k < s.amplitude.size(); ++k) {
            const double hz = static_cast<double>(k) * s.binHz();
            if (hz < 20.0 || hz > 20000.0)
                continue;
            const double p = s.amplitude[k] * s.amplitude[k];
            weighted += hz * p;
            total += p;
        }
        centroids.push_back(total > 0 ? weighted / total : 0.0);
        powers.push_back(total);
    }
    const double loudest = powers.empty() ? 0.0 : *std::max_element(powers.begin(), powers.end());
    std::vector<double> kept;
    for (std::size_t i = 0; i < centroids.size(); ++i)
        if (powers[i] > loudest * 1e-4)
            kept.push_back(centroids[i]);
    if (kept.empty())
        return 0.0;
    std::sort(kept.begin(), kept.end());
    const double rank = 0.95 * static_cast<double>(kept.size() - 1);
    const auto below = static_cast<std::size_t>(rank);
    const std::size_t above = std::min(below + 1, kept.size() - 1);
    return kept[below] + (rank - static_cast<double>(below)) * (kept[above] - kept[below]);
}

// 10 log10 of the mean square over a centred window of `seconds`.
inline std::vector<double> envelopeDb(const Signal& x, double sampleRate, double seconds = 0.020) {
    const auto w = std::max<std::size_t>(1, static_cast<std::size_t>(seconds * sampleRate));
    std::vector<double> squares(x.size() + 1, 0.0);
    for (std::size_t n = 0; n < x.size(); ++n)
        squares[n + 1] = squares[n] + static_cast<double>(x[n]) * x[n];
    std::vector<double> env(x.size());
    for (std::size_t n = 0; n < x.size(); ++n) {
        const std::size_t lo = n >= w / 2 ? n - w / 2 : 0, hi = std::min(x.size(), lo + w);
        env[n] = 10.0 * std::log10(std::max((squares[hi] - squares[lo]) / static_cast<double>(w), 1e-20));
    }
    return env;
}

// A zero-phase Butterworth high-pass: forward, then backward.
inline Signal zeroPhaseHighpass(const Signal& x, double hz, double sampleRate, int order) {
    Signal y = highpass(x, hz, sampleRate, order);
    std::reverse(y.begin(), y.end());
    y = highpass(y, hz, sampleRate, order);
    std::reverse(y.begin(), y.end());
    return y;
}

// The highs at the hits: the largest peak, over every onset, of the render high-passed at 6 kHz within 1 ms before
// and 2 ms after it, in dBFS.
inline double clickPeakDbfs(const Signal& x, const std::vector<std::size_t>& onsets, double sampleRate) {
    const Signal hp = zeroPhaseHighpass(x, 6000.0, sampleRate, 4);
    double worst = 0;
    const auto before = static_cast<std::size_t>(0.001 * sampleRate), after = static_cast<std::size_t>(0.002 * sampleRate);
    for (const std::size_t o : onsets)
        for (std::size_t n = o >= before ? o - before : 0; n < std::min(hp.size(), o + after); ++n)
            worst = std::max(worst, static_cast<double>(std::fabs(hp[n])));
    return toDb(worst);
}

} // namespace spec
