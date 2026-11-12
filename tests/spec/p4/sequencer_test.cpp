// SPDX-License-Identifier: AGPL-3.0-only
// Phase 4 spec tests: the sequencer, its clock and its keys (docs/specs/phase-4-sequencer.md, checks P4.06 to P4.11).
// spec-needs: core/include/undertow/sequencer.h

#include "doctest.h"

#include "spec_alloc.h"
#include "spec_engine.h"
#include "undertow/pattern.h"
#include "undertow/sequencer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr double kRate = 48000.0;
using namespace undertow::param;

constexpr double kStepBeats[8] = {4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0};
const char* const kRateNames[8] = {"1 bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T"};

enum Mode { Key = 0, Free = 1, Notes = 2 };

// Distinct pitches and no rests, so each step's start shows in its note; drift off unless a test turns it on.
spec::Params steps(Mode mode, int rate, int length = 8, float drift = 0.0f) {
    auto p = spec::defaults();
    for (int i = 0; i < undertow::kSteps; ++i) {
        spec::set(p, Step1Pitch + i, static_cast<float>(i));
        spec::set(p, Step1Vel + i, 0.3f + 0.1f * static_cast<float>(i % 4));
    }
    spec::set(p, SeqMode, static_cast<float>(mode));
    spec::set(p, SeqRate, static_cast<float>(rate));
    spec::set(p, SeqLength, static_cast<float>(length));
    spec::set(p, SeqDrift, drift);
    spec::set(p, SeqRoot, 36);
    return p;
}

undertow::Sequencer make(const spec::Params& p) {
    undertow::Sequencer s;
    s.prepare(kRate);
    s.setParams(p.data());
    s.reset();
    return s;
}

struct Start {
    long long sample;
    int step, note;
    float velocity;
};

// A clock: its beat at each sample, whether it runs and its tempo. The beat is worked out from where the clock was
// last set, never summed sample by sample, so it holds no rounding drift.
struct Clock {
    double origin = 0.0; // the beat at sample `at`
    double bpm = 120.0;
    bool running = true;
    long long sample = 0; // the next sample to tick
    long long at = 0;
    double beat() const {
        return running ? origin + static_cast<double>(sample - at) * bpm / (60.0 * kRate) : origin;
    }
    void jump(double to) {
        origin = to;
        at = sample;
    }
    void stop() {
        jump(beat());
        running = false;
    }
    void play(double from) {
        running = true;
        jump(from);
    }
};

// Tick the sequencer once a sample and keep every step start with its sample.
std::vector<Start> run(undertow::Sequencer& s, Clock& c, long long samples) {
    std::vector<Start> out;
    for (long long n = 0; n < samples; ++n, ++c.sample) {
        undertow::SeqStep step;
        if (s.tick(c.beat(), c.running, c.bpm, step))
            out.push_back({c.sample, step.step, step.note, step.velocity});
    }
    return out;
}

// The same, counting the starts without keeping them, so it never allocates.
int countStarts(undertow::Sequencer& s, Clock& c, long long samples) {
    int starts = 0;
    for (long long n = 0; n < samples; ++n, ++c.sample) {
        undertow::SeqStep step;
        starts += s.tick(c.beat(), c.running, c.bpm, step) ? 1 : 0;
    }
    return starts;
}

long long floorDiv(long long a, long long b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
int position(long long count, int length) { return static_cast<int>(count - floorDiv(count, length) * length); }

} // namespace

TEST_CASE("P4.06 on the grid: each step starts within one sample of its beat, at every rate, from 60 to 200 BPM") {
    for (int rate = 0; rate < 8; ++rate)
        for (const double bpm : {60.0, 97.3, 130.0, 174.6, 200.0})
            for (const double from : {0.0, 3.7, -2.15, 1000.03}) {
                auto s = make(steps(Free, rate));
                const double stepBeats = kStepBeats[rate], perSample = bpm / (60.0 * kRate);
                const long long first = static_cast<long long>(std::floor(from / stepBeats + 1e-9));
                const int count = rate <= 1 ? 6 : 20;
                Clock c{from, bpm};
                const auto starts = run(s, c, static_cast<long long>(((first + count) * stepBeats - from) / perSample));
                INFO("rate " << kRateNames[rate] << ", " << bpm << " BPM, from beat " << from);
                REQUIRE_MESSAGE(starts.size() == static_cast<std::size_t>(count),
                                "the clock ran from beat " << from << " through " << count - 1
                                                           << " step lengths: want " << count << " step starts, "
                                                           << "the first at once; got " << starts.size());
                CHECK_MESSAGE(starts[0].sample == 0, "the step the clock starts in must sound at once");
                double worst = 0;
                int wrongStep = 0;
                for (int k = 0; k < count; ++k) {
                    const auto& st = starts[static_cast<std::size_t>(k)];
                    wrongStep += st.step == position(first + k, 8) ? 0 : 1;
                    if (k == 0)
                        continue;
                    const double exact = ((first + k) * stepBeats - from) / perSample;
                    worst = std::max(worst, std::fabs(static_cast<double>(st.sample) - exact));
                    CHECK_MESSAGE(static_cast<double>(st.sample) >= exact - 1e-3,
                                  "count " << first + k << " started at sample " << st.sample
                                           << ", before its beat (sample " << exact << ")");
                }
                CHECK_MESSAGE(worst < 1.0, "a step started " << worst << " samples from its beat; within one sample");
                CHECK_MESSAGE(wrongStep == 0, wrongStep << " starts had the wrong step: count c plays step c mod "
                                                           "length, counting from beat 0");
            }

    // The host's position, block by block, jittering by up to 0.4 of a sample: still every step once.
    auto s = make(steps(Free, 4));
    std::uint32_t state = 5;
    auto next = [&]() {
        state = state * 1664525u + 1013904223u;
        return static_cast<double>(state >> 8) / 16777216.0;
    };
    const double perSample = 130.0 / (60.0 * kRate);
    const auto end = static_cast<long long>(16.0 / perSample); // the last sample before beat 16
    long long sample = 0;
    std::vector<int> seen;
    while (sample < end) {
        const auto block = std::min(end - sample, 1 + static_cast<long long>(next() * 700));
        const double jitter = sample == 0 ? 0.0 : (next() - 0.5) * 0.8 * perSample;
        double beat = static_cast<double>(sample) * perSample + jitter;
        for (long long n = 0; n < block; ++n, ++sample, beat += perSample) {
            undertow::SeqStep st;
            if (s.tick(beat, true, 130.0, st))
                seen.push_back(st.step);
        }
    }
    bool inOrder = seen.size() == 64;
    for (std::size_t k = 0; inOrder && k < seen.size(); ++k)
        inOrder = seen[k] == static_cast<int>(k % 8);
    CHECK_MESSAGE(inOrder, "16 beats at 1/16 with a host position that jitters by under half a sample must start "
                           "exactly 64 steps, in order; got "
                               << seen.size());
}

TEST_CASE("P4.07 Free plays whenever the clock runs, rooted on seq_root and then on the last key pressed") {
    auto p = steps(Free, 2, 5);
    spec::set(p, Step2Vel, 0.0f); // a rest still starts its step, at velocity 0
    auto s = make(p);
    Clock c;
    const long long beat = static_cast<long long>(kRate / 2); // a quarter note at 120 BPM
    auto starts = run(s, c, 6 * beat);
    REQUIRE_MESSAGE(starts.size() == 6u, "6 beats at 1/4 must start 6 steps without any key; got " << starts.size());
    for (int k = 0; k < 6; ++k) {
        const auto& st = starts[static_cast<std::size_t>(k)];
        CHECK_MESSAGE(st.step == k % 5, "with seq_length 5 the steps run 1 to 5 and round again");
        CHECK_MESSAGE(st.note == 36 + k % 5, "before any key the root is seq_root (36) and the note root + pitch; "
                                             "count "
                                                 << k << " played " << st.note);
    }
    CHECK_MESSAGE(starts[1].velocity == 0.0f, "a rest starts its step at velocity 0");
    CHECK_MESSAGE(starts[2].velocity == doctest::Approx(0.5f), "each step carries its velocity");

    // A key mid-step changes the root for the next step, not at once; the root stays after the key comes up.
    run(s, c, beat / 2);
    s.keyOn(50);
    auto after = run(s, c, beat / 2 + 1);
    REQUIRE_MESSAGE(after.size() == 1u, "a key in Free mode starts no step of its own: want the next step only");
    CHECK_MESSAGE(after[0].note == 50 + position(7, 5), "after key 50 the root is 50; got note " << after[0].note);
    s.keyOff(50);
    after = run(s, c, beat);
    REQUIRE(after.size() == 1u);
    CHECK_MESSAGE(after[0].note == 50 + position(8, 5), "in Free mode the root stays on the last key pressed after "
                                                        "it comes up; got note "
                                                            << after[0].note);
    // Notes stay within MIDI: a root of 120 with pitch +12, a root of 3 with pitch -12.
    auto q = steps(Free, 4, 1);
    spec::set(q, Step1Pitch, 12);
    auto high = make(q);
    high.keyOn(120);
    Clock hc;
    const auto h = run(high, hc, 100);
    REQUIRE(h.size() == 1u);
    CHECK_MESSAGE(h[0].note == 127, "root 120 plus 12 semitones is held at note 127; got " << h[0].note);
    spec::set(q, Step1Pitch, -12);
    auto low = make(q);
    low.keyOn(3);
    Clock lc;
    const auto l = run(low, lc, 100);
    REQUIRE(l.size() == 1u);
    CHECK_MESSAGE(l[0].note == 0, "root 3 less 12 semitones is held at note 0; got " << l[0].note);

    // The drift's guard at E1 reads the root each step plays on: on key D2 (38), with the sub an octave down, a
    // fourth down (to A1, 33) would put the sub at 27.5 Hz, so it must go up a fifth (to A2, 45) instead.
    auto g = steps(Free, 5); // 1/32: 16 arcs in 128 s at 120 BPM
    for (int i = 0; i < undertow::kSteps; ++i)
        spec::set(g, Step1Pitch + i, 0);
    spec::set(g, SeqDrift, 1.0f);
    spec::set(g, SeqRoot, 50); // on D3 the fourth down would be allowed: the guard must judge the root played
    auto guard = make(g);
    guard.keyOn(38);
    Clock gc;
    int below = 0, fifths = 0;
    for (long long n = 0; n < 128LL * 48000; ++n, ++gc.sample) {
        undertow::SeqStep st;
        if (guard.tick(gc.beat(), gc.running, gc.bpm, st)) {
            below += st.note == 33 ? 1 : 0;
            fifths += st.note == 45 ? 1 : 0;
        }
    }
    CHECK_MESSAGE((below == 0 && fifths > 0), "on key D2 the drift must judge its E1 guard on the root it plays, 38, "
                                              "not seq_root: fourths down to note 33 "
                                                  << below << ", fifths up to 45 " << fifths);
}

TEST_CASE("P4.08 Key plays while a key is held, rooted on the latest key held; a key from silence sounds at once") {
    auto s = make(steps(Key, 4));
    Clock c;
    const long long step = static_cast<long long>(kRate / 8); // a 1/16 at 120 BPM
    CHECK_MESSAGE(run(s, c, 4 * step).empty(), "in Key mode no step may start while no key is held");
    CHECK_MESSAGE(s.step() == -1, "with no key held step() must be -1; got " << s.step());

    run(s, c, step / 3); // a third into count 4
    s.keyOn(45);
    auto starts = run(s, c, step - step / 3 + 1); // through count 5's first sample
    REQUIRE_MESSAGE(starts.size() == 2u, "a key pressed from silence must sound its step at once, then the next step "
                                         "on the grid; got "
                                             << starts.size() << " starts");
    CHECK_MESSAGE(starts[0].sample == 4 * step + step / 3, "the key's own step must start at the key's sample");
    CHECK_MESSAGE(starts[0].step == 4, "the key came a third into count 4: step 5 must sound; got step "
                                           << starts[0].step + 1);
    CHECK_MESSAGE(starts[0].note == 45 + 4, "the root is the key held (45); got note " << starts[0].note);
    CHECK_MESSAGE(starts[1].sample == 5 * step, "then count 5 starts on its beat");
    CHECK_MESSAGE((s.count() == 5 && s.root() == 45), "count() and root() are the playing step's count and root: "
                                                       "want 5 and 45, got "
                                                           << s.count() << " and " << s.root());

    s.keyOn(52); // a second key while 45 is held: the root moves, nothing starts
    starts = run(s, c, step / 2);
    CHECK_MESSAGE(starts.empty(), "a second key while one is held must start nothing at once");
    CHECK_MESSAGE(s.root() == 45, "root() is the root of the step playing, 45, until the next step starts; got "
                                      << s.root());
    starts = run(s, c, step / 2 + 1);
    REQUIRE(starts.size() == 1u);
    CHECK_MESSAGE(starts[0].note == 52 + 6, "the root is the latest key held (52); got note " << starts[0].note);
    CHECK_MESSAGE((s.count() == 6 && s.root() == 52), "after the next step, count() 6 and root() 52; got "
                                                          << s.count() << " and " << s.root());
    s.keyOff(52);
    starts = run(s, c, step);
    REQUIRE(starts.size() == 1u);
    CHECK_MESSAGE(starts[0].note == 45 + 7, "with 52 up the root goes back to 45, still held; got note "
                                                << starts[0].note);
    s.keyOff(45);
    CHECK_MESSAGE(run(s, c, 4 * step).empty(), "with every key up the pattern must stop");
    CHECK_MESSAGE(s.step() == -1, "with every key up step() must be -1");
    CHECK_MESSAGE((s.count() == 0 && s.root() == -1), "with nothing playing count() is 0 and root() -1");

    s.keyOn(40);
    starts = run(s, c, 1);
    CHECK_MESSAGE(starts.size() == 1u, "a key from silence must sound its step at once, every time");
    s.keyOn(41);
    s.allKeysOff();
    CHECK_MESSAGE(run(s, c, 4 * step).empty(), "allKeysOff() must stop the pattern in Key mode");
}

TEST_CASE("P4.09 a loop plays its steps again exactly, drift included; a jump into a step sounds it at once") {
    // The default steps and drift at 1/16: beats 16 to 20 are the middle of the first arc, where two steps drift.
    auto p = spec::defaults();
    spec::set(p, SeqMode, Free);
    auto straight = make(p);
    Clock sc;
    const double perSample = 120.0 / (60.0 * kRate);
    const long long bar = std::llround(4.0 / perSample);
    run(straight, sc, std::llround(16.0 / perSample));
    const long long barStart = sc.sample;
    auto want = run(straight, sc, bar);
    for (auto& st : want)
        st.sample -= barStart;
    undertow::Pattern pattern;
    pattern.setParams(p.data());
    int drifted = 0;
    for (long long k = 64; k < 80; ++k)
        drifted += (pattern.at(k, 43).pitch != pattern.home(position(k, 8)).pitch ||
                    pattern.at(k, 43).velocity != pattern.home(position(k, 8)).velocity)
                       ? 1
                       : 0;
    REQUIRE_MESSAGE(drifted > 0, "the test needs a bar where the default drift is away from home");
    REQUIRE_MESSAGE(want.size() == 16u, "4 beats at 1/16 must start 16 steps; got " << want.size());
    int unlike = 0;
    for (std::size_t k = 0; k < want.size(); ++k) {
        const auto played = pattern.at(64 + static_cast<long long>(k), 43);
        unlike += (want[k].note == 43 + played.pitch && want[k].velocity == played.velocity) ? 0 : 1;
    }
    CHECK_MESSAGE(unlike == 0, unlike << " of the bar's steps did not play what the pattern's count plays, drift "
                                         "included: the note is the root plus at(count)'s pitch, at its velocity");

    auto looped = make(p);
    for (int pass = 0; pass < 3; ++pass) {
        Clock c{16.0};
        auto got = run(looped, c, bar);
        INFO("pass " << pass + 1 << " of a loop of beats 16 to 20");
        REQUIRE_MESSAGE(got.size() == want.size(), "the loop started " << got.size() << " steps; the bar played straight "
                                                                       "through starts "
                                                                    << want.size());
        int differ = 0;
        for (std::size_t k = 0; k < got.size(); ++k)
            differ += (got[k].sample == want[k].sample && got[k].step == want[k].step &&
                       got[k].note == want[k].note && got[k].velocity == want[k].velocity)
                          ? 0
                          : 1;
        CHECK_MESSAGE(differ == 0, differ << " steps of the loop differ from the bar played straight through: the "
                                             "drift is a function of the host's position");
    }

    // A loop as long as one step (a bar at the 1 bar rate) starts its step again at every jump.
    auto one = make(steps(Free, 0));
    const long long wholeBar = static_cast<long long>(4.0 * 60.0 / 120.0 * kRate);
    int jumps = 0;
    for (int pass = 0; pass < 3; ++pass) {
        Clock c;
        jumps += static_cast<int>(run(one, c, wholeBar).size());
    }
    CHECK_MESSAGE(jumps == 3, "a one-bar loop at the 1 bar rate must start its step at every pass: 3 passes, got "
                                  << jumps << " starts");

    // A jump forward into the middle of another step sounds that step at once; a jump within a step does not.
    auto fwd = make(steps(Free, 2));
    Clock c;
    run(fwd, c, 1000);
    c.jump(7.3);
    auto st = run(fwd, c, 10);
    REQUIRE_MESSAGE(st.size() == 1u, "a jump to beat 7.3 must start count 7 at once");
    CHECK_MESSAGE(st[0].step == 7, "beat 7.3 at 1/4 is count 7, step 8; got step " << st[0].step + 1);
    c.jump(7.6);
    CHECK_MESSAGE(run(fwd, c, 10).empty(), "a jump forward within the step that is playing starts nothing");
}

TEST_CASE("P4.10 with the host stopped a held key plays from step 1, counting from the key at the host's tempo") {
    for (const Mode mode : {Key, Free}) {
        INFO((mode == Key ? "Key mode" : "Free mode"));
        auto s = make(steps(mode, 4));
        Clock c{0.05, 96.0, false}; // stopped at beat 0.05, at 96 BPM
        CHECK_MESSAGE(run(s, c, 2000).empty(), "with the host stopped and no key, nothing plays");
        s.keyOn(43);
        const long long step = 7500; // a 1/16 at the host's 96 BPM
        auto starts = run(s, c, 8 * step + 10);
        REQUIRE_MESSAGE(starts.size() == 9u, "a held key must play a step every 1/16 from the key: want 9 starts in "
                                             "8 steps' time at the host's 96 BPM, got "
                                                 << starts.size());
        for (int k = 0; k < 9; ++k) {
            const auto& st = starts[static_cast<std::size_t>(k)];
            CHECK_MESSAGE(std::llabs(st.sample - (2000 + k * step)) <= 1,
                          "step " << k + 1 << " of the audition started at sample " << st.sample << "; want "
                                  << 2000 + k * step << ", counting from the key");
            CHECK_MESSAGE(st.step == k % 8, "the audition counts steps from step 1 at the key");
            CHECK_MESSAGE(st.note == 43 + k % 8, "rooted on the key held");
        }
        CHECK_MESSAGE((s.count() == 8 && s.root() == 43), "the audition counts from 0 at the key: after its ninth step "
                                                          "count() must be 8 (root 43); got "
                                                              << s.count() << " (root " << s.root() << ")");
        s.keyOff(43);
        CHECK_MESSAGE(run(s, c, 20000).empty(), "with the key up the audition stops");
        s.keyOn(43);
        starts = run(s, c, 10);
        REQUIRE_MESSAGE(starts.size() == 1u, "the next key starts the audition again at once");
        CHECK_MESSAGE(starts[0].step == 0, "the next key starts again from step 1; got step " << starts[0].step + 1);

        // The host starts under the held key, a little further on and in the count the audition is in: its grid
        // takes over at once.
        run(s, c, step / 2);
        c.play(0.1);
        starts = run(s, c, step);
        REQUIRE_MESSAGE(!starts.empty(), "when the host starts, the step at its position must sound at once");
        CHECK_MESSAGE(starts[0].sample == c.sample - step, "the host's step must start on the first sample it plays, "
                                                           "even in the count the audition was in");
        CHECK_MESSAGE(starts[0].step == 0, "beat 0.1 at 1/16 is count 0, step 1; got step " << starts[0].step + 1);
        // And when it stops again under the key, the audition starts again from step 1, at once, and counts from there.
        c.stop();
        starts = run(s, c, step + 10);
        REQUIRE_MESSAGE(starts.size() == 2u, "when the host stops under a held key, the audition must start at once "
                                             "from step 1, and step 2 follow a step later; got "
                                                 << starts.size() << " starts in a step's time");
        CHECK_MESSAGE((starts[0].step == 0 && starts[1].step == 1), "the audition starts from step 1");
        CHECK_MESSAGE(std::llabs(starts[1].sample - starts[0].sample - step) <= 1,
                      "the audition's step 2 came " << starts[1].sample - starts[0].sample
                                                    << " samples after step 1; it counts from the stop, a step of "
                                                    << step << " samples");
    }
}

TEST_CASE("P4.11 step() shows the step playing, -1 when not; Notes mode plays nothing; reset() forgets the keys") {
    auto s = make(steps(Free, 4));
    Clock c;
    const long long step = static_cast<long long>(kRate / 8);
    run(s, c, 2 * step + step / 2);
    CHECK_MESSAGE(s.step() == 2, "a half-step into count 2, step() must be 2; got " << s.step());
    CHECK_MESSAGE((s.count() == 2 && s.root() == 36), "count() and root() of the step playing: want 2 and seq_root 36; "
                                                      "got "
                                                          << s.count() << " and " << s.root());
    s.keyOn(50);
    s.reset();
    CHECK_MESSAGE(s.step() == -1, "after reset() nothing plays yet: step() must be -1");
    CHECK_MESSAGE((s.count() == 0 && s.root() == -1), "after reset() count() is 0 and root() -1");
    auto starts = run(s, c, 1);
    REQUIRE(starts.size() == 1u);
    CHECK_MESSAGE(starts[0].note == 36 + 2, "after reset() Free mode is rooted on seq_root again; got note "
                                                << starts[0].note);

    auto n = make(steps(Notes, 4));
    n.keyOn(40);
    Clock nc;
    CHECK_MESSAGE(run(n, nc, 4 * step).empty(), "in Notes mode the sequencer starts no step");
    CHECK_MESSAGE(n.step() == -1, "in Notes mode step() must be -1");

    // No allocation in anything the audio thread calls.
    auto a = make(steps(Key, 5, 6, 1.0f));
    const auto p = steps(Free, 6, 7, 0.9f);
    Clock ac{0.0, 140.0};
    int started = 0;
    spec::AllocationCounter counter;
    for (int k = 0; k < 40; ++k) {
        a.keyOn(30 + k);
        a.setParams(p.data());
        started += countStarts(a, ac, 500);
        a.keyOff(30 + k);
        a.allKeysOff();
    }
    const std::size_t allocations = counter.count();
    CHECK(started > 0);
    CHECK_MESSAGE(allocations == 0, allocations << " heap allocations in setParams, keyOn, keyOff, allKeysOff or "
                                                   "tick: they run on the audio thread");
}
