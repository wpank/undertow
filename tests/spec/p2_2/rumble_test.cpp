// Phase 2.2 spec tests: the rumble's drive colours without turning it down, and the rumble sits under the bass at the
// defaults (docs/specs/phase-2.2-gain-staging.md, P2.2.04 and P2.2.05).
// spec-needs: core/include/undertow/rumble.h

#include "doctest.h"

#include "spec_analysis.h"
#include "spec_engine.h"
#include "undertow/engine.h"
#include "undertow/rumble.h"

#include <cmath>

namespace {

constexpr double kRate = 48000.0;
constexpr double kBeat = 60.0 / 128.0 * kRate; // 22500 samples

// The rumble alone at rumble_level 1 with nothing sent in: its ghost kick at the default kick level into the reverb,
// 16 beats at 128 BPM; measured from beat 4 on.
spec::Signal rumbleAlone(float drive) {
    auto p = spec::defaults();
    spec::set(p, "rumble_level", 1);
    spec::set(p, "rumble_send", 0);
    spec::set(p, "rumble_drive", drive);
    undertow::Rumble rumble;
    rumble.prepare(kRate);
    rumble.setParams(p.data());
    rumble.reset();
    spec::Signal out(static_cast<std::size_t>(16 * kBeat));
    for (std::size_t n = 0; n < out.size(); ++n)
        out[n] = rumble.process(0.0f, static_cast<double>(n) / kBeat, 128.0, true);
    return out;
}

double crestDb(const spec::Signal& x, std::size_t from) { return spec::toDb(spec::peakAbs(x, from) / spec::rms(x, from)); }

// Every default, holding `note` at velocity 100 for 16 beats at 128 BPM (a playing host), with the rumble at its
// default level or off; the left channel.
spec::Signal defaultsHolding(int note, bool rumble) {
    auto p = spec::defaults();
    if (!rumble)
        spec::set(p, "rumble_level", 0);
    undertow::Engine engine;
    engine.prepare(kRate, 512);
    spec::apply(engine, p);
    engine.reset();
    return spec::render(engine, static_cast<std::size_t>(16 * kBeat), {spec::noteOn(0, note, 100)},
                        undertow::Transport{true, true, 128.0, 0.0}, 512)
        .left;
}

} // namespace

TEST_CASE("P2.2.04 rumble_drive keeps the rumble's level and flattens its peaks") {
    const auto from = static_cast<std::size_t>(4 * kBeat);
    const auto clean = rumbleAlone(0.0f);
    for (const float drive : {0.25f, 0.4f, 0.5f, 0.75f, 1.0f}) {
        const double change = spec::toDb(spec::rms(rumbleAlone(drive), from) / spec::rms(clean, from));
        CHECK_MESSAGE(std::fabs(change) <= 3.0, "at rumble_drive " << drive << " the rumble's RMS is " << change
                                                << " dB from drive 0: give its saturation a makeup gain, so the drive adds "
                                                   "grit instead of turning the rumble down");
    }
    const double flattened = crestDb(clean, from) - crestDb(rumbleAlone(1.0f), from);
    CHECK_MESSAGE(flattened >= 2.0, "full rumble_drive lowers the rumble's crest factor by only " << flattened
                                    << " dB: the drive must still saturate (round the peaks), not only add gain");
}

TEST_CASE("P2.2.05 at the defaults the rumble sits 9 to 15 dB under the bass below 200 Hz") {
    // The rumble is what a render with it adds to the same render without it (rumble_level 0), from beat 4 on.
    const auto from = static_cast<std::size_t>(4 * kBeat);
    for (const int note : {33, 45}) {
        const auto with = defaultsHolding(note, true);
        const auto bass = defaultsHolding(note, false);
        spec::Signal rumble(with.size());
        for (std::size_t n = 0; n < with.size(); ++n)
            rumble[n] = with[n] - bass[n];
        const double balance = spec::toDb(spec::rms(spec::lowpass(rumble, 200.0, kRate, 4), from) /
                                          spec::rms(spec::lowpass(bass, 200.0, kRate, 4), from));
        INFO("holding note " << note);
        CHECK_MESSAGE(balance >= -15.0, "at the defaults the rumble sits " << -balance << " dB under the bass below 200 Hz: "
                                        "too quiet to hear (want 9 to 15 dB under); raise the rumble's output gain");
        CHECK_MESSAGE(balance <= -9.0, "at the defaults the rumble sits only " << -balance << " dB under the bass below "
                                       "200 Hz (want 9 to 15 dB under): it would crowd the bass");
    }
}
