// Phase 2.2 spec tests: the rumble's drive colours without turning it down, and the rumble sits under the bass at the
// defaults (docs/specs/phase-2.2-gain-staging.md, P2.2.04 and P2.2.05).
// spec-needs: core/include/undertow/rumble.h

#include "doctest.h"

#include "spec_analysis.h"
#include "spec_engine.h"
#include "undertow/engine.h"
#include "undertow/rumble.h"

#include <cmath>
#include <vector>

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

// Every default, playing `events` for 16 beats at 128 BPM (a playing host), with the rumble at its default level or
// off; the left channel.
spec::Signal defaultsPlaying(const std::vector<undertow::MidiEvent>& events, bool rumble) {
    auto p = spec::defaults();
    if (!rumble)
        spec::set(p, "rumble_level", 0);
    undertow::Engine engine;
    engine.prepare(kRate, 512);
    spec::apply(engine, p);
    engine.reset();
    return spec::render(engine, static_cast<std::size_t>(16 * kBeat), events, undertow::Transport{true, true, 128.0, 0.0}, 512)
        .left;
}

// The rumble's level under the bass below 200 Hz, in dB: what a render with the rumble adds to the same render without
// it, from beat 4 on.
double rumbleUnderBass(const std::vector<undertow::MidiEvent>& events) {
    const auto from = static_cast<std::size_t>(4 * kBeat);
    const auto with = defaultsPlaying(events, true);
    const auto bass = defaultsPlaying(events, false);
    spec::Signal rumble(with.size());
    for (std::size_t n = 0; n < with.size(); ++n)
        rumble[n] = with[n] - bass[n];
    return spec::toDb(spec::rms(spec::lowpass(rumble, 200.0, kRate, 4), from) / spec::rms(spec::lowpass(bass, 200.0, kRate, 4), from));
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

TEST_CASE("P2.2.05 at the defaults the rumble sits 9 to 15 dB under a held bass below 200 Hz, and 4 to 11 dB under a rolling line") {
    // The conservative case: a held note leaves the bass no gaps (A1 shares the rumble's band; A2 sits above most of it).
    for (const int note : {33, 45}) {
        const double balance = rumbleUnderBass({spec::noteOn(0, note, 100)});
        INFO("holding note " << note);
        CHECK_MESSAGE(balance >= -15.0, "at the defaults the rumble sits " << -balance << " dB under the bass below 200 Hz: "
                                        "too quiet to hear (want 9 to 15 dB under); raise the rumble's output gain");
        CHECK_MESSAGE(balance <= -9.0, "at the defaults the rumble sits only " << -balance << " dB under the bass below "
                                       "200 Hz (want 9 to 15 dB under): it would crowd the bass");
    }
    // A rolling line: sixteenths on A2 (velocity 90, gate 60%) that rest on every beat, where the ghost kick lands. The
    // rumble then comes about 5 dB closer, which a duck or a level that only suits a held note would break.
    std::vector<undertow::MidiEvent> roller;
    for (int beat = 0; beat < 16; ++beat)
        for (int step = 1; step < 4; ++step) {
            const int at = static_cast<int>((beat + step / 4.0) * kBeat);
            roller.push_back(spec::noteOn(at, 45, 90));
            roller.push_back(spec::noteOff(at + static_cast<int>(0.6 * kBeat / 4.0), 45));
        }
    const double rolling = rumbleUnderBass(roller);
    CHECK_MESSAGE(rolling >= -11.0, "under a rolling line on A2 the rumble sits " << -rolling << " dB under the bass below 200 Hz: "
                                    "want 4 to 11 dB under");
    CHECK_MESSAGE(rolling <= -4.0, "under a rolling line on A2 the rumble sits only " << -rolling << " dB under the bass below "
                                   "200 Hz: want 4 to 11 dB under, so it doesn't cover the line");
}
