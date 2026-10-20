// Phase 2.1 spec tests: the filter keeps its resonance and its linearity at the levels the voice plays
// (docs/specs/phase-2.1-voice-fixes.md, P2.1.05 and P2.1.06).
// spec-needs: core/include/undertow/filter.h

#include "doctest.h"

#include "spec_analysis.h"
#include "undertow/filter.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kRate = 48000.0;

undertow::LowpassFilter make(double cutoff, double resonance) {
    undertow::LowpassFilter f;
    f.prepare(kRate);
    f.setCutoff(cutoff);
    f.setResonance(resonance);
    f.setDrive(0.0);
    f.reset();
    return f;
}

// The filter's gain in dB at hz for a sine of the given amplitude, cutoff 1 kHz, drive 0.
double gainAt(double resonance, double hz, double amplitude) {
    auto f = make(1000.0, resonance);
    return spec::gainDb([&](float x) { return f.process(x); }, hz, amplitude, kRate, 0.25, 0.25);
}

// How far a full-scale 110 Hz saw through cutoff 440 Hz is from 100 times the same saw at 0.01, through a second
// filter set alike: 10 log10(error power / signal power), from 0.2 s on. A linear filter gives float rounding only.
double deviationDb(double resonance) {
    auto full = make(440.0, resonance);
    auto quiet = make(440.0, resonance);
    double error = 0, power = 0;
    for (int n = 0; n < 48000; ++n) {
        const double saw = 2.0 * std::fmod(n * 110.0 / kRate, 1.0) - 1.0;
        const double a = full.process(static_cast<float>(saw));
        const double b = 100.0 * static_cast<double>(quiet.process(static_cast<float>(0.01 * saw)));
        if (n >= 9600) {
            error += (a - b) * (a - b);
            power += b * b;
        }
    }
    return 10.0 * std::log10(std::max(error, 1e-300) / power);
}

} // namespace

TEST_CASE("P2.1.05 resonance keeps its peak at full level: a 1.0 sine at the cutoff gains what a 0.1 sine does, within 3 dB") {
    const double quiet = gainAt(0.8, 1000.0, 0.1);
    const double full = gainAt(0.8, 1000.0, 1.0);
    const double flat = gainAt(0.0, 1000.0, 1.0);
    CHECK_MESSAGE(std::fabs(full - quiet) <= 3.0, "at resonance 0.8 the gain at the cutoff is " << quiet << " dB for a 0.1 sine "
                                                  "but " << full << " dB for a 1.0 sine: the filter flattens its own "
                                                  "resonance at playing levels. Limit its state only beyond the levels "
                                                  "the voice reaches");
    CHECK_MESSAGE(full >= flat + 6.0, "at full level the resonant peak (" << full << " dB at resonance 0.8) must stand 6 dB "
                                     "over the gain at resonance 0 (" << flat << " dB), as P1.06 asks at a small level");
}

TEST_CASE("P2.1.06 at drive 0 the filter is linear at full level: a full-scale saw comes out as 100 times the same saw at 0.01") {
    for (const double resonance : {0.0, 0.35, 0.8}) {
        const double deviation = deviationDb(resonance);
        CHECK_MESSAGE(deviation <= -60.0, "at resonance " << resonance << " and drive 0 a full-scale 110 Hz saw through cutoff "
                                          "440 Hz is " << deviation << " dB from linear (want -60 or less): drive 0 must be "
                                          "exactly linear at the levels the voice plays");
    }
}
