// Phase 2.2 spec tests: the filter's drive colours without turning the voice down (docs/specs/phase-2.2-gain-staging.md,
// P2.2.03).
// spec-needs: core/include/undertow/filter.h

#include "doctest.h"

#include "spec_analysis.h"
#include "undertow/filter.h"

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
