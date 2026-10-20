// Phase 2.1 spec tests: the envelope follows a sustain change smoothly (docs/specs/phase-2.1-voice-fixes.md, P2.1.01).
// spec-needs: core/include/undertow/envelope.h

#include "doctest.h"

#include "undertow/envelope.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kRate = 48000.0;

} // namespace

TEST_CASE("P2.1.01 a sustain change while a note is held moves the level smoothly: no step over 0.02, settled within 50 ms") {
    // A held note in its Sustain stage at level 1 (attack 0.5 ms, decay 1 ms), then the sustain level is turned
    // down, up, to 0 and up again, as a knob or host automation would. The 1 ms decay means a sustain change
    // that restarts the decay jumps too.
    undertow::Envelope e;
    e.prepare(kRate);
    e.setAttackMs(0.5);
    e.setDecayMs(1.0);
    e.setSustain(1.0);
    e.setReleaseMs(100.0);
    e.reset();
    e.noteOn();
    for (int n = 0; n < 4800; ++n)
        e.process();
    REQUIRE_MESSAGE(std::fabs(e.level() - 1.0f) <= 1e-6f, "100 ms into a note at sustain 1 the level is " << e.level());

    for (const double target : {0.3, 1.0, 0.0, 0.6}) {
        const double from = e.level();
        e.setSustain(target);
        double previous = from, step = 0, at50 = 0;
        for (int n = 0; n < 4800; ++n) {
            const double level = e.process();
            step = std::max(step, std::fabs(level - previous));
            previous = level;
            if (n == 2399)
                at50 = level;
        }
        INFO("sustain from " << from << " to " << target);
        CHECK_MESSAGE(step <= 0.02, "the level moved " << step << " in one sample: follow a sustain change over a few "
                                                                  "milliseconds (a one-pole of about 5 ms), not at once");
        CHECK_MESSAGE(std::fabs(at50 - target) <= 0.005, "50 ms after the change the level is " << at50 << "; it must be "
                                                                                                   "within 0.005 of the new sustain level");
        CHECK_MESSAGE(std::fabs(previous - target) <= 1e-5, "100 ms after the change the level is " << previous
                                                                                                    << "; it must hold the sustain level");
    }
    e.noteOff();
    int samples = 0;
    while (e.stage() != undertow::Envelope::Stage::Idle && samples < 48000) {
        e.process();
        ++samples;
    }
    CHECK_MESSAGE(e.level() == 0.0f, "the release after a sustain change must still end in an exact 0 (level " << e.level() << ")");
}
