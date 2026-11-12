// SPDX-License-Identifier: AGPL-3.0-only
// Phase 3 spec tests: the engine plays the voice from MIDI notes (Notes mode) (docs/specs/phase-3-voice.md, checks
// P3.15 to P3.19).
// spec-needs: core/include/undertow/voice.h

#include "doctest.h"

#include "spec_alloc.h"
#include "spec_analysis.h"
#include "spec_engine.h"
#include "spec_sound.h"
#include "undertow/engine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr double kRate = 48000.0;
using namespace undertow::param;

// Notes mode, with the rumble off unless a test turns it on.
spec::Params notes() {
    auto p = spec::defaults();
    spec::set(p, SeqMode, 2);
    spec::set(p, RumbleLevel, 0);
    spec::set(p, RumbleKick, 0);
    spec::set(p, RumbleSend, 0);
    return p;
}

void prepared(undertow::Engine& e, const spec::Params& p) {
    e.prepare(kRate, 512);
    spec::apply(e, p);
    e.reset();
}

const undertow::Transport kHost{true, true, 130.0, 0.0};

} // namespace

TEST_CASE("P3.15 in Notes mode each MIDI note-on is one hit at its pitch and velocity, from its own sample") {
    undertow::Engine e;
    prepared(e, notes());
    const auto out = spec::render(e, 2400, {spec::noteOn(1000, 45, 100)}, kHost);
    CHECK_MESSAGE(spec::peakAbs(out.left, 0, 1000) == 0.0, "before the note-on the output must be silent");
    CHECK_MESSAGE(spec::peakAbs(out.left, 1000, 1100) > 0.0, "the hit must start at the note-on's own sample");
    CHECK_MESSAGE(e.meters().noteHz == doctest::Approx(110.0).epsilon(1e-4), "note 45 is 110 Hz; meters().noteHz is "
                                                                                 << e.meters().noteHz);

    // Any channel; a note-off changes nothing (each hit is an attack and a decay); velocity 0 is no hit.
    undertow::Engine a, b, c;
    for (auto* x : {&a, &b, &c})
        prepared(*x, notes());
    const auto withOff = spec::render(a, 24000, {{0, 0x93, 45, 100}, spec::noteOff(4800, 45)}, kHost);
    const auto without = spec::render(b, 24000, {{0, 0x93, 45, 100}}, kHost);
    CHECK_MESSAGE(withOff.left == without.left, "a note-off changed the sound: in Notes mode a hit plays its whole "
                                                "decay");
    CHECK_MESSAGE(spec::peakAbs(without.left) > 0.0, "a note-on on channel 4 must play");
    const auto zero = spec::render(c, 4800, {{0, 0x90, 45, 0}}, kHost);
    CHECK_MESSAGE(spec::peakAbs(zero.left) == 0.0, "a note-on with velocity 0 is a note-off: no hit");

    // Velocity: a hit at 64 peaks about 6 dB under one at 127.
    undertow::Engine soft, loud;
    prepared(soft, notes());
    prepared(loud, notes());
    const double s = spec::peakAbs(spec::render(soft, 4800, {spec::noteOn(0, 43, 64)}, kHost).left);
    const double l = spec::peakAbs(spec::render(loud, 4800, {spec::noteOn(0, 43, 127)}, kHost).left);
    CHECK_MESSAGE(spec::toDb(l / s) >= 4.5, "a note at velocity 127 peaks only " << spec::toDb(l / s)
                                                                               << " dB over one at 64; the MIDI "
                                                                                  "velocity / 127 is the hit's "
                                                                                  "velocity");

    // All sound off (controller 120) silences at once.
    undertow::Engine d;
    prepared(d, notes());
    const auto cut = spec::render(d, 9600, {spec::noteOn(0, 43, 127), spec::controller(4800, 120, 0)}, kHost);
    CHECK_MESSAGE(spec::peakAbs(cut.left, 4800) == 0.0, "after all sound off (controller 120) the output must be "
                                                        "silent at once; it reached "
                                                            << spec::peakAbs(cut.left, 4800));
}

TEST_CASE("P3.16 the meters show the note and its pitch while it sounds, and -1 and 0 once it has died away") {
    undertow::Engine e;
    auto p = notes();
    spec::set(p, VcaDecay, 100);
    prepared(e, p);
    spec::render(e, 480, {spec::noteOn(0, 57, 100)}, kHost);
    CHECK_MESSAGE(e.meters().note == 57, "while note 57 sounds meters().note must be 57; got " << e.meters().note);
    CHECK_MESSAGE(e.meters().noteHz == doctest::Approx(220.0).epsilon(1e-4), "meters().noteHz must be 220");
    spec::render(e, static_cast<std::size_t>(kRate), {}, kHost);
    CHECK_MESSAGE(e.meters().note == -1, "once the hit has died away meters().note must be -1; got " << e.meters().note);
    CHECK_MESSAGE(e.meters().noteHz == 0.0f, "once the hit has died away meters().noteHz must be 0");
}

TEST_CASE("P3.17 the voice feeds the rumble: with the ghost kick off, rumble_send gives the bass its own dark tail") {
    auto tail = [](float send) {
        undertow::Engine e;
        auto p = notes();
        spec::set(p, VcaDecay, 60);
        spec::set(p, RumbleLevel, 1);
        spec::set(p, RumbleSend, send);
        prepared(e, p);
        const auto out = spec::render(e, static_cast<std::size_t>(kRate), {spec::noteOn(0, 43, 127)}, kHost);
        return spec::rms(out.left, spec::ms(300), spec::ms(800));
    };
    const double sent = tail(0.6f), dry = tail(0.0f);
    CHECK_MESSAGE(dry <= 1e-5, "with rumble_send 0 and the ghost kick off, the voice's 60 ms hit must leave nothing "
                               "300 ms later; RMS "
                                   << dry);
    CHECK_MESSAGE(sent >= 1e-3, "with rumble_send 0.6 the voice must feed the rumble's reverb: 300 to 800 ms after a "
                                "60 ms hit its tail has RMS "
                                    << sent);
}

TEST_CASE("P3.18 the default patch at the output: peaks between -12 and -6 dBFS, no clicks, no DC, mono") {
    // The default patch played as its pattern would be (1/16 at 130 BPM, the steps' velocities, root G2), the rumble
    // on at its defaults.
    undertow::Engine e;
    auto p = spec::defaults();
    spec::set(p, SeqMode, 2);
    prepared(e, p);
    const double step = 0.25 * 60.0 / 130.0 * kRate;
    std::vector<undertow::MidiEvent> events;
    std::vector<std::size_t> onsets;
    for (int k = 0; k < 128; ++k) {
        const float velocity = p[static_cast<std::size_t>(Step1Vel + k % undertow::kSteps)];
        if (velocity <= 0.0f)
            continue;
        const auto at = static_cast<std::size_t>(std::lround(k * step));
        events.push_back(spec::noteOn(static_cast<int>(at), 43, static_cast<int>(std::lround(velocity * 127))));
        onsets.push_back(at);
    }
    const auto out = spec::render(e, static_cast<std::size_t>(std::lround(128 * step)), events, kHost);
    const double peak = spec::toDb(spec::peakAbs(out.left));
    CHECK_MESSAGE(peak >= -12.0, "the default pattern peaks at " << peak
                                                                 << " dBFS; master_gain's default puts it between "
                                                                    "-12 and -6 dBFS");
    CHECK_MESSAGE(peak <= -6.0, "the default pattern peaks at " << peak
                                                                << " dBFS; master_gain's default puts it between -12 "
                                                                   "and -6 dBFS");
    const double click = spec::clickPeakDbfs(out.left, onsets, kRate);
    CHECK_MESSAGE(click <= -60.0, "around a hit the output high-passed at 6 kHz reaches " << click
                                                                                          << " dBFS; want -60 or less: "
                                                                                             "rise along half a "
                                                                                             "cosine, never a line");
    CHECK_MESSAGE(std::fabs(spec::mean(out.left)) <= 1e-4, "the output's mean is " << spec::mean(out.left)
                                                                                   << "; want -80 dBFS or less");
    CHECK_MESSAGE(out.left == out.right, "both channels must carry the same signal");
}

TEST_CASE("P3.19 under random notes and settings the engine stays finite, bounded and allocation-free, then silent") {
    undertow::Engine e;
    prepared(e, notes());
    std::uint32_t state = 77;
    auto next = [&]() {
        state = state * 1664525u + 1013904223u;
        return static_cast<double>(state >> 8) / 16777216.0;
    };
    std::vector<float> left(512), right(512);
    std::vector<undertow::MidiEvent> events(4);
    double peak = 0;
    int nonFinite = 0;
    undertow::Transport t = kHost;
    spec::AllocationCounter counter;
    const double start = spec::threadSeconds();
    for (int block = 0; block < 900; ++block) { // 9.6 s
        if (block % 10 == 0)
            for (int i = 0; i < undertow::kNumParams; ++i) {
                const auto& spec = undertow::paramSpec(i);
                if (i != SeqMode)
                    e.setParam(i, spec.min + static_cast<float>(next()) * (spec.max - spec.min));
            }
        for (int k = 0; k < 4; ++k)
            events[static_cast<std::size_t>(k)] = spec::noteOn(k * 128, 24 + static_cast<int>(next() * 60),
                                                                1 + static_cast<int>(next() * 126));
        e.process(left.data(), right.data(), 512, events.data(), block % 3 == 0 ? 4 : 0, t);
        t.ppq += 512.0 / kRate * 130.0 / 60.0;
        for (int n = 0; n < 512; ++n) {
            nonFinite += std::isfinite(left[static_cast<std::size_t>(n)]) ? 0 : 1;
            peak = std::max(peak, static_cast<double>(std::fabs(left[static_cast<std::size_t>(n)])));
        }
    }
    const double seconds = spec::threadSeconds() - start;
    const std::size_t allocations = counter.count(); // read before CHECK: doctest's first assertion allocates
    CHECK_MESSAGE(nonFinite == 0, nonFinite << " non-finite samples under random notes and settings");
    // The clipper's ceiling is the float 0.966f, a hair over the double 0.966: a sample held exactly at it is in.
    CHECK_MESSAGE(peak <= static_cast<double>(0.966f), "the output reached " << peak
                                                                             << "; the clipper holds it within 0.966");
    CHECK_MESSAGE(allocations == 0, allocations << " heap allocations while playing: size everything in prepare()");
    CHECK_MESSAGE(seconds <= 1.0, "9.6 s of random playing took " << seconds << " s of CPU; the budget is 1 s");
    // Then the rumble off, a short decay and no more notes: the output ends in exact zeros.
    e.setParam(RumbleLevel, 0);
    e.setParam(VcaDecay, 50);
    const auto tail = spec::render(e, static_cast<std::size_t>(3 * kRate), {}, t);
    CHECK_MESSAGE(spec::peakAbs(tail.left, static_cast<std::size_t>(2.5 * kRate)) == 0.0,
                  "2.5 s after the last note, with the rumble off, the output must be exact silence; it reached "
                      << spec::peakAbs(tail.left, static_cast<std::size_t>(2.5 * kRate)));
}
