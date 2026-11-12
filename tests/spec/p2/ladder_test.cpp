// SPDX-License-Identifier: AGPL-3.0-only
// Phase 2 spec tests: the ladder low-pass (docs/specs/phase-2-sources.md, checks P2.10 to P2.17).
// spec-needs: core/include/undertow/ladder.h

#include "doctest.h"

#include "spec_alloc.h"
#include "spec_analysis.h"
#include "spec_engine.h"
#include "undertow/ladder.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr double kRate = 48000.0;

undertow::Ladder make(double cutoff, double resonance, double drive) {
    undertow::Ladder ladder;
    ladder.prepare(kRate);
    ladder.setCutoff(cutoff);
    ladder.setResonance(resonance);
    ladder.setDrive(drive);
    ladder.reset();
    return ladder;
}

// The steady-state gain at hz for a small sine (-40 dBFS), where the stages are linear.
double gain(double cutoff, double resonance, double hz, double drive = 0.0) {
    auto ladder = make(cutoff, resonance, drive);
    return spec::gainDb([&](float x) { return ladder.process(x); }, hz, 0.01, kRate, 0.5, 0.5);
}

struct Peak {
    double db = -300, hz = 0;
};

// The highest gain over a fine sweep from 0.5 to 1.5 times the cutoff.
Peak resonantPeak(double cutoff, double resonance) {
    Peak best;
    for (double r = 0.5; r <= 1.5; r += 0.01) {
        const double db = gain(cutoff, resonance, cutoff * r);
        if (db > best.db) {
            best.db = db;
            best.hz = cutoff * r;
        }
    }
    return best;
}

spec::Signal run(undertow::Ladder& ladder, const spec::Signal& in) {
    spec::Signal out(in.size());
    for (std::size_t n = 0; n < in.size(); ++n)
        out[n] = ladder.process(in[n]);
    return out;
}

} // namespace

TEST_CASE("P2.10 at resonance 0 the ladder is a 24 dB/octave low-pass: -12 dB at the cutoff") {
    const double fc = 500.0;
    const double dc = gain(fc, 0.0, 20.0);
    CHECK_MESSAGE(std::fabs(dc) <= 0.5, "the passband (20 Hz) gain is " << dc << " dB; want 0 dB");
    const double at = gain(fc, 0.0, fc), octave = gain(fc, 0.0, 2 * fc), two = gain(fc, 0.0, 4 * fc),
                 three = gain(fc, 0.0, 8 * fc);
    CHECK_MESSAGE(std::fabs(at + 12.0) <= 0.5, "the gain at the cutoff is " << at
                                                                            << " dB; four one-pole stages at the "
                                                                               "cutoff are -12 dB there "
                                                                               "(g = tan(pi fc / fs))");
    CHECK_MESSAGE(std::fabs(octave + 28.0) <= 1.0, "an octave above the cutoff the gain is " << octave
                                                                                             << " dB; want -28 dB");
    CHECK_MESSAGE(std::fabs(two + 49.2) <= 1.5, "two octaves above the cutoff the gain is " << two
                                                                                            << " dB; want -49 dB");
    const double slope = two - three;
    CHECK_MESSAGE(slope >= 23.0, "from 4 to 8 times the cutoff the gain falls " << slope
                                                                                << " dB; a 4-pole low-pass falls "
                                                                                   "23 to 25 dB there");
    CHECK_MESSAGE(slope <= 25.0, "from 4 to 8 times the cutoff the gain falls " << slope
                                                                                << " dB; a 4-pole low-pass falls "
                                                                                   "23 to 25 dB there");
    const double low = gain(100.0, 0.0, 100.0);
    CHECK_MESSAGE(std::fabs(low + 12.0) <= 0.5, "at a 100 Hz cutoff the gain at 100 Hz is " << low
                                                                                           << " dB; want -12 dB");
}

TEST_CASE("P2.11 resonance never thins the bass: below a quarter of the cutoff the gain stays within 1 dB") {
    const double fc = 500.0;
    const double base = gain(fc, 0.0, 60.0);
    for (const double r : {0.25, 0.5, 0.75, 1.0}) {
        const double g = gain(fc, r, 60.0);
        CHECK_MESSAGE(std::fabs(g - base) <= 1.0, "at resonance " << r << " the gain at 60 Hz (cutoff 500 Hz) is "
                                                                  << g << " dB against " << base
                                                                  << " dB at 0: scale the input by 1 + k so the "
                                                                     "feedback never takes the bass away");
    }
}

TEST_CASE("P2.12 resonance raises a peak just under the cutoff: +11 dB at the top of the knob, +5 dB at 0.6") {
    const double fc = 500.0;
    const double base = gain(fc, 0.0, 20.0);
    double previous = -300.0;
    for (const double r : {0.0, 0.2, 0.4, 0.6, 0.8, 1.0}) {
        const auto p = resonantPeak(fc, r);
        const double rise = p.db - base;
        INFO("resonance " << r << ": the peak is " << rise << " dB over the passband, at " << p.hz << " Hz");
        CHECK_MESSAGE(rise >= previous - 0.05, "the peak must rise steadily with resonance (k = 2.5 r)");
        previous = rise;
        if (r == 1.0) {
            CHECK_MESSAGE(rise >= 10.0, "at the top of the knob the peak must be +11 dB (within 1 dB): k = 2.5");
            CHECK_MESSAGE(rise <= 12.0, "at the top of the knob the peak must be +11 dB (within 1 dB): k = 2.5");
            CHECK_MESSAGE(p.hz >= 0.85 * fc, "the peak sits just under the cutoff");
            CHECK_MESSAGE(p.hz <= fc, "the peak sits just under the cutoff");
        }
        if (r == 0.6) {
            CHECK_MESSAGE(rise >= 4.0, "at resonance 0.6 the peak must be about +5 dB (4 to 6): the presets' ceiling");
            CHECK_MESSAGE(rise <= 6.0, "at resonance 0.6 the peak must be about +5 dB (4 to 6): the presets' ceiling");
        }
    }
}

TEST_CASE("P2.13 the ladder never self-oscillates: a ring dies within 12 cycles, and silence ends in zeros") {
    for (const double fc : {200.0, 1000.0}) {
        auto ladder = make(fc, 1.0, 0.0);
        spec::Signal impulse(static_cast<std::size_t>(kRate), 0.0f);
        impulse[0] = 0.01f;
        const auto h = run(ladder, impulse);
        const double peak = spec::peakAbs(h);
        const auto after = static_cast<std::size_t>(12.0 / fc * kRate);
        const double tail = spec::peakAbs(h, after);
        INFO("cutoff " << fc << " Hz, resonance 1");
        CHECK_MESSAGE(peak > 0.0, "the impulse response is silent");
        CHECK_MESSAGE(spec::toDb(tail / peak) <= -60.0, "12 cycles of the cutoff after an impulse its response still "
                                                        "reaches "
                                                            << spec::toDb(tail / peak)
                                                            << " dB of its peak; it must stay 60 dB under: at the top "
                                                               "of the knob the feedback is k = 2.5, short of the "
                                                               "self-oscillation at 4");
    }
    auto ladder = make(300.0, 1.0, 1.0);
    spec::Signal burst(static_cast<std::size_t>(kRate));
    std::uint32_t state = 7;
    for (auto& v : burst) {
        state = state * 1664525u + 1013904223u;
        v = static_cast<float>(static_cast<double>(state >> 8) / 8388608.0 - 1.0);
    }
    run(ladder, burst);
    const auto quiet = run(ladder, spec::Signal(static_cast<std::size_t>(kRate), 0.0f));
    CHECK_MESSAGE(spec::peakAbs(quiet, static_cast<std::size_t>(0.5 * kRate)) == 0.0,
                  "half a second after the input fell silent the output is still "
                      << spec::peakAbs(quiet, static_cast<std::size_t>(0.5 * kRate))
                      << ": flush the stages' state to exact zero, all together, once it is negligible");
    CHECK_MESSAGE(spec::countSubnormal(quiet) == 0, "the silent tail holds denormal samples");
}

TEST_CASE("P2.14 drive warms and never fuzzes: a 55 Hz sine at a 220 Hz cutoff") {
    // -6 dBFS, the cutoff four times the note, resonance 0. The drive's gain into the stages is 0.5 * 6^drive.
    const auto in = spec::sine(55.0, 0.5, static_cast<std::size_t>(2 * kRate), kRate);
    double base = 0;
    for (const double d : {0.0, 0.25, 0.5, 0.75, 1.0}) {
        auto ladder = make(220.0, 0.0, d);
        const auto out = run(ladder, in);
        const auto s = spec::spectrum(out, 24000, 1u << 16, kRate);
        const double fundamental = spec::amplitudeAt(s, 55.0);
        double strongest = 0;
        for (int h = 2; h <= 10; ++h)
            strongest = std::max(strongest, spec::amplitudeAt(s, 55.0 * h));
        const double harmonics = spec::toDb(strongest / fundamental);
        const double level = spec::toDb(spec::rms(out, 24000));
        if (d == 0.0)
            base = level;
        INFO("drive " << d << ": the strongest of harmonics 2 to 10 is " << harmonics << " dB under the fundamental");
        if (d == 0.0)
            CHECK_MESSAGE(harmonics <= -38.0, "at drive 0 the ladder is clean: harmonics 38 dB or more under (the "
                                              "gain into the stages is 0.5)");
        if (d == 1.0) {
            CHECK_MESSAGE(harmonics >= -30.0, "at the top of the knob the drive must be heard: the strongest "
                                              "harmonic 12 to 30 dB under (the gain into the stages is 3)");
            CHECK_MESSAGE(harmonics <= -12.0, "at the top of the knob the drive warms, never fuzzes: the strongest "
                                              "harmonic 12 to 30 dB under");
        }
        CHECK_MESSAGE(std::fabs(level - base) <= 4.0, "at drive " << d << " the level is " << level - base
                                                                  << " dB from drive 0's: divide the drive's gain back "
                                                                     "out after the stages");
    }
}

TEST_CASE("P2.15 the cutoff may move every sample: a sweeping filter stays smooth and stable") {
    // A filter decay at 1/16 and 130 BPM: from 2 kHz down to 100 Hz in 115 ms, again and again, at full resonance.
    auto ladder = make(100.0, 1.0, 0.4);
    const std::size_t step = static_cast<std::size_t>(0.115 * kRate);
    spec::Signal out(static_cast<std::size_t>(4 * kRate));
    double phase = 0;
    for (std::size_t n = 0; n < out.size(); ++n) {
        const double t = static_cast<double>(n % step) / static_cast<double>(step);
        ladder.setCutoff(100.0 * std::pow(20.0, 1.0 - t));
        phase += 55.0 / kRate;
        phase -= std::floor(phase);
        out[n] = ladder.process(phase < 0.5 ? 0.5f : -0.5f);
    }
    CHECK_MESSAGE(spec::allFinite(out), "the sweeping filter produced non-finite samples");
    CHECK_MESSAGE(spec::peakAbs(out) <= 2.0, "the sweeping filter peaked at " << spec::peakAbs(out)
                                                                              << " for a 0.5 square in");
    double jump = 0;
    for (std::size_t n = 1; n < out.size(); ++n)
        jump = std::max(jump, static_cast<double>(std::fabs(out[n] - out[n - 1])));
    CHECK_MESSAGE(jump <= 0.25, "the output jumped " << jump
                                                     << " in one sample while the cutoff moved: compute the stages "
                                                        "from the new coefficient every sample (zero-delay feedback), "
                                                        "never by resetting them");
}

TEST_CASE("P2.16 nothing a host sets makes the ladder blow up") {
    undertow::Ladder ladder;
    ladder.prepare(kRate);
    ladder.reset();
    std::uint32_t state = 99;
    auto next = [&]() {
        state = state * 1664525u + 1013904223u;
        return static_cast<double>(state >> 8) / 16777216.0;
    };
    double peak = 0;
    int nonFinite = 0;
    for (std::size_t n = 0; n < static_cast<std::size_t>(10 * kRate); ++n) {
        if (n % 32 == 0) {
            ladder.setCutoff(20.0 * std::pow(400.0, next()));
            ladder.setResonance(next());
            ladder.setDrive(next());
        }
        const float y = ladder.process(static_cast<float>(4.0 * (next() - 0.5)));
        nonFinite += std::isfinite(y) ? 0 : 1;
        peak = std::max(peak, static_cast<double>(std::fabs(y)));
    }
    CHECK_MESSAGE(nonFinite == 0, nonFinite << " non-finite samples under random settings");
    CHECK_MESSAGE(peak <= 16.0, "under random settings and a +-2 input the ladder peaked at " << peak);
    // A cutoff out of range is clamped to 20 Hz .. 0.45 sampleRate, never used as it is.
    const double low = gain(1.0, 0.0, 20.0), high = gain(1e6, 0.0, 1000.0), zero = gain(0.0, 0.0, 20.0);
    CHECK_MESSAGE(std::fabs(low + 12.0) <= 1.0, "a 1 Hz cutoff must act as 20 Hz: -12 dB at 20 Hz; got " << low);
    CHECK_MESSAGE(std::fabs(zero + 12.0) <= 1.0, "a 0 Hz cutoff must act as 20 Hz: -12 dB at 20 Hz; got " << zero);
    CHECK_MESSAGE(std::fabs(high) <= 0.5, "a 1 MHz cutoff must act as 0.45 sampleRate: 0 dB at 1 kHz; got " << high);
}

TEST_CASE("P2.17 the ladder never allocates, and runs at least 30 times faster than real time") {
    undertow::Ladder ladder;
    ladder.prepare(kRate);
    ladder.setResonance(0.5);
    ladder.setDrive(0.4);
    ladder.reset();
    double sink = 0;
    spec::AllocationCounter counter;
    const double start = spec::threadSeconds();
    for (std::size_t n = 0; n < static_cast<std::size_t>(60 * kRate); ++n) {
        ladder.setCutoff(140.0 * (1.0 + static_cast<double>(n % 5520) / 1380.0));
        sink += ladder.process(static_cast<float>((n / 245) % 2 == 0 ? 0.5 : -0.5));
    }
    const double seconds = spec::threadSeconds() - start;
    const std::size_t allocations = counter.count(); // read before CHECK: doctest's first assertion allocates
    CHECK_MESSAGE(allocations == 0, allocations << " heap allocations in the ladder");
    CHECK_MESSAGE(std::isfinite(sink), "the output was not finite");
    CHECK_MESSAGE(seconds <= 2.0, "60 s through the ladder, its cutoff moving every sample, took "
                                      << seconds << " s of CPU; the budget is 2 s");
}
