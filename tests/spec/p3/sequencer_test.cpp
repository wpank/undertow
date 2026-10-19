// Phase 3 spec tests: the hypnotic sequencer (docs/specs/phase-3-sequencer.md, P3.01 to P3.09).
// spec-needs: core/include/undertow/sequencer.h

#include "doctest.h"

#include "spec_alloc.h"
#include "spec_engine.h"
#include "undertow/sequencer.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <vector>

namespace {

using Type = undertow::SeqEvent::Type;
constexpr double kRate = 48000.0;
constexpr double kStep = 5625.0; // a 1/16 at 128 BPM, in samples

struct Event {
    long long at; // absolute sample
    Type type;
    int note;
    bool accent;
};

spec::Params seqPatch() {
    auto p = spec::defaults();
    spec::set(p, "seq_on", 1);
    spec::set(p, "seq_rate", 1); // 1/16
    spec::set(p, "seq_length", 16);
    spec::set(p, "seq_gate", 0.5f);
    spec::set(p, "seq_swing", 0);
    spec::set(p, "seq_root", 33);
    spec::set(p, "seq_scale", 1);
    spec::set(p, "seq_seed", 1);
    spec::set(p, "seq_evolve", 0);
    return p;
}

undertow::Pattern flat(int semitones = 0) {
    undertow::Pattern p;
    for (auto& step : p.steps)
        step = undertow::Step{semitones, true, false, false, 1.0f};
    return p;
}

undertow::Sequencer make(const spec::Params& params, const undertow::Pattern& pattern) {
    undertow::Sequencer seq;
    seq.prepare(kRate);
    seq.setParams(params.data());
    seq.setPattern(pattern);
    seq.reset();
    return seq;
}

// Run the sequencer over `samples` samples in blocks of `block`, starting at beat `ppq`, at 128 BPM.
std::vector<Event> run(undertow::Sequencer& seq, long long samples, double ppq = 0.0, int block = 333,
                       bool running = true, std::vector<int>* steps = nullptr) {
    std::vector<Event> events;
    undertow::SeqEvent out[64];
    for (long long start = 0; start < samples; start += block) {
        const int n = static_cast<int>(std::min<long long>(block, samples - start));
        const double at = ppq + static_cast<double>(start) / (60.0 / 128.0 * kRate);
        const int count = seq.process(at, 128.0, running, n, out, 64);
        for (int i = 0; i < count; ++i)
            events.push_back({start + out[i].sampleOffset, out[i].type, out[i].note, out[i].accent});
        if (steps != nullptr)
            steps->push_back(seq.currentStep());
    }
    return events;
}

std::vector<Event> only(const std::vector<Event>& events, Type type) {
    std::vector<Event> out;
    for (const auto& e : events)
        if (e.type == type)
            out.push_back(e);
    return out;
}

} // namespace

TEST_CASE("P3.01 steps fall on the host's 1/16 grid to the sample, and notes last seq_gate of a step") {
    auto seq = make(seqPatch(), flat());
    std::vector<int> steps;
    const auto events = run(seq, static_cast<long long>(64 * kStep), 0.0, 333, true, &steps);
    const auto ons = only(events, Type::NoteOn), offs = only(events, Type::NoteOff);
    REQUIRE_MESSAGE(ons.size() == 64, "4 bars of 1/16 steps with every gate on must give 64 note-ons; got " << ons.size());
    for (std::size_t k = 0; k < ons.size(); ++k)
        CHECK_MESSAGE(std::llabs(ons[k].at - std::llround(k * kStep)) <= 1,
                      "step " << k << " started at sample " << ons[k].at << "; at 128 BPM a 1/16 lasts 5625 samples, so it must "
                                                                            "start at "
                              << std::llround(k * kStep) << " (blocks of 333 samples: offsets inside a block count)");
    REQUIRE(offs.size() >= 63);
    for (std::size_t k = 0; k < 63; ++k)
        CHECK_MESSAGE(std::llabs(offs[k].at - std::llround(k * kStep + 0.5 * kStep)) <= 1,
                      "with seq_gate 0.5 step " << k << "'s note must end half a step in (sample "
                                                << std::llround(k * kStep + 0.5 * kStep) << "); it ended at " << offs[k].at);
    CHECK_MESSAGE(ons[0].note == 33, "a step at 0 semitones plays the root (seq_root 33); it played " << ons[0].note);
    // currentStep() after each block: the step whose time it is.
    bool stepsRight = true;
    for (std::size_t b = 0; b < steps.size(); ++b) {
        const long long last = std::min(static_cast<long long>(b + 1) * 333, static_cast<long long>(64 * kStep)) - 1;
        const int want = static_cast<int>(static_cast<long long>(static_cast<double>(last) / kStep) % 16);
        stepsRight = stepsRight && steps[b] == want;
    }
    CHECK_MESSAGE(stepsRight, "currentStep() must be the step playing (index modulo seq_length) after each block");
}

TEST_CASE("P3.02 seq_rate, seq_length and seq_swing") {
    auto p = seqPatch();
    spec::set(p, "seq_rate", 0); // 1/8
    auto eighth = make(p, flat());
    const auto e8 = only(run(eighth, static_cast<long long>(16 * kStep)), Type::NoteOn);
    REQUIRE(e8.size() == 8);
    CHECK_MESSAGE(std::llabs(e8[3].at - std::llround(3 * 2 * kStep)) <= 1, "at 1/8 steps are 11250 samples apart");

    auto lengthPatch = seqPatch();
    spec::set(lengthPatch, "seq_length", 5);
    undertow::Pattern ramp = flat();
    for (int i = 0; i < 16; ++i)
        ramp.steps[static_cast<std::size_t>(i)].semitones = i;
    auto five = make(lengthPatch, ramp);
    const auto e5 = only(run(five, static_cast<long long>(12 * kStep)), Type::NoteOn);
    REQUIRE(e5.size() == 12);
    const int want[] = {33, 34, 35, 36, 37, 33, 34, 35, 36, 37, 33, 34};
    for (int k = 0; k < 12; ++k)
        CHECK_MESSAGE(e5[static_cast<std::size_t>(k)].note == want[k],
                      "with seq_length 5 the steps cycle 0..4: step " << k << " must play " << want[k] << ", not "
                                                                     << e5[static_cast<std::size_t>(k)].note);

    auto swingPatch = seqPatch();
    spec::set(swingPatch, "seq_swing", 1);
    auto swung = make(swingPatch, flat());
    const auto es = only(run(swung, static_cast<long long>(8 * kStep)), Type::NoteOn);
    REQUIRE(es.size() == 8);
    CHECK_MESSAGE(std::llabs(es[2].at - std::llround(2 * kStep)) <= 1, "swing leaves even steps on the grid");
    CHECK_MESSAGE(std::llabs(es[3].at - std::llround(3 * kStep + kStep / 3.0)) <= 1,
                  "swing 1 delays odd steps by a third of a step (1875 samples at 1/16, 128 BPM): step 3 started at "
                      << es[3].at << ", want " << std::llround(3 * kStep + kStep / 3.0));
}

TEST_CASE("P3.03 rests, accents and slides") {
    undertow::Pattern p = flat();
    p.steps[1].gate = false;
    p.steps[2].accent = true;
    p.steps[4].slide = true;
    p.steps[4].semitones = 0;
    p.steps[5].semitones = 7;
    p.steps[8].slide = true;
    p.steps[9].gate = false;
    auto seq = make(seqPatch(), p);
    const auto events = run(seq, static_cast<long long>(16 * kStep), 0.0, 333);
    const auto ons = only(events, Type::NoteOn);
    REQUIRE_MESSAGE(ons.size() == 14, "16 steps with two rests must give 14 note-ons; got " << ons.size());
    CHECK_MESSAGE(std::llabs(ons[1].at - std::llround(2 * kStep)) <= 1, "a rest (step 1) must play nothing");
    CHECK_MESSAGE(ons[1].accent, "step 2's accent must reach its note-on");
    CHECK_MESSAGE(!ons[0].accent, "an unaccented step's note-on has no accent");
    // Step 4 slides into step 5: the next note starts before step 4's note ends, at the same sample.
    long long on5 = -1, off4 = -1;
    std::size_t on5Index = 0, off4Index = 0;
    for (std::size_t i = 0; i < events.size(); ++i) {
        if (events[i].type == Type::NoteOn && events[i].note == 40 && on5 < 0) {
            on5 = events[i].at;
            on5Index = i;
        }
        if (events[i].type == Type::NoteOff && events[i].note == 33 && events[i].at > static_cast<long long>(4 * kStep) &&
            events[i].at <= static_cast<long long>(5 * kStep) + 1 && off4 < 0) {
            off4 = events[i].at;
            off4Index = i;
        }
    }
    REQUIRE_MESSAGE(on5 >= 0, "step 5 (7 semitones, note 40) never played");
    CHECK_MESSAGE(std::llabs(on5 - std::llround(5 * kStep)) <= 1, "step 5 must start on its grid point");
    CHECK_MESSAGE(off4 == on5, "a slide step's note must be held into the next step and end at the same sample as the next "
                               "note starts (got the end at "
                                   << off4 << " and the start at " << on5 << ")");
    CHECK_MESSAGE(on5Index < off4Index, "on a slide the next note-on must come before the old note-off (legato)");
    // Step 8 slides into a rest: its note ends at step 9's start.
    bool releasedAtRest = false;
    for (const auto& e : events)
        releasedAtRest = releasedAtRest || (e.type == Type::NoteOff && std::llabs(e.at - std::llround(9 * kStep)) <= 1);
    CHECK_MESSAGE(releasedAtRest, "a slide into a rest must end its note when the rest begins");
}

TEST_CASE("P3.04 stops, loops and starts in the middle of a step") {
    auto seq = make(seqPatch(), flat());
    run(seq, static_cast<long long>(kStep * 0.25));
    undertow::SeqEvent out[16];
    const int stopped = seq.process(1.0, 128.0, false, 256, out, 16);
    REQUIRE_MESSAGE(stopped == 1, "stopping the transport with a note playing must send exactly one note-off; got " << stopped);
    CHECK_MESSAGE((out[0].type == Type::NoteOff && out[0].sampleOffset == 0), "the note-off must come at the stop's first sample");
    CHECK_MESSAGE(seq.currentStep() == -1, "a stopped sequencer reports step -1");
    CHECK_MESSAGE(seq.process(1.0, 128.0, false, 256, out, 16) == 0, "a stopped sequencer must send nothing");

    // A host loop: play from beat 3.9 to 4.1, then jump back to 0.
    auto loop = make(seqPatch(), flat());
    run(loop, static_cast<long long>(0.2 * 22500), 3.9);
    const int jumped = loop.process(0.0, 128.0, true, 256, out, 16);
    bool off = false, on = false;
    for (int i = 0; i < jumped; ++i) {
        off = off || (out[i].type == Type::NoteOff && out[i].sampleOffset == 0);
        on = on || (out[i].type == Type::NoteOn && out[i].sampleOffset == 0);
    }
    CHECK_MESSAGE(off, "a jump back in the host's loop must end the note that was playing");
    CHECK_MESSAGE(on, "a jump that lands exactly on a step must play that step at once");
    CHECK_MESSAGE(loop.currentStep() == 0, "after a jump to beat 0 the step is 0");

    auto late = make(seqPatch(), flat());
    const auto events = run(late, static_cast<long long>(kStep), 0.1, 333);
    REQUIRE_MESSAGE(!only(events, Type::NoteOn).empty(), "starting mid-step, the next step must still play");
    CHECK_MESSAGE(std::llabs(only(events, Type::NoteOn)[0].at - std::llround(0.15 * 22500)) <= 1,
                  "starting at beat 0.1, nothing plays until the next step (beat 0.25)");
}

TEST_CASE("P3.05 probability is decided by seed and step position, so loops repeat exactly") {
    undertow::Pattern half = flat();
    for (auto& step : half.steps)
        step.probability = 0.5f;
    auto a = make(seqPatch(), half);
    const auto first = only(run(a, static_cast<long long>(4096 * kStep), 0.0, 4096), Type::NoteOn);
    const double share = static_cast<double>(first.size()) / 4096.0;
    CHECK_MESSAGE(share >= 0.45, "probability 0.5 must play about half of the steps; it played " << share);
    CHECK_MESSAGE(share <= 0.55, "probability 0.5 must play about half of the steps; it played " << share);
    auto b = make(seqPatch(), half);
    const auto second = only(run(b, static_cast<long long>(4096 * kStep), 0.0, 4096), Type::NoteOn);
    bool same = first.size() == second.size();
    for (std::size_t i = 0; same && i < first.size(); ++i)
        same = first[i].at == second[i].at;
    CHECK_MESSAGE(same, "the same seed must play the same steps: probability is a function of (seq_seed, step number)");
    auto p = seqPatch();
    spec::set(p, "seq_seed", 2);
    auto c = make(p, half);
    const auto third = only(run(c, static_cast<long long>(256 * kStep), 0.0, 4096), Type::NoteOn);
    std::set<long long> one, two;
    for (const auto& e : first)
        if (e.at < static_cast<long long>(256 * kStep))
            one.insert(e.at);
    for (const auto& e : third)
        two.insert(e.at);
    std::size_t differ = 0;
    for (long long k = 0; k < 256; ++k) {
        const long long at = std::llround(k * kStep);
        differ += (one.count(at) != two.count(at)) ? 1 : 0;
    }
    CHECK_MESSAGE(differ >= 50, "a different seed must decide differently (only " << differ << " of 256 steps differ)");
    // A replayed position plays what it played before.
    auto d = make(seqPatch(), half);
    const auto x = only(run(d, static_cast<long long>(16 * kStep), 8.0, 4096), Type::NoteOn);
    const auto y = only(run(d, static_cast<long long>(16 * kStep), 8.0, 4096), Type::NoteOn);
    CHECK_MESSAGE(x.size() == y.size(), "looping over the same bar must replay the same probabilistic steps");
    for (float probability : {0.0f, 1.0f}) {
        undertow::Pattern fixed = flat();
        for (auto& step : fixed.steps)
            step.probability = probability;
        auto e = make(seqPatch(), fixed);
        const auto played = only(run(e, static_cast<long long>(64 * kStep)), Type::NoteOn);
        CHECK_MESSAGE(played.size() == (probability > 0.5f ? 64u : 0u), "probability " << probability << " played "
                                                                                      << played.size() << " of 64 steps");
    }
}

TEST_CASE("P3.06 notes are root + transpose + the step's semitones") {
    undertow::Pattern p = flat();
    const int offsets[] = {0, 3, 7, 12};
    for (int i = 0; i < 16; ++i)
        p.steps[static_cast<std::size_t>(i)].semitones = offsets[i % 4];
    auto seq = make(seqPatch(), p);
    auto ons = only(run(seq, static_cast<long long>(4 * kStep)), Type::NoteOn);
    REQUIRE(ons.size() == 4);
    const int want[] = {33, 36, 40, 45};
    for (int i = 0; i < 4; ++i)
        CHECK_MESSAGE(ons[static_cast<std::size_t>(i)].note == want[i], "root 33 + " << offsets[i] << " must play " << want[i]);
    seq.setTranspose(5);
    ons = only(run(seq, static_cast<long long>(4 * kStep), 1.0), Type::NoteOn);
    REQUIRE(ons.size() >= 4);
    CHECK_MESSAGE(ons[0].note == 38, "setTranspose(5) must move every note up 5 semitones (33 to 38); got " << ons[0].note);
}

TEST_CASE("P3.07 mutate changes about `amount` of the steps, deterministically, in scale and within an octave") {
    CHECK(undertow::inScale(undertow::Scale::Minor, 3));
    CHECK(!undertow::inScale(undertow::Scale::Minor, 4));
    CHECK(undertow::inScale(undertow::Scale::Minor, -2));
    CHECK(undertow::inScale(undertow::Scale::Phrygian, 1));
    CHECK(!undertow::inScale(undertow::Scale::Dorian, 8));
    CHECK(undertow::inScale(undertow::Scale::Dorian, 9));
    CHECK(!undertow::inScale(undertow::Scale::MinorPentatonic, 2));
    CHECK(undertow::inScale(undertow::Scale::Chromatic, 6));

    auto base = make(seqPatch(), undertow::defaultPattern());
    base.mutate(0.0f);
    CHECK_MESSAGE(base.pattern() == undertow::defaultPattern(), "mutate(0) must change nothing");

    double changed = 0;
    const undertow::Pattern defaults = undertow::defaultPattern();
    for (int seed = 1; seed <= 200; ++seed) {
        auto p = seqPatch();
        spec::set(p, "seq_seed", static_cast<float>(seed));
        auto seq = make(p, undertow::defaultPattern());
        seq.mutate(0.25f);
        const auto& after = seq.pattern();
        for (int i = 0; i < 16; ++i) {
            const auto& was = defaults.steps[static_cast<std::size_t>(i)];
            const auto& now = after.steps[static_cast<std::size_t>(i)];
            changed += now != was ? 1 : 0;
            CHECK_MESSAGE(undertow::inScale(undertow::Scale::Minor, now.semitones),
                          "mutate put step " << i << " on " << now.semitones << " semitones, outside the minor scale");
            CHECK_MESSAGE(std::abs(now.semitones) <= 12, "mutate must keep notes within an octave of the root");
            CHECK_MESSAGE(now.gate == was.gate, "mutate must not touch gates (rests stay where the person put them)");
        }
        auto again = make(p, undertow::defaultPattern());
        again.mutate(0.25f);
        CHECK_MESSAGE(again.pattern() == after, "the same seed and pattern must mutate the same way");
    }
    const double share = changed / (200.0 * 16.0);
    CHECK_MESSAGE(share >= 0.19, "mutate(0.25) changed " << share << " of the steps on average; want 0.25 +- 0.06");
    CHECK_MESSAGE(share <= 0.31, "mutate(0.25) changed " << share << " of the steps on average; want 0.25 +- 0.06");
}

TEST_CASE("P3.08 evolve mutates at bar boundaries only") {
    auto p = seqPatch();
    spec::set(p, "seq_evolve", 1); // every bar
    spec::set(p, "seq_mutate", 0.5f);
    auto seq = make(p, undertow::defaultPattern());
    // Each run continues exactly where the last one ended (a whole number of samples, 22500 a beat).
    long long done = 0;
    auto playOn = [&](long long samples) {
        run(seq, samples, static_cast<double>(done) / 22500.0);
        done += samples;
    };
    playOn(87187); // 15.5 steps: steps 0 to 15 of the first bar
    CHECK_MESSAGE(seq.pattern() == undertow::defaultPattern(), "evolve must not mutate inside the first bar");
    playOn(5625); // across the bar line at beat 4
    const auto afterBar = seq.pattern();
    CHECK_MESSAGE(afterBar != undertow::defaultPattern(), "with seq_evolve 1 bar and seq_mutate 0.5 the pattern must mutate at "
                                                          "the bar line");
    playOn(78750); // 14 more steps, inside the second bar
    CHECK_MESSAGE(seq.pattern() == afterBar, "evolve must not mutate inside a bar");

    auto off = seqPatch();
    auto still = make(off, undertow::defaultPattern());
    run(still, static_cast<long long>(64 * kStep));
    CHECK_MESSAGE(still.pattern() == undertow::defaultPattern(), "with seq_evolve Off the pattern never changes");
}

TEST_CASE("P3.09 the sequencer never allocates") {
    auto p = seqPatch();
    spec::set(p, "seq_evolve", 1);
    auto seq = make(p, undertow::defaultPattern());
    undertow::SeqEvent out[64];
    std::size_t allocations = 0;
    for (int block = 0; block < 2000; ++block) {
        spec::AllocationCounter counter;
        seq.setParams(p.data());
        seq.process(block * 512 / 22500.0, 128.0, block % 300 != 299, 512, out, 64);
        if (block % 100 == 0) {
            seq.mutate(0.3f);
            seq.setPattern(undertow::defaultPattern());
        }
        allocations += counter.count();
    }
    CHECK_MESSAGE(allocations == 0, allocations << " heap allocations in the sequencer's process, setParams, mutate or setPattern");
}
