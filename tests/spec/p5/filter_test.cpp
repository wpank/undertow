// Phase 5 spec tests: the top of the resonance knob (docs/specs/phase-5-ship.md, P5.10).
// spec-needs: core/include/undertow/presets.h
// (The filter exists from phase 1; the line above only keeps this test out of tools/check.sh until phase 5 runs.)

#include "doctest.h"

#include "spec_analysis.h"
#include "undertow/filter.h"

#include <cmath>

namespace {

constexpr double kRate = 48000.0;

// The filter's gain in dB at its cutoff (1 kHz) for a quiet sine (0.01), drive 0.
double peakDb(double resonance) {
    undertow::LowpassFilter f;
    f.prepare(kRate);
    f.setCutoff(1000.0);
    f.setResonance(resonance);
    f.setDrive(0.0);
    f.reset();
    return spec::gainDb([&](float x) { return f.process(x); }, 1000.0, 0.01, kRate, 0.5, 0.25);
}

} // namespace

TEST_CASE("P5.10 the top of the resonance knob rings: at least +17 dB at the cutoff at 0.9 and +20 dB at 1") {
    // Phase 2.2 checked the middle of the knob (P2.2.11) and the full-level bound (P2.2.13) but left its top open: a
    // stage capped at Q 10 stops at about +14.5 dB, tamer than phase 1's law. The top is where acid lines scream.
    const double nearTop = peakDb(0.9);
    CHECK_MESSAGE(nearTop >= 17.0, "at resonance 0.9 a quiet tone at the cutoff gains " << nearTop
                                   << " dB; want +17 dB or more: don't cap the resonant stage's Q below the top of the knob");
    const double top = peakDb(1.0);
    CHECK_MESSAGE(top >= 20.0, "at resonance 1 a quiet tone at the cutoff gains " << top << " dB; want +20 dB or more");
}
