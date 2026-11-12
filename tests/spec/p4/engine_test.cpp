// SPDX-License-Identifier: AGPL-3.0-only
// Phase 4 spec tests: the engine plays the pattern in the Key and Free modes (docs/specs/phase-4-sequencer.md, checks
// P4.12 to P4.18).
// spec-needs: core/include/undertow/sequencer.h

#include "doctest.h"

#include "spec_alloc.h"
#include "spec_analysis.h"
#include "spec_engine.h"
#include "spec_sound.h"
#include "undertow/engine.h"
#include "undertow/pattern.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace {

constexpr double kRate = 48000.0;
using namespace undertow::param;

enum Mode { Key = 0, Free = 1, Notes = 2 };

// Distinct pitches and no rests, so each hit shows in meters().note; the drift and the rumble off.
spec::Params steps(Mode mode, int rate) {
    auto p = spec::defaults();
    for (int i = 0; i < undertow::kSteps; ++i) {
        spec::set(p, Step1Pitch + i, static_cast<float>(i));
        spec::set(p, Step1Vel + i, 0.6f + 0.05f * static_cast<float>(i));
    }
    spec::set(p, SeqMode, static_cast<float>(mode));
    spec::set(p, SeqRate, static_cast<float>(rate));
    spec::set(p, SeqDrift, 0);
    spec::set(p, SeqRoot, 36);
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

// The engine one sample at a time, so the meters show every sample: the step playing, the voice's note and the
// output. While the host plays, its position at sample n is `from` plus n samples at its tempo.
struct Trace {
    std::vector<int> step, note, root;
    std::vector<long long> count;
    spec::Signal out;
};

Trace trace(undertow::Engine& e, std::size_t samples, const std::vector<undertow::MidiEvent>& events,
            undertow::Transport t) {
    Trace r;
    r.step.resize(samples);
    r.note.resize(samples);
    r.root.resize(samples);
    r.count.resize(samples);
    r.out.resize(samples);
    const double from = t.ppq, perSample = t.bpm / (60.0 * kRate);
    std::size_t next = 0;
    std::vector<undertow::MidiEvent> now;
    float right = 0;
    for (std::size_t n = 0; n < samples; ++n) {
        now.clear();
        while (next < events.size() && static_cast<std::size_t>(events[next].sampleOffset) <= n) {
            auto ev = events[next++];
            ev.sampleOffset = 0;
            now.push_back(ev);
        }
        if (t.valid && t.playing)
            t.ppq = from + static_cast<double>(n) * perSample;
        e.process(&r.out[n], &right, 1, now.data(), static_cast<int>(now.size()), t);
        const auto m = e.meters();
        r.step[n] = m.step;
        r.note[n] = m.note;
        r.root[n] = m.root;
        r.count[n] = m.count;
    }
    return r;
}

long long floorDiv(long long a, long long b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
int position(long long count) { return static_cast<int>(count - floorDiv(count, 8) * 8); }

// The first sample from `from` on at which the trace shows step `step` playing note `note`, or -1.
long long firstShowing(const Trace& r, long long from, int step, int note) {
    for (auto n = static_cast<std::size_t>(std::max(0LL, from)); n < r.step.size(); ++n)
        if (r.step[n] == step && r.note[n] == note)
            return static_cast<long long>(n);
    return -1;
}

} // namespace

TEST_CASE("P4.12 Key mode: a held key plays the pattern on the host's grid, each hit within one sample") {
    undertow::Engine e;
    prepared(e, steps(Key, 4));
    const undertow::Transport host{true, true, 97.3, 3.7};
    const double perSample = 97.3 / (60.0 * kRate), stepBeats = 0.25;
    const long long keyAt = 1000, offAt = 120000;
    const auto r = trace(e, 130000, {spec::noteOn(keyAt, 40), spec::noteOff(offAt, 40)}, host);
    bool idle = true;
    for (long long n = 0; n < keyAt; ++n) {
        const auto i = static_cast<std::size_t>(n);
        idle = idle && r.step[i] == -1 && r.count[i] == 0 && r.root[i] == -1 && r.out[i] == 0.0f;
    }
    CHECK_MESSAGE(idle, "before the key, Key mode must play nothing: meters().step -1, count 0, root -1 and exact "
                        "silence");
    const auto first = static_cast<long long>(std::floor((3.7 + keyAt * perSample) / stepBeats));
    CHECK_MESSAGE(r.step[keyAt] == position(first), "the key came during count " << first << ": its step ("
                                                                                   << position(first) + 1
                                                                                   << ") must sound at once; "
                                                                                      "meters().step shows "
                                                                                   << r.step[keyAt]);
    CHECK_MESSAGE(r.note[keyAt] == 40 + position(first), "the key's own step plays root 40 plus its pitch at once; "
                                                         "meters().note shows "
                                                             << r.note[keyAt]);
    int checked = 0, wrongCount = 0;
    double worst = 0;
    for (long long k = first + 1; (k * stepBeats - 3.7) / perSample < offAt - 2; ++k) {
        const double exact = (k * stepBeats - 3.7) / perSample;
        const long long at = firstShowing(r, static_cast<long long>(exact) - 2, position(k), 40 + position(k));
        INFO("count " << k << ", due at sample " << exact);
        REQUIRE_MESSAGE(at >= 0, "the step never sounded");
        worst = std::max(worst, std::fabs(static_cast<double>(at) - exact));
        wrongCount += (r.count[static_cast<std::size_t>(at)] == k && r.root[static_cast<std::size_t>(at)] == 40) ? 0 : 1;
        ++checked;
    }
    CHECK_MESSAGE(wrongCount == 0, wrongCount << " steps showed another count or root in the meters: meters().count is "
                                                 "the playing step's count from beat 0, and meters().root its root");
    CHECK_MESSAGE(worst < 1.0, "a hit started " << worst << " samples from its place on the host's grid; within one "
                                                            "sample");
    CHECK(checked >= 15);
    bool stopped = true;
    for (long long n = offAt; n < 130000; ++n)
        stopped = stopped && r.step[static_cast<std::size_t>(n)] == -1 && r.count[static_cast<std::size_t>(n)] == 0 &&
                  r.root[static_cast<std::size_t>(n)] == -1;
    CHECK_MESSAGE(stopped, "with the key up the pattern must stop at once: meters().step -1, count 0, root -1");

    // In blocks of 512 too, each step starts at its own sample, not its block's: Free mode, three rests, then the
    // first hit at beat 0.75 (sample 18000 at 120 BPM, 80 samples into a block).
    undertow::Engine b;
    auto p = steps(Free, 4);
    for (int i = 0; i < 3; ++i)
        spec::set(p, Step1Vel + i, 0);
    prepared(b, p);
    const auto out = spec::render(b, 24000, {}, {true, true, 120.0, 0.0}, 512).left;
    CHECK_MESSAGE(spec::peakAbs(out, 0, 17999) == 0.0, "before the first hit at sample 18000 the output must be "
                                                       "silent: three rests");
    CHECK_MESSAGE(spec::peakAbs(out, 18000, 18100) > 1e-6, "the first hit is due at sample 18000, 80 samples into a "
                                                           "512-sample block: the sequencer must read the beat at "
                                                           "every sample, not once a block");
}

TEST_CASE("P4.13 Free mode plays on seq_root without a key; the own clock without a host; a stopped host auditions") {
    // Free, with the host playing: the pattern on seq_root (36), no MIDI at all.
    {
        undertow::Engine e;
        prepared(e, steps(Free, 4));
        const auto r = trace(e, 60000, {}, {true, true, 120.0, 0.0});
        for (int k = 0; k < 9; ++k) {
            const long long at = k * 6000;
            INFO("count " << k);
            CHECK_MESSAGE((r.step[static_cast<std::size_t>(at)] == k % 8 &&
                           r.note[static_cast<std::size_t>(at)] == 36 + k % 8),
                          "Free mode must play step " << k % 8 + 1 << " on seq_root 36 at sample " << at
                                                      << "; meters show step " << r.step[static_cast<std::size_t>(at)] + 1
                                                      << ", note " << r.note[static_cast<std::size_t>(at)]);
        }
    }
    // A parameter changed while it plays takes effect: seq_rate from 1/16 to 1/4.
    {
        undertow::Engine e;
        prepared(e, steps(Free, 4));
        e.setParam(SeqRate, 2);
        const auto r = trace(e, 60000, {}, {true, true, 120.0, 0.0});
        int changes = 0;
        for (std::size_t n = 1; n < r.step.size(); ++n)
            changes += r.step[n] != r.step[n - 1] ? 1 : 0;
        CHECK_MESSAGE((changes == 2 && r.step[24000] == 1 && r.step[48000] == 2),
                      "after seq_rate goes to 1/4 the steps must come every 24000 samples at 120 BPM: the engine "
                      "gives the sequencer the parameters at every block; "
                          << changes << " changes of step in 60000 samples");
    }
    // No host position (the Standalone app): the engine's own clock at seq_bpm, from 0 after reset().
    {
        undertow::Engine e;
        auto p = steps(Free, 4);
        spec::set(p, SeqBpm, 150);
        prepared(e, p);
        const auto r = trace(e, 50000, {}, undertow::Transport{});
        for (int k = 1; k < 10; ++k) {
            const long long at = firstShowing(r, k * 4800 - 2, k % 8, 36 + k % 8);
            CHECK_MESSAGE(std::llabs(at - k * 4800) <= 1, "on its own clock at 150 BPM count "
                                                              << k << " is due at sample " << k * 4800
                                                              << "; it sounded at " << at);
        }
    }
    // The host stopped: a held key plays from step 1 at once, counting at the host's tempo.
    for (const Mode mode : {Key, Free}) {
        undertow::Engine e;
        prepared(e, steps(mode, 4));
        const auto r = trace(e, 40000, {spec::noteOn(3000, 45)}, {true, false, 100.0, 12.3});
        INFO((mode == Key ? "Key mode" : "Free mode"));
        CHECK_MESSAGE(r.step[2999] == -1, "with the host stopped and no key, nothing plays");
        for (int k = 0; k < 5; ++k) {
            const long long at = firstShowing(r, 3000 + k * 7200 - 2, k, 45 + k);
            CHECK_MESSAGE(std::llabs(at - (3000 + k * 7200)) <= 1,
                          "step " << k + 1 << " of the audition is due at sample " << 3000 + k * 7200
                                  << " (the key, then a 1/16 at the host's 100 BPM each); it sounded at " << at);
        }
    }
}

TEST_CASE("P4.14 each hit takes its root and pitch when it starts and holds them; a rest lets the note ring") {
    undertow::Engine e;
    auto p = steps(Key, 2); // 1/4: a step every 24000 samples at 120 BPM
    spec::set(p, Step3Vel, 0);
    spec::set(p, VcaDecay, 4000);
    prepared(e, p);
    const auto r = trace(e, 80000, {spec::noteOn(0, 43), spec::noteOn(6000, 50)}, {true, true, 120.0, 0.0});
    bool held = true;
    for (std::size_t n = 6000; n < 24000; ++n)
        held = held && r.note[n] == 43;
    CHECK_MESSAGE(held, "a key pressed mid-step must not change the note that is playing: it holds until the next "
                        "hit");
    CHECK_MESSAGE(r.note[24000] == 51, "the next hit takes the new root, 50, plus its pitch: want note 51, got "
                                           << r.note[24000]);
    bool rings = true;
    for (std::size_t n = 48000; n < 72000; ++n)
        rings = rings && r.note[n] == 51;
    CHECK_MESSAGE(rings, "step 3 is a rest: the note before it must ring on through it");
    CHECK_MESSAGE(spec::rms(r.out, 60000, 72000) > 1e-4, "through the rest the ringing note must still be heard");
    CHECK_MESSAGE(r.step[48000] == 2, "a rest is still a step: meters().step shows it");

    // Notes mode is unchanged: each note is one hit, at once, and the sequencer stays idle.
    undertow::Engine n;
    prepared(n, steps(Notes, 4));
    const auto q = trace(n, 30000, {spec::noteOn(5000, 55), spec::noteOn(17000, 48)}, {true, true, 120.0, 0.0});
    CHECK_MESSAGE((q.note[5000] == 55 && q.note[17000] == 48), "in Notes mode each note-on is a hit at its own pitch");
    CHECK_MESSAGE(*std::max_element(q.step.begin(), q.step.end()) == -1, "in Notes mode meters().step stays -1");
}

TEST_CASE("P4.15 all notes off and all sound off release the keys; reset() forgets them") {
    // Controller 123, controller 120, and (for comparison) a note-on at velocity 0, which is a note-off.
    for (const int cc : {123, 120, 0}) {
        undertow::Engine e;
        prepared(e, steps(Key, 4));
        const undertow::MidiEvent off = cc > 0 ? spec::controller(20000, cc, 0) : undertow::MidiEvent{20000, 0x90, 43, 0};
        const auto r = trace(e, 60000, {spec::noteOn(0, 43), off}, {true, true, 120.0, 0.0});
        INFO((cc > 0 ? "controller " : "a note-on at velocity 0") << (cc > 0 ? std::to_string(cc) : std::string()));
        bool stopped = true;
        for (std::size_t n = 20000; n < 60000; ++n)
            stopped = stopped && r.step[n] == -1;
        CHECK_MESSAGE(stopped, "after it no key is held: the pattern must stop");
        if (cc == 120)
            CHECK_MESSAGE(spec::peakAbs(r.out, 20000) == 0.0, "all sound off silences at once, and no step follows");
        else
            CHECK_MESSAGE(spec::peakAbs(r.out, 20000, 21000) > 0.0, "the ringing note must decay, not stop");
    }
    undertow::Engine e;
    prepared(e, steps(Key, 4));
    trace(e, 10000, {spec::noteOn(0, 43)}, {true, true, 120.0, 0.0});
    e.reset();
    const auto r = trace(e, 20000, {}, {true, true, 120.0, 10000 / 24000.0});
    CHECK_MESSAGE(*std::max_element(r.step.begin(), r.step.end()) == -1, "after reset() no key is held");
}

TEST_CASE("P4.16 the default patch's own pattern, drift included, keeps its energy between 30 and 150 Hz") {
    // The PRD's render: 8 bars at 130 BPM of the default pattern, rooted on G2 held from the first sample, the voice
    // alone.
    undertow::Engine e;
    auto p = spec::defaults();
    spec::set(p, RumbleLevel, 0);
    spec::set(p, RumbleKick, 0);
    spec::set(p, RumbleSend, 0);
    prepared(e, p);
    const auto samples = static_cast<std::size_t>(std::lround(32 * 60.0 / 130.0 * kRate));
    const auto line = spec::render(e, samples, {spec::noteOn(0, 43)}, {true, true, 130.0, 0.0}).left;
    const auto l = spec::ltas(line, kRate);
    const double below150 = spec::share(l, 0, 150), below300 = spec::share(l, 0, 300),
                 band = spec::share(l, 30, 150), below30 = spec::share(l, 0, 30);
    CHECK_MESSAGE(below150 >= 0.90, (100 * below150) << "% of the power is below 150 Hz; want 90% or more");
    CHECK_MESSAGE(below300 >= 0.985, (100 * below300) << "% of the power is below 300 Hz; want 98.5% or more");
    CHECK_MESSAGE(band >= 0.88, (100 * band) << "% of the power is between 30 and 150 Hz; want 88% or more");
    CHECK_MESSAGE(below30 <= 0.01, (100 * below30) << "% of the power is below 30 Hz; want 1% at most");
    CHECK_MESSAGE(spec::centroidHz(l) <= 90.0, "the spectral centroid is " << spec::centroidHz(l)
                                                                           << " Hz; want 90 Hz at most");
    const double above500 = 10 * std::log10(spec::share(l, 500, 24001)),
                 above1k = 10 * std::log10(spec::share(l, 1000, 24001)),
                 above2k = 10 * std::log10(spec::share(l, 2000, 24001)),
                 body = 10 * std::log10(spec::share(l, 150, 500));
    CHECK_MESSAGE(above500 <= -25.0, "the band above 500 Hz is " << above500 << " dB re the total; want -25 dB or less");
    CHECK_MESSAGE(above1k <= -40.0, "the band above 1 kHz is " << above1k << " dB re the total; want -40 dB or less");
    CHECK_MESSAGE(above2k <= -55.0, "the band above 2 kHz is " << above2k << " dB re the total; want -55 dB or less");
    CHECK_MESSAGE(body >= -20.0, "150 to 500 Hz holds " << body << " dB of the power; want 1% (-20 dB) or more");
    CHECK_MESSAGE(body <= -11.0, "150 to 500 Hz holds " << body << " dB of the power; want 8% (-11 dB) at most");
    const double bright = spec::brightnessP95(line, kRate);
    CHECK_MESSAGE(bright <= 150.0, "the brightest 5% of the frames have their centroid at " << bright
                                                                                           << " Hz; want 150 Hz at "
                                                                                              "most");
    // The drift is in it: the same 8 bars at drift 0 sound different.
    undertow::Engine home;
    spec::set(p, SeqDrift, 0);
    prepared(home, p);
    const auto still = spec::render(home, samples, {spec::noteOn(0, 43)}, {true, true, 130.0, 0.0}).left;
    CHECK_MESSAGE(still != line, "the default drift (0.7) must change the 8 bars: at drift 0 they are the same");
    CHECK_MESSAGE(spec::peakAbs(line) > 0.05, "the default pattern must sound under a held key");
}

TEST_CASE("P4.17 a looped bar replays exactly: the same steps and notes on every pass, drift included") {
    // The default patch with the steps' pitches spread, so the drift's moves show in the notes; the host plays beats
    // 16 to 20, where the default drift is away from home, then loops back twice.
    auto p = spec::defaults();
    spec::set(p, RumbleLevel, 0);
    spec::set(p, RumbleKick, 0);
    spec::set(p, RumbleSend, 0);
    for (int i = 0; i < undertow::kSteps; ++i)
        spec::set(p, Step1Pitch + i, static_cast<float>(i - 3));
    const std::size_t bar = 96000; // 4 beats at 120 BPM
    undertow::Engine straight;
    prepared(straight, p);
    const auto whole = trace(straight, 5 * bar, {spec::noteOn(0, 43)}, {true, true, 120.0, 0.0});
    // The bar before the loop plays first, so the loop's first pass starts with the same note ringing.
    undertow::Engine looped;
    prepared(looped, p);
    trace(looped, bar, {spec::noteOn(0, 43)}, {true, true, 120.0, 12.0});
    for (int pass = 0; pass < 3; ++pass) {
        const auto r = trace(looped, bar, {}, {true, true, 120.0, 16.0});
        int differ = 0;
        for (std::size_t n = 0; n < bar; ++n)
            differ += (r.step[n] == whole.step[4 * bar + n] && r.note[n] == whole.note[4 * bar + n]) ? 0 : 1;
        INFO("pass " << pass + 1);
        CHECK_MESSAGE(differ == 0, differ << " samples of the loop show another step or note than the same bar played "
                                             "straight through: the steps follow the host's position, drift and all");
    }
    undertow::Pattern pattern;
    pattern.setParams(p.data());
    int drifted = 0;
    for (long long k = 64; k < 80; ++k)
        drifted += pattern.at(k, 43).pitch != pattern.home(position(k)).pitch ? 1 : 0;
    CHECK_MESSAGE(drifted > 0, "the test needs a bar where the default drift moves a pitch");
}

TEST_CASE("P4.18 under random transports, keys and settings the engine stays finite, bounded and allocation-free") {
    undertow::Engine e;
    prepared(e, steps(Key, 4));
    std::uint32_t state = 4242;
    auto next = [&]() {
        state = state * 1664525u + 1013904223u;
        return static_cast<double>(state >> 8) / 16777216.0;
    };
    std::vector<float> left(512), right(512);
    std::vector<undertow::MidiEvent> events(6);
    double peak = 0;
    int nonFinite = 0;
    undertow::Transport t{true, true, 130.0, 0.0};
    spec::AllocationCounter counter;
    const double start = spec::threadSeconds();
    for (int block = 0; block < 900; ++block) { // 9.6 s
        if (block % 10 == 0)
            for (int i = 0; i < undertow::kNumParams; ++i) {
                const auto& spec = undertow::paramSpec(i);
                e.setParam(i, spec.min + static_cast<float>(next()) * (spec.max - spec.min));
            }
        if (block % 7 == 0) {
            t.valid = next() < 0.8;
            t.playing = next() < 0.7;
            t.bpm = 40.0 + 210.0 * next();
            if (next() < 0.5)
                t.ppq = -4.0 + 64.0 * next();
        }
        int numEvents = 0;
        for (int k = 0; k < 6; ++k) {
            const double r = next();
            const int note = 24 + static_cast<int>(next() * 60), at = static_cast<int>(next() * 512);
            if (r < 0.3)
                events[static_cast<std::size_t>(numEvents++)] = spec::noteOn(at, note, 1 + static_cast<int>(next() * 126));
            else if (r < 0.55)
                events[static_cast<std::size_t>(numEvents++)] = spec::noteOff(at, note);
            else if (r < 0.58)
                events[static_cast<std::size_t>(numEvents++)] = spec::controller(at, 123, 0);
        }
        std::sort(events.begin(), events.begin() + numEvents,
                  [](const undertow::MidiEvent& a, const undertow::MidiEvent& b) { return a.sampleOffset < b.sampleOffset; });
        e.process(left.data(), right.data(), 512, events.data(), numEvents, t);
        if (t.valid && t.playing)
            t.ppq += 512.0 / kRate * t.bpm / 60.0;
        for (int n = 0; n < 512; ++n) {
            nonFinite += std::isfinite(left[static_cast<std::size_t>(n)]) ? 0 : 1;
            peak = std::max(peak, static_cast<double>(std::fabs(left[static_cast<std::size_t>(n)])));
        }
    }
    const double seconds = spec::threadSeconds() - start;
    const std::size_t allocations = counter.count(); // read before CHECK: doctest's first assertion allocates
    CHECK_MESSAGE(nonFinite == 0, nonFinite << " non-finite samples under random transports, keys and settings");
    // The clipper's ceiling is the float 0.966f, a hair over the double 0.966: a sample held exactly at it is in.
    CHECK_MESSAGE(peak <= static_cast<double>(0.966f), "the output reached " << peak
                                                                             << "; the clipper holds it within 0.966");
    CHECK_MESSAGE(allocations == 0, allocations << " heap allocations while playing: size everything in prepare()");
    CHECK_MESSAGE(seconds <= 1.0, "9.6 s of random playing took " << seconds << " s of CPU; the budget is 1 s");
    // Then Key mode, every key up, the rumble off and a short decay: the output ends in exact zeros.
    e.setParam(SeqMode, Key);
    e.setParam(RumbleLevel, 0);
    e.setParam(VcaDecay, 50);
    t = {true, true, 130.0, 100.0};
    const auto tail = spec::render(e, static_cast<std::size_t>(3 * kRate), {spec::controller(0, 123, 0)}, t);
    CHECK_MESSAGE(spec::peakAbs(tail.left, static_cast<std::size_t>(2.5 * kRate)) == 0.0,
                  "2.5 s after every key went up, with the rumble off, the output must be exact silence; it reached "
                      << spec::peakAbs(tail.left, static_cast<std::size_t>(2.5 * kRate)));
}
