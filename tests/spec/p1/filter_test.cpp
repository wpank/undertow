// Phase 1 spec tests: the 24 dB/octave low-pass with resonance and drive (docs/specs/phase-1-voice.md, P1.05 to P1.07).
// spec-needs: core/include/undertow/filter.h

#include "doctest.h"

#include "spec_analysis.h"
#include "undertow/filter.h"

#include <cmath>

namespace {

constexpr double kRate = 48000.0;

undertow::LowpassFilter make(double cutoff, double resonance, double drive) {
    undertow::LowpassFilter f;
    f.prepare(kRate);
    f.setCutoff(cutoff);
    f.setResonance(resonance);
    f.setDrive(drive);
    f.reset();
    return f;
}

double gainAt(double cutoff, double resonance, double hz) {
    auto f = make(cutoff, resonance, 0.0);
    return spec::gainDb([&](float x) { return f.process(x); }, hz, 0.01, kRate, 0.25, 0.25);
}

} // namespace

TEST_CASE("P1.05 at resonance 0 the filter is a 24 dB/octave low-pass: -3 dB at the cutoff") {
    for (double fc : {200.0, 1000.0}) {
        INFO("cutoff " << fc << " Hz");
        const double atCutoff = gainAt(fc, 0.0, fc);
        CHECK_MESSAGE(std::fabs(atCutoff + 3.0) <= 1.5,
                      "the gain at the cutoff is " << atCutoff << " dB; it must be -3 dB +- 1.5 (the cutoff is the -3 dB "
                                                      "point: a cutoff off by 2x reads about -24 dB or 0 dB here)");
        const double passband = gainAt(fc, 0.0, fc / 4.0);
        CHECK_MESSAGE(std::fabs(passband) <= 1.0, "the gain two octaves below the cutoff is " << passband << " dB; want 0 +- 1");
        const double octave = gainAt(fc, 0.0, fc * 2.0);
        CHECK_MESSAGE(octave <= -20.0, "the gain an octave above the cutoff is " << octave
                                                                                 << " dB; a 24 dB/octave slope gives about -24 "
                                                                                    "(a 12 dB/octave filter gives -12)");
        const double twoOctaves = gainAt(fc, 0.0, fc * 4.0);
        CHECK_MESSAGE(twoOctaves <= -40.0, "the gain two octaves above the cutoff is " << twoOctaves << " dB; want -40 or less");
    }
}

TEST_CASE("P1.06 resonance raises a peak at the cutoff") {
    const double fc = 1000.0;
    const double flat = gainAt(fc, 0.0, fc);
    const double resonant = gainAt(fc, 0.8, fc);
    CHECK_MESSAGE(resonant >= flat + 6.0, "at resonance 0.8 the gain at the cutoff is " << resonant << " dB against " << flat
                                                                                       << " dB at resonance 0; it must be at "
                                                                                          "least 6 dB higher");
    double peak = -300.0, peakHz = 0.0;
    for (double hz = 600.0; hz <= 1600.0; hz += 20.0) {
        const double g = gainAt(fc, 0.8, hz);
        if (g > peak) {
            peak = g;
            peakHz = hz;
        }
    }
    const double low = gainAt(fc, 0.8, fc / 8.0);
    CHECK_MESSAGE(peak >= low + 6.0, "at resonance 0.8 the peak (" << peak << " dB) must stand 6 dB over the passband ("
                                                                  << low << " dB)");
    CHECK_MESSAGE(std::fabs(peakHz - fc) <= 0.2 * fc, "the resonant peak is at " << peakHz << " Hz; it must sit within 20% of the cutoff");
}

TEST_CASE("P1.07 drive adds odd harmonics, and nothing makes the filter blow up or leave denormals") {
    auto distortion = [](double drive) {
        auto f = make(5000.0, 0.0, drive);
        spec::Signal out(1u << 16);
        const auto in = spec::sine(100.0, 0.5, out.size() + 4800, kRate);
        for (std::size_t n = 0; n < 4800; ++n)
            f.process(in[n]);
        for (std::size_t n = 0; n < out.size(); ++n)
            out[n] = f.process(in[n + 4800]);
        return spec::spectrum(out, 0, out.size(), kRate);
    };
    const auto clean = distortion(0.0);
    CHECK_MESSAGE(spec::toDb(spec::thd(clean, 100.0, 10)) <= -60.0,
                  "at drive 0 the filter must be linear: THD " << spec::toDb(spec::thd(clean, 100.0, 10)) << " dB, want -60 or less");
    const auto driven = distortion(1.0);
    const double thdDriven = spec::toDb(spec::thd(driven, 100.0, 10));
    CHECK_MESSAGE(thdDriven >= -30.0, "at drive 1 a 0.5 sine must come out distorted: THD " << thdDriven << " dB, want -30 or more");
    const double second = spec::amplitudeAt(driven, 200.0), third = spec::amplitudeAt(driven, 300.0);
    CHECK_MESSAGE(spec::toDb(third / std::max(second, 1e-12)) >= 20.0,
                  "drive must be a symmetric saturation (odd harmonics): the 3rd harmonic is only "
                      << spec::toDb(third / std::max(second, 1e-12)) << " dB over the 2nd");

    // The worst case: full resonance and drive, a full-scale saw, the cutoff swept 20 Hz to 18 kHz and back
    // every quarter second, for 10 s.
    auto f = make(1000.0, 1.0, 1.0);
    double peak = 0.0;
    int nonFinite = 0;
    for (int n = 0; n < 480000; ++n) {
        const double t = std::fmod(n / kRate, 0.25) / 0.25;
        const double sweep = t < 0.5 ? t * 2.0 : 2.0 - t * 2.0;
        f.setCutoff(20.0 * std::pow(900.0, sweep));
        const float in = static_cast<float>(2.0 * std::fmod(n * 55.0 / kRate, 1.0) - 1.0);
        const float y = f.process(in);
        nonFinite += std::isfinite(y) ? 0 : 1;
        if (std::isfinite(y))
            peak = std::max(peak, static_cast<double>(std::fabs(y)));
    }
    CHECK_MESSAGE(nonFinite == 0, nonFinite << " non-finite samples under a swept cutoff at full resonance and drive");
    CHECK_MESSAGE(peak <= 4.0, "the output reached " << peak << " under a swept cutoff at full resonance and drive; it must "
                                                       "stay within +-4");

    // Ring the filter hard, then feed it silence: its state must decay away, never through denormals.
    auto ring = make(1000.0, 1.0, 0.0);
    for (int n = 0; n < 48000; ++n)
        ring.process(static_cast<float>(2.0 * std::fmod(n * 110.0 / kRate, 1.0) - 1.0));
    spec::Signal tail(20 * 48000);
    for (auto& v : tail)
        v = ring.process(0.0f);
    CHECK_MESSAGE(spec::countSubnormal(tail) == 0,
                  spec::countSubnormal(tail) << " denormal samples in 20 s of silence after a resonant ring: flush filter state "
                                                "below about 1e-20 to 0");
    CHECK_MESSAGE(spec::allFinite(tail), "the tail after a resonant ring is not finite");
    CHECK_MESSAGE(spec::peakAbs(tail, tail.size() - 48000) <= 1e-9,
                  "after 19 s of silence the filter still outputs " << spec::peakAbs(tail, tail.size() - 48000)
                                                                    << ": its state must decay to (near) zero");
}
