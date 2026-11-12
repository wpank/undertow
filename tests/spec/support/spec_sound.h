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
#include <utility>
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

// Integrated loudness (ITU-R BS.1770-4, as pyloudnorm measures it) of a mono signal played on both channels of a
// stereo file, in LUFS: K-weighting (the 48 kHz coefficients), 400 ms blocks every 100 ms, the absolute gate at -70
// LUFS and the relative gate 10 LU under the gated mean.
inline double integratedLufs(const Signal& x) {
    const double b1[3] = {1.53512485958697, -2.69169618940638, 1.19839281085285};
    const double a1[3] = {1.0, -1.69065929318241, 0.73248077421585};
    const double b2[3] = {1.0, -2.0, 1.0};
    const double a2[3] = {1.0, -1.99004745483398, 0.99007225036621};
    std::vector<double> k(x.size());
    double s1[2] = {0, 0}, s2[2] = {0, 0};
    for (std::size_t n = 0; n < x.size(); ++n) {
        const double in = x[n];
        const double y1 = b1[0] * in + s1[0];
        s1[0] = b1[1] * in - a1[1] * y1 + s1[1];
        s1[1] = b1[2] * in - a1[2] * y1;
        const double y2 = b2[0] * y1 + s2[0];
        s2[0] = b2[1] * y1 - a2[1] * y2 + s2[1];
        s2[1] = b2[2] * y1 - a2[2] * y2;
        k[n] = y2;
    }
    const std::size_t block = 19200, hop = 4800; // 400 ms and 100 ms at 48 kHz
    std::vector<double> z;
    for (std::size_t start = 0; start + block <= k.size(); start += hop) {
        double sum = 0;
        for (std::size_t n = start; n < start + block; ++n)
            sum += k[n] * k[n];
        z.push_back(2.0 * sum / static_cast<double>(block)); // the same signal on two channels
    }
    auto loudness = [](double power) { return -0.691 + 10.0 * std::log10(std::max(power, 1e-30)); };
    auto gatedMean = [&](double gate) {
        double sum = 0;
        int count = 0;
        for (const double v : z)
            if (loudness(v) > gate) {
                sum += v;
                ++count;
            }
        return count > 0 ? sum / count : 0.0;
    };
    const double relative = loudness(gatedMean(-70.0)) - 10.0;
    return loudness(gatedMean(std::max(-70.0, relative)));
}

// The true peak in dBTP: the largest sample of the signal upsampled 4 times (scipy's resample_poly(x, 4, 1): an
// 81-tap Kaiser-windowed sinc, beta 5).
inline double truePeakDbtp(const Signal& x) {
    constexpr int kTaps = 81, kUp = 4;
    auto besselI0 = [](double v) {
        double sum = 1, term = 1;
        for (int i = 1; i < 30; ++i) {
            term *= (v / (2.0 * i)) * (v / (2.0 * i));
            sum += term;
        }
        return sum;
    };
    double h[kTaps], total = 0;
    for (int n = 0; n < kTaps; ++n) {
        const double t = n - (kTaps - 1) / 2.0, r = 2.0 * n / (kTaps - 1) - 1.0;
        const double sinc = t == 0 ? 1.0 : std::sin(3.14159265358979323846 * t / kUp) / (3.14159265358979323846 * t / kUp);
        h[n] = sinc / kUp * besselI0(5.0 * std::sqrt(std::max(0.0, 1.0 - r * r))) / besselI0(5.0);
        total += h[n];
    }
    double peak = 0;
    const auto size = static_cast<long>(x.size());
    for (long m = 0; m < size * kUp; ++m) { // output sample m of the upsampled signal
        double acc = 0;
        for (int n = (m + 40) % kUp; n < kTaps; n += kUp) {
            const long i = (m + 40 - n) / kUp;
            if (i >= 0 && i < size)
                acc += h[n] * x[static_cast<std::size_t>(i)];
        }
        peak = std::max(peak, std::fabs(acc * kUp / total));
    }
    return 20.0 * std::log10(std::max(peak, 1e-15));
}

// How the same passage differs from one repeat to the next: the 20 ms envelope (dB) of `cycles` consecutive repeats
// of `cycle` samples from `start`, the spread (standard deviation) across repeats at each moment, its median.
inline double repeatSpreadDb(const Signal& x, std::size_t start, std::size_t cycle, int cycles, double sampleRate) {
    const auto env = envelopeDb(x, sampleRate, 0.020);
    std::vector<double> spreads;
    for (std::size_t k = 0; k < cycle; ++k) {
        double mean = 0, square = 0;
        int n = 0;
        for (int c = 0; c < cycles; ++c) {
            const std::size_t at = start + static_cast<std::size_t>(c) * cycle + k;
            if (at < env.size()) {
                mean += env[at];
                ++n;
            }
        }
        if (n < 2)
            continue;
        mean /= n;
        for (int c = 0; c < cycles; ++c) {
            const std::size_t at = start + static_cast<std::size_t>(c) * cycle + k;
            if (at < env.size())
                square += (env[at] - mean) * (env[at] - mean);
        }
        spreads.push_back(std::sqrt(square / n));
    }
    if (spreads.empty())
        return 0.0;
    std::sort(spreads.begin(), spreads.end());
    return spreads[spreads.size() / 2];
}

// A plain 4/4 kick, the one the PRD's under-the-kick goals are measured with: a sine falling from 190 to 49 Hz
// (26 ms), held 10 ms and decaying with a 90 ms time constant, 0.6 s long, softly saturated. One on every beat from
// sample 0; `onsets` gets their samples.
inline Signal kickTrack(std::size_t samples, double bpm, double sampleRate, std::vector<std::size_t>* onsets = nullptr) {
    const auto length = static_cast<std::size_t>(0.6 * sampleRate);
    std::vector<double> hit(length);
    double phase = 0;
    for (std::size_t n = 0; n < length; ++n) {
        const double t = static_cast<double>(n) / sampleRate;
        const double hz = 49.0 + (190.0 - 49.0) * std::exp(-t / 0.026);
        double amp = std::exp(-std::max(0.0, t - 0.010) / 0.09);
        const auto attack = static_cast<std::size_t>(0.0004 * sampleRate), fade = static_cast<std::size_t>(0.03 * sampleRate);
        if (n < attack)
            amp *= 0.5 * (1.0 - std::cos(3.14159265358979323846 * static_cast<double>(n) / static_cast<double>(attack)));
        if (n >= length - fade)
            amp *= 0.5 * (1.0 + std::cos(3.14159265358979323846 * static_cast<double>(n - (length - fade)) / static_cast<double>(fade)));
        hit[n] = std::tanh(1.4 * std::sin(phase) * amp) / std::tanh(1.4);
        phase += 2.0 * 3.14159265358979323846 * hz / sampleRate;
    }
    Signal out(samples, 0.0f);
    const double beat = 60.0 / bpm * sampleRate;
    for (int b = 0; static_cast<double>(b) * beat < static_cast<double>(samples); ++b) {
        const auto at = static_cast<std::size_t>(std::lround(b * beat));
        if (onsets != nullptr)
            onsets->push_back(at);
        for (std::size_t n = 0; n < length && at + n < samples; ++n)
            out[at + n] = static_cast<float>(out[at + n] + hit[n]);
    }
    return out;
}

// The voice under the kick: the kick scaled so the voice's loudest 20 ms sit `voiceOverKickDb` from the kick's.
inline Signal underTheKick(const Signal& voice, double bpm, double sampleRate, double voiceOverKickDb,
                           std::vector<std::size_t>* onsets = nullptr) {
    const auto kick = kickTrack(voice.size(), bpm, sampleRate, onsets);
    const auto ve = envelopeDb(voice, sampleRate, 0.020), ke = envelopeDb(kick, sampleRate, 0.020);
    const double gain = std::pow(10.0, (*std::max_element(ve.begin(), ve.end()) - voiceOverKickDb -
                                        *std::max_element(ke.begin(), ke.end())) / 20.0);
    Signal mix(voice.size());
    for (std::size_t n = 0; n < voice.size(); ++n)
        mix[n] = static_cast<float>(voice[n] + gain * kick[n]);
    return mix;
}

// The holes the kick's tail digs in a held note: for each kick but the first two and the last, the deepest valley of
// the mix's 20 ms envelope under its own 80 ms running maximum, 60 to 440 ms after the kick. Returns the median and
// the worst, in dB.
inline std::pair<double, double> postKickValleysDb(const Signal& mix, const std::vector<std::size_t>& onsets,
                                                   double sampleRate) {
    const auto env = envelopeDb(mix, sampleRate, 0.020);
    const auto w = static_cast<std::size_t>(0.08 * sampleRate), half = w / 2;
    std::vector<double> valleys;
    for (std::size_t i = 2; i + 1 < onsets.size(); ++i) {
        const std::size_t a = onsets[i] + static_cast<std::size_t>(0.06 * sampleRate);
        const std::size_t b = std::min(env.size(), onsets[i] + static_cast<std::size_t>(0.44 * sampleRate));
        if (b <= a + static_cast<std::size_t>(0.1 * sampleRate))
            continue;
        // The centred running maximum over [n - half, n + half], kept in a monotonic queue.
        std::vector<std::size_t> queue(b - a);
        std::size_t head = 0, tail = 0, next = a;
        double worst = 0;
        for (std::size_t n = a; n < b; ++n) {
            for (; next < b && next <= n + half; ++next) {
                while (tail > head && env[queue[tail - 1]] <= env[next])
                    --tail;
                queue[tail++] = next;
            }
            while (queue[head] + half < n)
                ++head;
            worst = std::max(worst, env[queue[head]] - env[n]);
        }
        valleys.push_back(worst);
    }
    if (valleys.empty())
        return {0.0, 0.0};
    std::sort(valleys.begin(), valleys.end());
    return {valleys[valleys.size() / 2], valleys.back()};
}

// How far each hit falls before the next one (the PRD's roll): for each onset, the 20 ms envelope from the onset to
// 12 ms before the next onset (so the centred window never reaches the next attack), its peak in the first 60 ms
// against its last value. Returns the median, in dB.
inline double medianDropBeforeNextDb(const Signal& x, const std::vector<std::size_t>& onsets, double sampleRate) {
    const auto env = envelopeDb(x, sampleRate, 0.020);
    const auto guard = static_cast<std::size_t>(0.6 * 0.020 * sampleRate);
    std::vector<double> drops;
    for (std::size_t i = 0; i < onsets.size(); ++i) {
        const std::size_t a = onsets[i];
        std::size_t b = (i + 1 < onsets.size() ? onsets[i + 1] : x.size());
        if (b < a + guard + static_cast<std::size_t>(0.02 * sampleRate))
            continue;
        b -= guard;
        const std::size_t head = std::min(b, a + static_cast<std::size_t>(0.06 * sampleRate));
        const double peak = *std::max_element(env.begin() + static_cast<long>(a), env.begin() + static_cast<long>(head));
        drops.push_back(peak - env[b - 1]);
    }
    if (drops.empty())
        return 0.0;
    std::sort(drops.begin(), drops.end());
    return drops[drops.size() / 2];
}

} // namespace spec
