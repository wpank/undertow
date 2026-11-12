// SPDX-License-Identifier: AGPL-3.0-only
// Phase 2 spec tests: the oscillators, the locked sine sub and the noise (docs/specs/phase-2-sources.md, checks P2.01
// to P2.09).
// spec-needs: core/include/undertow/vco.h
// spec-needs: core/include/undertow/noise.h

#include "doctest.h"

#include "spec_alloc.h"
#include "spec_analysis.h"
#include "spec_engine.h"
#include "undertow/noise.h"
#include "undertow/vco.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace {

constexpr double kRate = 48000.0;
constexpr double kPi = 3.14159265358979323846;

std::string name(undertow::Wave wave) { return wave == undertow::Wave::Square ? "square" : "triangle"; }

spec::Signal play(undertow::Vco& vco, double hz, std::size_t samples) {
    vco.setFrequency(hz);
    spec::Signal out(samples);
    for (auto& v : out)
        v = vco.process();
    return out;
}

spec::Signal tone(undertow::Wave wave, double hz, std::size_t samples) {
    undertow::Vco vco;
    vco.prepare(kRate);
    vco.setWave(wave);
    vco.reset();
    return play(vco, hz, samples);
}

// The strongest spectral component below hiHz that isn't within guardBins of a harmonic of f0, in dB under the
// fundamental.
double worstNonHarmonicDb(const spec::Spectrum& s, double f0, double hiHz, double guardBins) {
    double worst = 0;
    for (std::size_t k = 1; k < s.amplitude.size(); ++k) {
        const double hz = static_cast<double>(k) * s.binHz();
        if (hz > hiHz)
            break;
        const double harmonic = std::max(1.0, std::round(hz / f0));
        if (std::fabs(hz - harmonic * f0) <= guardBins * s.binHz())
            continue;
        worst = std::max(worst, s.amplitude[k]);
    }
    return spec::toDb(worst / spec::amplitudeAt(s, f0));
}

double triangleAt(double phase) {
    return phase < 0.25 ? 4.0 * phase : (phase < 0.75 ? 2.0 - 4.0 * phase : 4.0 * phase - 4.0);
}

} // namespace

TEST_CASE("P2.01 the oscillators play the frequency they are given, within 0.1%") {
    for (const auto wave : {undertow::Wave::Triangle, undertow::Wave::Square})
        for (const double hz : {49.0, 98.0, 523.25, 1234.5}) {
            const auto out = tone(wave, hz, static_cast<std::size_t>(2 * kRate));
            const double got = spec::zeroCrossingHz(out, 4800, out.size(), kRate);
            INFO(name(wave) << " at " << hz << " Hz");
            CHECK_MESSAGE(std::fabs(got / hz - 1.0) <= 0.001, "it plays " << got << " Hz; want " << hz
                                                                          << " within 0.1%: the phase advances by "
                                                                             "frequency / sampleRate every sample");
        }
}

TEST_CASE("P2.02 the waves: a triangle's and a square's odd harmonics, at full scale, aligned with a sine") {
    // 150 bins of a 65536-point spectrum (about 110 Hz): the fundamental and its harmonics fall on bin centres, where
    // the spectrum reads amplitudes exactly.
    const double hz = 150.0 * kRate / 65536.0;
    for (const auto wave : {undertow::Wave::Triangle, undertow::Wave::Square}) {
        const auto out = tone(wave, hz, 1u << 17);
        const auto s = spec::spectrum(out, 4096, 1u << 16, kRate);
        const double fundamental = spec::amplitudeAt(s, hz);
        const bool square = wave == undertow::Wave::Square;
        INFO(name(wave) << " at " << hz << " Hz");
        const double want = square ? 4.0 / kPi : 8.0 / (kPi * kPi);
        CHECK_MESSAGE(std::fabs(fundamental / want - 1.0) <= 0.01,
                      "its fundamental is " << fundamental << "; a full-scale (-1 to +1) " << name(wave) << " has "
                                            << want);
        for (int h : {3, 5, 7}) {
            const double db = spec::toDb(spec::amplitudeAt(s, hz * h) / fundamental);
            const double ideal = spec::toDb(square ? 1.0 / h : 1.0 / (h * h));
            CHECK_MESSAGE(std::fabs(db - ideal) <= 0.3, "harmonic " << h << " is at " << db << " dB; a "
                                                                    << name(wave) << "'s is at " << ideal << " dB");
        }
        for (int h : {2, 4, 6}) {
            const double db = spec::toDb(spec::amplitudeAt(s, hz * h) / fundamental);
            CHECK_MESSAGE(db <= -50.0, "even harmonic " << h << " is at " << db
                                                        << " dB: a symmetric wave has only odd harmonics");
        }
        // Aligned with sin(2 pi phase): the wave's fundamental is in phase with the sine of the same frequency.
        const auto reference = spec::sine(hz, 1.0, out.size(), kRate);
        const double c = spec::correlation(out, reference, 0, out.size());
        CHECK_MESSAGE(c >= (square ? 0.85 : 0.98),
                      "the " << name(wave) << " correlates " << c << " with sin(2 pi f t) from the same start: "
                             << (square ? "the square is +1 on [0, 0.5) and -1 on [0.5, 1)"
                                        : "the triangle is 0 at phase 0, +1 at 0.25 and -1 at 0.75"));
    }
}

TEST_CASE("P2.03 the waves stay clear of aliases: 60 dB under at C5, and 50 dB at 1234.5 Hz") {
    for (const auto wave : {undertow::Wave::Triangle, undertow::Wave::Square}) {
        INFO(name(wave));
        const auto c5 = tone(wave, 523.25, 1u << 17);
        const auto s = spec::spectrum(c5, 4096, 1u << 16, kRate);
        const double worst = worstNonHarmonicDb(s, 523.25, 5000.0, 6.0);
        CHECK_MESSAGE(worst <= -60.0, "at C5 (523.25 Hz) the strongest component below 5 kHz that is not a "
                                      "harmonic is "
                                          << worst << " dB under the fundamental; want -60 dB or less: correct the "
                                                      "square's two edges with a two-sample PolyBLEP");
        const auto high = tone(wave, 1234.5, 1u << 17);
        const auto t = spec::spectrum(high, 4096, 1u << 16, kRate);
        const double between = 10.0 * std::log10(spec::powerAwayFromHarmonics(t, 1234.5, 20.0, 4000.0, 6.0) /
                                                  std::pow(spec::amplitudeAt(t, 1234.5), 2));
        CHECK_MESSAGE(between <= -50.0, "at 1234.5 Hz the power between the harmonics below 4 kHz is "
                                            << between << " dB under the fundamental; want -50 dB or less");
    }
}

TEST_CASE("P2.04 reset starts the phase where it is told, the same way every time") {
    for (const auto wave : {undertow::Wave::Triangle, undertow::Wave::Square}) {
        INFO(name(wave));
        undertow::Vco vco;
        vco.prepare(kRate);
        vco.setWave(wave);
        vco.reset();
        const auto first = play(vco, 98.0, 9600);
        vco.reset(0.0);
        const auto second = play(vco, 98.0, 9600);
        CHECK_MESSAGE(first == second, "two plays from reset() differ: reset must clear everything the wave depends on");
        vco.reset(0.25);
        CHECK_MESSAGE(vco.phase() == doctest::Approx(0.25), "after reset(0.25) phase() must be 0.25; got "
                                                                << vco.phase());
        vco.setFrequency(98.0);
        (void)vco.process();
        CHECK_MESSAGE(vco.triangle() == doctest::Approx(1.0).epsilon(1e-6),
                      "the first process() after reset(0.25) plays phase 0.25, where the triangle is +1; got "
                          << vco.triangle());
        CHECK_MESSAGE(vco.sub(0) == doctest::Approx(1.0).epsilon(1e-6),
                      "and where the sub, sin(2 pi phase), is +1; got " << vco.sub(0));
        CHECK_MESSAGE(vco.phase() == doctest::Approx(0.25 + 98.0 / kRate).epsilon(1e-12),
                      "after one process() the phase has moved on by frequency / sampleRate");
    }
    undertow::Vco vco;
    vco.prepare(kRate);
    vco.reset(1.75);
    CHECK_MESSAGE(vco.phase() == doctest::Approx(0.75), "reset(1.75) wraps the phase into [0, 1): 0.75; got "
                                                            << vco.phase());
    // An odd number of cycles, then a reset: the octave-under sine starts again from its even half.
    vco.reset();
    play(vco, 1000.0, 48 * 3 + 10);
    vco.reset(0.1);
    vco.setFrequency(98.0);
    (void)vco.process();
    CHECK_MESSAGE(vco.sub(-1) == doctest::Approx(std::sin(kPi * 0.1)).epsilon(1e-6),
                  "after reset(0.1) sub(-1) must be sin(pi * 0.1), the first half of its cycle; got "
                      << vco.sub(-1) << ": reset() clears the count of cycles too");
}

TEST_CASE("P2.05 per-sample frequency changes: the phase never runs backwards, and triangle() is the modulator") {
    // VCO 2 under linear FM from VCO 1's triangle, as the voice will drive it: f2 * (1 + fm * tri1), fm = 1.
    undertow::Vco vco1, vco2;
    for (auto* v : {&vco1, &vco2}) {
        v->prepare(kRate);
        v->reset();
    }
    vco1.setWave(undertow::Wave::Square);
    vco2.setWave(undertow::Wave::Square);
    vco1.setFrequency(55.0);
    const std::size_t samples = static_cast<std::size_t>(2 * kRate);
    spec::Signal out(samples);
    int wraps = 0, backwards = 0, wrongTriangle = 0;
    double lowest = 1e9, highest = 0;
    for (std::size_t n = 0; n < samples; ++n) {
        const double played1 = vco1.phase();
        (void)vco1.process();
        if (std::fabs(vco1.triangle() - triangleAt(played1)) > 1e-6)
            ++wrongTriangle;
        const double f2 = 110.0 * (1.0 + 1.0 * vco1.triangle());
        lowest = std::min(lowest, f2);
        highest = std::max(highest, f2);
        vco2.setFrequency(f2);
        const double before = vco2.phase();
        out[n] = vco2.process();
        double step = vco2.phase() - before;
        if (step < -0.5) { // the phase wrapped from near 1 to near 0
            step += 1.0;
            ++wraps;
        }
        if (step < -1e-12)
            ++backwards;
    }
    CHECK_MESSAGE(wrongTriangle == 0, wrongTriangle << " samples where triangle() was not the triangle at the phase "
                                                       "process() played: it is the FM modulator whatever the wave");
    CHECK_MESSAGE(backwards == 0, "the phase ran backwards " << backwards << " times");
    CHECK_MESSAGE(lowest >= 0.0, "the modulated frequency went to " << lowest << " Hz");
    CHECK_MESSAGE(std::abs(wraps - 220) <= 1, "over 2 s VCO 2 made " << wraps
                                                                      << " cycles; linear FM keeps the carrier's "
                                                                         "mean frequency, 110 Hz: 220 cycles");
    CHECK_MESSAGE(spec::peakAbs(out) <= 1.2, "under FM the band-limited square peaked at " << spec::peakAbs(out)
                                                                                            << ": correct each edge "
                                                                                               "with that sample's "
                                                                                               "own phase increment");
    undertow::Vco still;
    still.prepare(kRate);
    still.reset(0.3);
    still.setFrequency(-10.0);
    (void)still.process();
    CHECK_MESSAGE(still.phase() == doctest::Approx(0.3), "a negative frequency is clamped to 0 Hz: the phase must "
                                                         "stay at 0.3; got "
                                                             << still.phase());
}

TEST_CASE("P2.06 the sub is a pure sine at the VCO's pitch or an octave under, locked to its phase") {
    undertow::Vco vco;
    vco.prepare(kRate);
    vco.reset();
    std::uint32_t random = 12345;
    int parity = 0, wrongSame = 0, wrongUnder = 0;
    double hz = 98.0;
    for (std::size_t n = 0; n < static_cast<std::size_t>(10 * kRate); ++n) {
        if (n % 64 == 0) {
            random = random * 1664525u + 1013904223u;
            hz = 20.0 * std::pow(100.0, static_cast<double>(random >> 8) / 16777216.0); // 20 Hz to 2 kHz
        }
        vco.setFrequency(hz);
        const double played = vco.phase();
        (void)vco.process();
        if (std::fabs(vco.sub(0) - std::sin(2.0 * kPi * played)) > 1e-5)
            ++wrongSame;
        if (std::fabs(vco.sub(-1) - std::sin(kPi * (parity + played))) > 1e-5)
            ++wrongUnder;
        if (vco.phase() < played)
            parity ^= 1;
    }
    CHECK_MESSAGE(wrongSame == 0, wrongSame << " samples where sub(0) was not sin(2 pi phase) at the phase "
                                               "process() played");
    CHECK_MESSAGE(wrongUnder == 0, wrongUnder << " samples where sub(-1) was not sin(pi (cycles mod 2 + phase)): the "
                                                 "octave-under sine is locked to the VCO, through every frequency "
                                                 "change, never a second free-running phase");
    undertow::Vco steady;
    steady.prepare(kRate);
    steady.reset();
    steady.setFrequency(98.0);
    spec::Signal under(1u << 17);
    for (auto& v : under) {
        (void)steady.process();
        v = steady.sub(-1);
    }
    const auto s = spec::spectrum(under, 0, 1u << 17, kRate);
    CHECK_MESSAGE(spec::peakHz(s, 30.0, 70.0) == doctest::Approx(49.0).epsilon(0.002),
                  "sub(-1) of a 98 Hz VCO must sound 49 Hz; it peaks at " << spec::peakHz(s, 30.0, 70.0) << " Hz");
    CHECK_MESSAGE(spec::toDb(spec::thd(s, 49.0, 10)) <= -60.0, "the sub's harmonics 2 to 10 are at "
                                                                   << spec::toDb(spec::thd(s, 49.0, 10))
                                                                   << " dB: it must be a pure sine");
    CHECK_MESSAGE(spec::peakAbs(under) == doctest::Approx(1.0).epsilon(0.001), "the sub is a full-scale sine");
}

TEST_CASE("P2.07 at or above half the sample rate the wave is silent") {
    for (const auto wave : {undertow::Wave::Triangle, undertow::Wave::Square})
        for (const double hz : {24000.0, 30000.0}) {
            const auto out = tone(wave, hz, 4800);
            INFO(name(wave) << " at " << hz << " Hz");
            CHECK_MESSAGE(spec::peakAbs(out) == 0.0, "at or above sampleRate / 2 the wave must be 0 (it would alias); "
                                                     "it peaked at "
                                                         << spec::peakAbs(out));
        }
    const auto below = tone(undertow::Wave::Square, 23000.0, 4800);
    CHECK_MESSAGE(spec::peakAbs(below) <= 1.5, "just under half the sample rate the square must stay bounded");
}

TEST_CASE("P2.08 the noise is white, RMS 0.5, and the same after every reset") {
    undertow::Noise noise;
    noise.reset();
    spec::Signal x(static_cast<std::size_t>(4 * kRate));
    for (auto& v : x)
        v = noise.process();
    CHECK_MESSAGE(std::fabs(spec::rms(x) - 0.5) <= 0.02, "the noise's RMS is " << spec::rms(x) << "; want 0.5");
    CHECK_MESSAGE(std::fabs(spec::mean(x)) <= 0.01, "the noise's mean is " << spec::mean(x) << "; want 0");
    CHECK_MESSAGE(spec::peakAbs(x) <= 1.0, "the noise peaked at " << spec::peakAbs(x) << "; keep it within 1");
    // Flat: the power per hertz of each octave band from 100 Hz to 12.8 kHz, averaged over 16 spectra.
    std::vector<double> density(7, 0.0);
    for (int k = 0; k < 16; ++k) {
        const auto s = spec::spectrum(x, static_cast<std::size_t>(k) * 8192, 8192, kRate);
        for (int b = 0; b < 7; ++b) {
            const double lo = 100.0 * std::pow(2.0, b), hi = 2 * lo;
            density[static_cast<std::size_t>(b)] += spec::bandPower(s, lo, hi) / (hi - lo);
        }
    }
    const double middle = density[3];
    for (int b = 0; b < 7; ++b) {
        const double db = 10.0 * std::log10(density[static_cast<std::size_t>(b)] / middle);
        CHECK_MESSAGE(std::fabs(db) <= 2.0, "the octave from " << 100.0 * std::pow(2.0, b) << " Hz is " << db
                                                               << " dB from the 800 Hz octave: white noise is flat "
                                                                  "within 2 dB");
    }
    const double lag = spec::correlation(spec::Signal(x.begin(), x.end() - 1), spec::Signal(x.begin() + 1, x.end()));
    CHECK_MESSAGE(std::fabs(lag) <= 0.02, "neighbouring samples correlate " << lag << ": white noise has none");
    undertow::Noise other;
    other.reset();
    noise.reset();
    int different = 0;
    for (int n = 0; n < 48000; ++n)
        different += noise.process() == other.process() ? 0 : 1;
    CHECK_MESSAGE(different == 0, "two Noise objects after reset() differ in " << different
                                                                               << " samples: every reset() starts "
                                                                                  "the same sequence");
}

TEST_CASE("P2.09 the oscillators and the noise never allocate, and run at least 40 times faster than real time") {
    undertow::Vco vco1, vco2;
    undertow::Noise noise;
    for (auto* v : {&vco1, &vco2})
        v->prepare(kRate);
    vco2.setWave(undertow::Wave::Square);
    const std::size_t samples = static_cast<std::size_t>(60 * kRate);
    double sink = 0;
    spec::AllocationCounter counter;
    const double start = spec::threadSeconds();
    vco1.reset();
    vco2.reset();
    noise.reset();
    for (std::size_t n = 0; n < samples; ++n) {
        vco1.setFrequency(98.0 * (1.0 + 0.001 * static_cast<double>(n % 480)));
        const float a = vco1.process();
        vco2.setFrequency(49.0 * (1.0 + 0.3 * vco1.triangle()));
        sink += a + vco2.process() + vco1.sub(-1) + noise.process();
    }
    const double seconds = spec::threadSeconds() - start;
    const std::size_t allocations = counter.count(); // read before CHECK: doctest's first assertion allocates
    CHECK_MESSAGE(allocations == 0, allocations << " heap allocations in the oscillators or the noise");
    CHECK_MESSAGE(std::isfinite(sink), "the output was not finite");
    CHECK_MESSAGE(seconds <= 1.5, "60 s of two oscillators, the sub and the noise took " << seconds
                                                                                       << " s of CPU; the budget is "
                                                                                          "1.5 s");
}
