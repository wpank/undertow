// Phase 1 spec tests: the ADSR envelope (docs/specs/phase-1-voice.md, P1.08 to P1.10).
// spec-needs: core/include/undertow/envelope.h

#include "doctest.h"

#include "undertow/envelope.h"

#include <cmath>
#include <vector>

namespace {

using Stage = undertow::Envelope::Stage;
constexpr double kRate = 48000.0;

undertow::Envelope make(double attack, double decay, double sustain, double release) {
    undertow::Envelope e;
    e.prepare(kRate);
    e.setAttackMs(attack);
    e.setDecayMs(decay);
    e.setSustain(sustain);
    e.setReleaseMs(release);
    e.reset();
    return e;
}

bool near(double value, double want, double tolerance) { return std::fabs(value - want) <= tolerance; }

std::vector<float> run(undertow::Envelope& e, int samples) {
    std::vector<float> out(static_cast<std::size_t>(samples));
    for (auto& v : out)
        v = e.process();
    return out;
}

} // namespace

TEST_CASE("P1.08 the attack is a straight line from the current level to 1 in the attack time") {
    auto e = make(10.0, 100.0, 1.0, 100.0);
    CHECK_MESSAGE(e.stage() == Stage::Idle, "a reset envelope is Idle");
    CHECK_MESSAGE(e.level() == 0.0f, "a reset envelope is at 0");
    e.noteOn();
    CHECK_MESSAGE(e.stage() == Stage::Attack, "noteOn() starts the Attack stage");
    const auto out = run(e, 600);
    // 10 ms at 48 kHz is 480 samples: after k samples the level is k / 480.
    CHECK_MESSAGE(near(out[119], 0.25, 0.005), "a quarter of the way into a 10 ms attack the level is "
                                                                       << out[119] << "; want 0.25 (a linear rise)");
    CHECK_MESSAGE(near(out[239], 0.5, 0.005), "halfway into a 10 ms attack (5 ms) the level is "
                                                                      << out[239] << "; want 0.5. The attack must take exactly "
                                                                                     "the attack time");
    CHECK_MESSAGE(near(out[359], 0.75, 0.005), "three quarters into the attack the level is " << out[359]);
    CHECK_MESSAGE(near(out[481], 1.0, 1e-6), "the attack reaches 1 at 10 ms; at 10.04 ms it is " << out[481]);
    float most = 0.0f;
    for (float v : out)
        most = std::max(most, v);
    CHECK_MESSAGE(most <= 1.0f, "the envelope went over 1 (" << most << ")");
    CHECK_MESSAGE(e.stage() != Stage::Attack, "after reaching 1 the attack hands over to the decay");
}

TEST_CASE("P1.09 the decay falls to the sustain level exponentially, 1% of the way left after the decay time") {
    auto e = make(0.5, 200.0, 0.5, 300.0);
    e.noteOn();
    int peakAt = -1;
    std::vector<float> out = run(e, 48000);
    for (int i = 0; i < 100; ++i)
        if (out[static_cast<std::size_t>(i)] >= 1.0f && peakAt < 0)
            peakAt = i;
    REQUIRE_MESSAGE(peakAt >= 0, "a 0.5 ms attack must reach 1 within 100 samples");
    auto remaining = [&](double ms) {
        const auto index = static_cast<std::size_t>(peakAt) + static_cast<std::size_t>(std::lround(ms * 48.0));
        return (out[index] - 0.5) / 0.5;
    };
    CHECK_MESSAGE(near(remaining(100.0), 0.1, 0.015),
                  "100 ms into a 200 ms decay to sustain 0.5, " << remaining(100.0)
                                                                << " of the distance is left; want 0.1 (the distance falls by "
                                                                   "a factor of 100 over the decay time, exponentially)");
    CHECK_MESSAGE(near(remaining(200.0), 0.01, 0.004),
                  "at the end of the decay time " << remaining(200.0) << " of the distance is left; want 0.01");
    CHECK_MESSAGE(e.stage() == Stage::Sustain, "1 s into the note, the envelope must be in its Sustain stage");
    const auto held = run(e, 48000);
    float worst = 0.0f;
    for (float v : held)
        worst = std::max(worst, std::fabs(v - 0.5f));
    CHECK_MESSAGE(worst <= 1e-6f, "the sustain must hold the sustain level exactly while the note is held; it strayed by " << worst);
}

TEST_CASE("P1.10 the release falls from the current level to an exact 0, and a new note attacks from where it is") {
    auto e = make(0.5, 50.0, 0.5, 300.0);
    e.noteOn();
    run(e, 24000);
    e.noteOff();
    CHECK_MESSAGE(e.stage() == Stage::Release, "noteOff() starts the Release stage");
    const auto out = run(e, 48000);
    CHECK_MESSAGE(near(out[7199], 0.05, 0.006),
                  "150 ms into a 300 ms release from 0.5 the level is " << out[7199] << "; want 0.05 (a tenth of the start)");
    CHECK_MESSAGE(near(out[14399], 0.005, 0.002), "at the end of the release time the level is "
                                                                           << out[14399] << "; want 0.005 (1% of the start)");
    CHECK_MESSAGE(out[38399] == 0.0f, "800 ms into the release the level is " << out[38399]
                                                                             << ": below -100 dB the envelope must stop at exactly 0");
    CHECK_MESSAGE(e.stage() == Stage::Idle, "once released to 0 the envelope is Idle");

    // A new note in the middle of a release attacks from the current level, never from 0.
    auto r = make(20.0, 50.0, 0.8, 200.0);
    r.noteOn();
    run(r, 9600);
    r.noteOff();
    run(r, 4800);
    const float before = r.level();
    REQUIRE(before > 0.05f);
    r.noteOn();
    const float after = r.process();
    CHECK_MESSAGE(after >= before, "a retrigger dropped the level from " << before << " to " << after
                                                                         << ": the attack must start from the current level");
    CHECK_MESSAGE(near(after - before, 1.0 / 960.0, 0.1 / 960.0),
                  "the retriggered attack rises at the attack's rate (1/960 per sample for 20 ms); it rose " << after - before);

    // A note released during its attack releases from where it got to.
    auto a = make(100.0, 50.0, 0.8, 200.0);
    a.noteOn();
    run(a, 2400);
    const float during = a.level();
    a.noteOff();
    const float next = a.process();
    CHECK_MESSAGE(next <= during, "a release during the attack must not jump up");
    CHECK_MESSAGE(next >= during * 0.99f, "a release during the attack must not jump down: " << during << " to " << next);
}
