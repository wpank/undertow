// SPDX-License-Identifier: AGPL-3.0-only
// Phase 6 spec tests: the scope's length and the window's spectrum (docs/specs/phase-6-window.md, checks P6.01 to
// P6.07; docs/design/window.md section 4.2).
// spec-needs: core/include/undertow/spectrum.h

#include "doctest.h"

#include "spec_alloc.h"
#include "spec_engine.h"
#include "undertow/engine.h"
#include "undertow/spectrum.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;

std::vector<float> sine(double hz, double db, double sampleRate = 48000.0, double phase = 0.3) {
    std::vector<float> x(undertow::kSpectrumSize);
    const double a = std::pow(10.0, db / 20.0);
    for (int n = 0; n < undertow::kSpectrumSize; ++n)
        x[static_cast<std::size_t>(n)] = static_cast<float>(a * std::sin(2.0 * kPi * hz * n / sampleRate + phase));
    return x;
}

// A fixed xorshift, so every run analyses the same noise.
struct Rng {
    std::uint32_t s = 0x2545F491u;
    float next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return static_cast<float>(s) / 4294967296.0f * 2.0f - 1.0f;
    }
};

std::vector<float> white(Rng& rng, float amplitude = 0.2f) {
    std::vector<float> x(undertow::kSpectrumSize);
    for (auto& v : x)
        v = amplitude * rng.next();
    return x;
}

// Pink noise from white through Paul Kellet's filter (within 0.05 dB of -3 dB per octave above 10 Hz at 48 kHz).
std::vector<float> pink(Rng& rng) {
    std::vector<float> x(undertow::kSpectrumSize);
    double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    for (int warm = 0; warm < 4 * undertow::kSpectrumSize; ++warm) {  // settle the filter
        const double w = rng.next();
        b0 = 0.99886 * b0 + w * 0.0555179;
        b1 = 0.99332 * b1 + w * 0.0750759;
        b2 = 0.96900 * b2 + w * 0.1538520;
        b3 = 0.86650 * b3 + w * 0.3104856;
        b4 = 0.55000 * b4 + w * 0.5329522;
        b5 = -0.7616 * b5 - w * 0.0168980;
        b6 = w * 0.115926;
    }
    for (auto& v : x) {
        const double w = rng.next();
        b0 = 0.99886 * b0 + w * 0.0555179;
        b1 = 0.99332 * b1 + w * 0.0750759;
        b2 = 0.96900 * b2 + w * 0.1538520;
        b3 = 0.86650 * b3 + w * 0.3104856;
        b4 = 0.55000 * b4 + w * 0.5329522;
        b5 = -0.7616 * b5 - w * 0.0168980;
        v = static_cast<float>(0.05 * (b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362));
        b6 = w * 0.115926;
    }
    return x;
}

int peakPoint(const undertow::Spectrum& s) {
    int best = 0;
    for (int i = 1; i < undertow::kSpectrumPoints; ++i)
        if (s.frameLevel(i) > s.frameLevel(best))
            best = i;
    return best;
}

}  // namespace

TEST_CASE("P6.01 the scope keeps the newest 16384 output samples, in order, through blocks of any size, without "
          "allocating") {
    CHECK_MESSAGE(undertow::kScopeSamples == 16384,
                  "kScopeSamples is " << undertow::kScopeSamples << "; the window's spectrum needs 16384 samples (341 "
                  "ms at 48 kHz) to resolve the 49 Hz sub: change the constant in core/include/undertow/engine.h");
    CHECK_MESSAGE(undertow::kSpectrumSize == undertow::kScopeSamples,
                  "kSpectrumSize must equal kScopeSamples: a frame analyses the whole scope");
    undertow::Engine engine;
    engine.prepare(48000.0, 512);
    spec::apply(engine, spec::rumblePatch());  // the rumble plays on the engine's own clock: the output is not silent
    std::vector<float> left(1536), right(1536), all;
    std::vector<float> copy(static_cast<std::size_t>(undertow::kScopeSamples) + 100);
    const undertow::Transport noHost{};
    const int sizes[] = {1, 37, 512, 129, 1536, 3, 480, 1000};
    std::size_t allocations = 0;
    for (int round = 0; round < 12; ++round)
        for (const int n : sizes) {
            {
                spec::AllocationCounter counter;
                engine.process(left.data(), right.data(), n, nullptr, 0, noHost);
                (void)engine.copyScope(copy.data(), undertow::kScopeSamples);
                allocations += counter.count();
            }
            all.insert(all.end(), left.begin(), left.begin() + n);
        }
    CHECK_MESSAGE(allocations == 0, "process() and copyScope() allocated " << allocations << " times: the scope's "
                                    "ring is sized in prepare() (or in the engine's state) and never grows");
    REQUIRE(all.size() > static_cast<std::size_t>(undertow::kScopeSamples));
    double energy = 0;
    for (const float v : all)
        energy += static_cast<double>(v) * v;
    REQUIRE_MESSAGE(energy > 0.0, "the rumble alone should make sound on the engine's own clock");
    const int copied = engine.copyScope(copy.data(), static_cast<int>(copy.size()));
    CHECK_MESSAGE(copied == undertow::kScopeSamples, "copyScope copied " << copied << " samples when asked for more "
                                                     "than the scope holds; it copies at most kScopeSamples");
    int mismatches = 0, first = -1;
    for (int i = 0; i < undertow::kScopeSamples; ++i) {
        const float expected = all[all.size() - static_cast<std::size_t>(undertow::kScopeSamples) + i];
        if (copy[static_cast<std::size_t>(i)] != expected) {
            ++mismatches;
            if (first < 0)
                first = i;
        }
    }
    CHECK_MESSAGE(mismatches == 0, mismatches << " of the scope's samples differ from the last "
                                   << undertow::kScopeSamples << " output samples (first at " << first
                                   << "); copyScope gives the newest samples, oldest first");
}

TEST_CASE("P6.02 the 220 display points are log-spaced from 20 Hz to 2 kHz, and silence reads the floor, -96 dB") {
    CHECK(undertow::kSpectrumPoints == 220);
    for (int i = 0; i < undertow::kSpectrumPoints; ++i) {
        const double expected = 20.0 * std::pow(100.0, i / 219.0);
        CHECK_MESSAGE(undertow::Spectrum::pointHz(i) == doctest::Approx(expected).epsilon(1e-9),
                      "pointHz(" << i << ") is " << undertow::Spectrum::pointHz(i) << "; it is 20 * 100^(i / 219), "
                      << expected << " Hz");
    }
    undertow::Spectrum s;
    s.setSampleRate(48000.0);
    const std::vector<float> silence(undertow::kSpectrumSize, 0.0f);
    s.analyse(silence.data(), 1.0 / 30.0);
    for (int i = 0; i < undertow::kSpectrumPoints; ++i) {
        CHECK_MESSAGE(s.frameLevel(i) == doctest::Approx(-96.0), "silence must read the floor, -96 dB, at point " << i
                                                                   << "; it reads " << s.frameLevel(i));
        CHECK_MESSAGE(s.level(i) == doctest::Approx(-96.0), "a new spectrum's smoothed points start at -96 dB");
    }
    CHECK(s.share() == doctest::Approx(0.0));
}

TEST_CASE("P6.03 a sine reads its own level within 0.5 dB at its own frequency, and its peak sits on it") {
    const double frequencies[] = {41.2, 49.0, 73.4, 98.0, 150.0, 500.0, 1000.0};
    for (const double rate : {48000.0, 44100.0}) {
        undertow::Spectrum s;
        s.setSampleRate(rate);
        for (const double hz : frequencies)
            for (const double db : {-12.0, -36.0}) {
                const auto x = sine(hz, db, rate);
                s.analyse(x.data(), 10.0);
                INFO("a " << hz << " Hz sine at " << db << " dBFS, sample rate " << rate);
                int nearest = 0;
                for (int i = 1; i < undertow::kSpectrumPoints; ++i)
                    if (std::abs(std::log(undertow::Spectrum::pointHz(i) / hz)) <
                        std::abs(std::log(undertow::Spectrum::pointHz(nearest) / hz)))
                        nearest = i;
                CHECK_MESSAGE(std::abs(s.frameLevel(nearest) - db) <= 0.5,
                              "the point nearest it (" << undertow::Spectrum::pointHz(nearest) << " Hz) reads "
                              << s.frameLevel(nearest) << " dB: sum the power in the point's band (a twelfth of an "
                              "octave either side, at least 1.5 bins either side, fractional bins at the edges; "
                              "magnitudes scaled so a full-scale sine's peak bin is 1) and divide by the window's "
                              "equivalent noise bandwidth, 2.0 bins for a 4-term Blackman-Harris window");
                const int p = peakPoint(s);
                CHECK_MESSAGE(s.frameLevel(p) <= db + 0.5, "no point may read more than the sine's level; point "
                                                           << p << " reads " << s.frameLevel(p) << " dB");
                if (hz <= 150.0)
                    CHECK_MESSAGE(std::abs(undertow::Spectrum::pointHz(p) / hz - 1.0) <= 0.03,
                                  "below 150 Hz the peak must sit on the sine: it is at " << undertow::Spectrum::pointHz(p)
                                  << " Hz (centre each band on its point)");
                CHECK_MESSAGE(std::abs(s.level(nearest) - s.frameLevel(nearest)) <= 0.01,
                              "after 10 s of the same frame the smoothed point equals the frame's");
            }
    }
}

TEST_CASE("P6.04 pink noise reads flat within 3 dB from 100 Hz to 2 kHz, and white noise rises about 3 dB an octave") {
    undertow::Spectrum s;
    s.setSampleRate(48000.0);
    Rng rng;
    auto average = [&](bool pinkNoise) {
        std::vector<double> sum(undertow::kSpectrumPoints, 0.0);
        const int frames = 16;
        for (int f = 0; f < frames; ++f) {
            const auto x = pinkNoise ? pink(rng) : white(rng);
            s.analyse(x.data(), 1.0 / 30.0);
            for (int i = 0; i < undertow::kSpectrumPoints; ++i)
                sum[static_cast<std::size_t>(i)] += std::pow(10.0, s.frameLevel(i) / 10.0) / frames;
        }
        for (auto& v : sum)
            v = 10.0 * std::log10(v);
        return sum;
    };
    const auto pinkDb = average(true);
    double lo = 1e9, hi = -1e9, mean = 0;
    int n = 0;
    for (int i = 0; i < undertow::kSpectrumPoints; ++i) {
        const double f = undertow::Spectrum::pointHz(i);
        if (f < 100.0 || f > 2000.0)
            continue;
        lo = std::min(lo, pinkDb[static_cast<std::size_t>(i)]);
        hi = std::max(hi, pinkDb[static_cast<std::size_t>(i)]);
        mean += pinkDb[static_cast<std::size_t>(i)];
        ++n;
    }
    mean /= n;
    CHECK_MESSAGE(hi - mean <= 3.0, "pink noise peaks " << hi - mean << " dB over its mean between 100 Hz and 2 kHz; a "
                                    "band a twelfth of an octave either side reads pink noise flat");
    CHECK_MESSAGE(mean - lo <= 3.0, "pink noise dips " << mean - lo << " dB under its mean between 100 Hz and 2 kHz");

    const auto whiteDb = average(false);
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    int m = 0;
    for (int i = 0; i < undertow::kSpectrumPoints; ++i) {
        const double f = undertow::Spectrum::pointHz(i);
        if (f < 200.0 || f > 2000.0)
            continue;
        const double x = std::log2(f), y = whiteDb[static_cast<std::size_t>(i)];
        sx += x;
        sy += y;
        sxx += x * x;
        sxy += x * y;
        ++m;
    }
    const double slope = (m * sxy - sx * sy) / (m * sxx - sx * sx);
    CHECK_MESSAGE(slope >= 2.0, "white noise rises " << slope << " dB an octave from 200 Hz to 2 kHz; band power "
                                "(a band's width grows with its frequency) makes it rise about 3 dB an octave");
    CHECK_MESSAGE(slope <= 4.0, "white noise rises " << slope << " dB an octave from 200 Hz to 2 kHz; band power "
                                "makes it rise about 3 dB an octave");
}

TEST_CASE("P6.05 each point moves towards the frame's level: 25 ms rising, 300 ms falling") {
    undertow::Spectrum s;
    s.setSampleRate(48000.0);
    const auto x = sine(98.0, -24.0);
    s.analyse(x.data(), 0.025);
    const int p = peakPoint(s);
    const double frame = s.frameLevel(p);
    const double risen = -96.0 + (frame + 96.0) * (1.0 - std::exp(-1.0));
    CHECK_MESSAGE(s.level(p) == doctest::Approx(risen).epsilon(0.005),
                  "25 ms after a sine starts, its point reads " << s.level(p) << " dB; it moves (frame - level) * (1 - "
                  "e^(-seconds / 0.025)) rising: " << risen << " dB");
    const double before = s.level(p);
    const std::vector<float> silence(undertow::kSpectrumSize, 0.0f);
    s.analyse(silence.data(), 0.3);
    const double fallen = before + (-96.0 - before) * (1.0 - std::exp(-1.0));
    CHECK_MESSAGE(s.level(p) == doctest::Approx(fallen).epsilon(0.005),
                  "300 ms into silence the point reads " << s.level(p) << " dB; it moves with a 300 ms time constant "
                  "falling: " << fallen << " dB");
    CHECK_MESSAGE(s.frameLevel(p) == doctest::Approx(-96.0), "the frame itself reads silence at once");
    s.reset();
    CHECK_MESSAGE(s.level(p) == doctest::Approx(-96.0), "reset() puts every point back at the floor");
}

TEST_CASE("P6.06 the share below 150 Hz: all of a sub's power, none of a 1 kHz tone's, smoothed over 500 ms") {
    undertow::Spectrum s;
    s.setSampleRate(48000.0);
    auto x = sine(49.0, -12.0);
    s.analyse(x.data(), 10.0);
    CHECK_MESSAGE(s.share() >= 0.99, "a 49 Hz sine is all below 150 Hz; the share reads " << s.share() << ": it is the "
                                     "power of the bins from 20 to 150 Hz over the power of the bins from 20 Hz up");
    x = sine(1000.0, -12.0);
    s.analyse(x.data(), 10.0);
    CHECK_MESSAGE(s.share() <= 0.01, "a 1 kHz sine has nothing below 150 Hz; the share reads " << s.share());
    x = sine(200.0, -12.0);
    s.analyse(x.data(), 10.0);
    CHECK_MESSAGE(s.share() <= 0.01, "a 200 Hz sine is above 150 Hz; the share reads " << s.share() << ": the bins counted "
                                     "low end below 150 Hz");
    const auto a = sine(49.0, -15.0), b = sine(1000.0, -15.0, 48000.0, 1.1);
    std::vector<float> mix(a.size());
    for (std::size_t i = 0; i < mix.size(); ++i)
        mix[i] = a[i] + b[i];
    s.analyse(mix.data(), 10.0);
    CHECK_MESSAGE(s.share() == doctest::Approx(0.5).epsilon(0.04), "equal powers at 49 Hz and 1 kHz share half and "
                                                                   "half; the share reads " << s.share());
    s.reset();
    x = sine(49.0, -12.0);
    s.analyse(x.data(), 0.5);
    CHECK_MESSAGE(s.share() == doctest::Approx(1.0 - std::exp(-1.0)).epsilon(0.02),
                  "the share moves with a 500 ms time constant: from 0, half a second of a sub reads "
                  << 1.0 - std::exp(-1.0) << "; it reads " << s.share());
}

TEST_CASE("P6.07 a frame's analysis allocates nothing and takes at most 3 ms of CPU") {
    undertow::Spectrum s;
    s.setSampleRate(48000.0);
    Rng rng;
    const auto x = pink(rng);
    std::vector<double> ms;
    std::size_t allocations = 0;
    for (int i = 0; i < 40; ++i) {
        const double start = spec::threadSeconds();
        {
            spec::AllocationCounter counter;
            s.analyse(x.data(), 1.0 / 30.0);
            allocations += counter.count();
        }
        ms.push_back((spec::threadSeconds() - start) * 1000.0);
    }
    CHECK_MESSAGE(allocations == 0, "analyse() allocated " << allocations << " times; size every buffer in the "
                                    "constructor");
    std::sort(ms.begin(), ms.end());
    CHECK_MESSAGE(ms[ms.size() / 2] <= 3.0, "a frame takes " << ms[ms.size() / 2] << " ms of CPU (median); the window "
                                            "analyses 30 frames a second inside a 60 Hz budget: an iterative radix-2 "
                                            "FFT with precomputed twiddles and band edges fits in well under 3 ms");
}
