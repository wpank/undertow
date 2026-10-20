// Phase 2.2 spec tests: the filter's drive colours without turning the voice down, its resonance has a musical middle, and
// a held tone at the cutoff stays bounded (docs/specs/phase-2.2-gain-staging.md, P2.2.03, P2.2.11 and P2.2.13).
// spec-needs: core/include/undertow/filter.h

#include "doctest.h"

#include "spec_analysis.h"
#include "undertow/filter.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kRate = 48000.0;

// The RMS of a 110 Hz saw at 0.6 (the default osc_level) through the filter at the default cutoff (600 Hz) and
// resonance (0.35), from 0.5 s on.
double sawRms(double drive) {
    undertow::LowpassFilter f;
    f.prepare(kRate);
    f.setCutoff(600.0);
    f.setResonance(0.35);
    f.setDrive(drive);
    f.reset();
    spec::Signal out(96000);
    for (std::size_t n = 0; n < out.size(); ++n)
        out[n] = f.process(static_cast<float>(0.6 * (2.0 * std::fmod(static_cast<double>(n) * 110.0 / kRate, 1.0) - 1.0)));
    return spec::rms(out, 24000);
}

// The filter's gain in dB at its cutoff (1 kHz) for a quiet sine (0.01), drive 0.
double peakDb(double resonance) {
    undertow::LowpassFilter f;
    f.prepare(kRate);
    f.setCutoff(1000.0);
    f.setResonance(resonance);
    f.setDrive(0.0);
    f.reset();
    return spec::gainDb([&](float x) { return f.process(x); }, 1000.0, 0.01, kRate, 0.25, 0.25);
}

} // namespace

TEST_CASE("P2.2.03 filter_drive keeps the voice's level: a 0.6 saw stays within 3 dB of drive 0 at every drive") {
    const double clean = sawRms(0.0);
    for (const double drive : {0.25, 0.5, 0.75, 1.0}) {
        const double change = spec::toDb(sawRms(drive) / clean);
        CHECK_MESSAGE(std::fabs(change) <= 3.0, "at filter_drive " << drive << " a 110 Hz saw at the default osc_level (0.6) "
                                                "comes out " << change << " dB from drive 0: give the drive a makeup gain, so "
                                                "turning it up colours the sound instead of changing its level");
    }
}

TEST_CASE("P2.2.11 resonance has a musical middle: at least +8 dB at the cutoff at 0.6, rising steadily across the knob") {
    // Phase 1's law moves one stage's damping linearly, which leaves the middle of the knob tame (+4.6 dB at 0.6) and
    // crowds the resonance into its top fifth. Hypnotic techno lives in the middle.
    const double middle = peakDb(0.6);
    CHECK_MESSAGE(middle >= 8.0, "at resonance 0.6 the gain at the cutoff is " << middle << " dB; want +8 dB or more");
    double previous = peakDb(0.0);
    for (const double resonance : {0.2, 0.35, 0.5, 0.6, 0.7, 0.8, 0.9}) {
        const double gain = peakDb(resonance);
        CHECK_MESSAGE(gain >= previous + 0.5, "the peak must rise steadily with resonance: " << gain << " dB at " << resonance
                                              << " after " << previous << " dB");
        previous = gain;
    }
}

TEST_CASE("P2.2.13 a full-scale tone held at the cutoff stays within 4, at any resonance") {
    // P1.07 bounds a swept cutoff at full drive, which never rings the filter up for long. A note held on the cutoff at
    // high resonance does: the output needs a final limit near the levels P1.07 allows.
    for (const double cutoff : {600.0, 1000.0})
        for (const double resonance : {0.8, 0.9, 1.0}) {
            undertow::LowpassFilter f;
            f.prepare(kRate);
            f.setCutoff(cutoff);
            f.setResonance(resonance);
            f.setDrive(0.0);
            f.reset();
            double peak = 0;
            for (int n = 0; n < 96000; ++n) {
                const float y = f.process(static_cast<float>(std::sin(2.0 * 3.14159265358979323846 * cutoff * n / kRate)));
                if (n >= 48000)
                    peak = std::max(peak, static_cast<double>(std::fabs(y)));
            }
            CHECK_MESSAGE(peak <= 4.0, "a 1.0 sine held at the cutoff (" << cutoff << " Hz) at resonance " << resonance
                                       << " peaks at " << peak << ": limit the filter's output to within 4");
        }
}
