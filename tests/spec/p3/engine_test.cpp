// Phase 3 spec tests: the sequencer inside the engine (docs/specs/phase-3-sequencer.md, P3.10 to P3.15).
// spec-needs: core/include/undertow/sequencer.h

#include "doctest.h"

#include "spec_alloc.h"
#include "spec_analysis.h"
#include "spec_engine.h"
#include "undertow/engine.h"
#include "undertow/sequencer.h"

#include <atomic>
#include <cmath>
#include <thread>
#include <vector>

namespace {

constexpr double kRate = 48000.0;
constexpr double kStep = 5625.0; // a 1/16 at 128 BPM

undertow::Transport host(double ppq = 0.0) { return undertow::Transport{true, true, 128.0, ppq}; }

spec::Params seqSub() {
    auto p = spec::subPatch();
    spec::set(p, "seq_on", 1);
    spec::set(p, "seq_sync", 0);
    spec::set(p, "seq_rate", 1);
    spec::set(p, "seq_length", 16);
    spec::set(p, "seq_gate", 0.9f);
    spec::set(p, "seq_root", 33);
    spec::set(p, "seq_evolve", 0);
    spec::set(p, "amp_release", 5);
    return p;
}

undertow::Pattern notes(std::initializer_list<int> semitones) {
    undertow::Pattern p;
    int i = 0;
    for (int s : semitones)
        p.steps[static_cast<std::size_t>(i++)] = undertow::Step{s, true, false, false, 1.0f};
    for (; i < undertow::kSteps; ++i)
        p.steps[static_cast<std::size_t>(i)] = undertow::Step{0, true, false, false, 1.0f};
    return p;
}

undertow::Engine& prepared(undertow::Engine& engine, const spec::Params& patch, const undertow::Pattern& pattern) {
    engine.prepare(kRate, 512);
    spec::apply(engine, patch);
    engine.reset();
    engine.setPattern(pattern);
    return engine;
}

// The sub's pitch in a slice of a step (by default 15% to 85% of it).
double subHzInStep(const spec::Signal& x, int step, double from = 0.15, double to = 0.85) {
    const auto start = static_cast<std::size_t>(step * kStep + from * kStep);
    const auto end = static_cast<std::size_t>(step * kStep + to * kStep);
    return spec::zeroCrossingHz(x, start, end, kRate);
}

} // namespace

TEST_CASE("P3.10 with seq_on the engine plays the pattern on the host's grid, and a MIDI note transposes it") {
    undertow::Engine engine;
    prepared(engine, seqSub(), notes({0, 12, 7, 3}));
    const auto out = spec::render(engine, static_cast<std::size_t>(4 * kStep), {}, host());
    const double want[] = {27.5, 55.0, 27.5 * std::exp2(7.0 / 12.0), 27.5 * std::exp2(3.0 / 12.0)};
    for (int step = 0; step < 4; ++step)
        CHECK_MESSAGE(std::fabs(subHzInStep(out.left, step) - want[step]) <= 0.03 * want[step],
                      "step " << step << " must play root 33 + its semitones (the sub at " << want[step] << " Hz); it plays "
                              << subHzInStep(out.left, step) << " Hz");
    CHECK_MESSAGE(engine.meters().step == 3, "meters().step is " << engine.meters().step << " during step 3");

    // A MIDI note while the sequencer runs only transposes the pattern, and the transposition is latched: 38 = root
    // 33 + 5. The key is released halfway through step 4: that must neither cut step 4's note nor undo the transposition.
    const int release = static_cast<int>(kStep / 2);
    const auto shifted = spec::render(engine, static_cast<std::size_t>(4 * kStep), {spec::noteOn(0, 38), spec::noteOff(release, 38)},
                                      host(1.0));
    const double up5 = 27.5 * std::exp2(5.0 / 12.0);
    const double held = subHzInStep(shifted.left, 0, 0.1, 0.45); // before the release
    CHECK_MESSAGE(std::fabs(held - up5) <= 0.03 * up5,
                  "a MIDI note-on 38 with seq_root 33 must transpose the pattern up 5 semitones; step 4 plays " << held << " Hz");
    CHECK_MESSAGE(std::fabs(subHzInStep(shifted.left, 1) - up5) <= 0.03 * up5,
                  "the transposition is latched: after the key is released, step 5 must still play " << up5 << " Hz; it plays "
                                                                                                       << subHzInStep(shifted.left, 1) << " Hz");
    const double before = spec::peakAbs(shifted.left, static_cast<std::size_t>(0.1 * kStep), static_cast<std::size_t>(release));
    const double after = spec::peakAbs(shifted.left, static_cast<std::size_t>(0.6 * kStep), static_cast<std::size_t>(0.88 * kStep));
    CHECK_MESSAGE(after >= 0.7 * before, "releasing the transposing key must not cut the playing step: while the sequencer runs, MIDI "
                                         "note-offs never reach the voice (step 4's level fell from "
                                             << before << " to " << after << " after the key's release)");

    // The key never plays the voice itself: with every step a rest, holding a key leaves the output silent.
    undertow::Pattern rests = notes({0});
    for (auto& step : rests.steps)
        step.gate = false;
    engine.setPattern(rests);
    const auto quiet = spec::render(engine, static_cast<std::size_t>(4 * kStep), {spec::noteOn(0, 40)}, host(2.0));
    const double leak = spec::rms(quiet.left, static_cast<std::size_t>(kStep), static_cast<std::size_t>(4 * kStep));
    CHECK_MESSAGE(leak < 1e-4, "while the sequencer runs, a MIDI key only transposes the pattern and must not play the voice: "
                               "every step here is a rest, yet a held key 40 sounds (RMS "
                                   << leak << ")");

    engine.setParam(spec::id("seq_on"), 0.0f);
    const auto direct = spec::render(engine, 24000, {spec::noteOn(0, 45)}, host(3.0));
    CHECK_MESSAGE(std::fabs(spec::zeroCrossingHz(direct.left, 4800, 24000, kRate) - 55.0) <= 0.5,
                  "with seq_on off, MIDI plays the voice directly again (note 45: the sub at 55 Hz)");
    CHECK_MESSAGE(engine.meters().step == -1, "with seq_on off, meters().step is -1");
}

TEST_CASE("P3.11 accented steps are louder, and slides glide without retriggering") {
    auto p = seqSub();
    spec::set(p, "accent", 1);
    undertow::Pattern pattern = notes({0, 0, 0, 0});
    pattern.steps[1].accent = true;
    undertow::Engine engine;
    prepared(engine, p, pattern);
    const auto out = spec::render(engine, static_cast<std::size_t>(4 * kStep), {}, host());
    const double plain = spec::peakAbs(out.left, static_cast<std::size_t>(0.2 * kStep), static_cast<std::size_t>(0.8 * kStep));
    const double loud = spec::peakAbs(out.left, static_cast<std::size_t>(1.2 * kStep), static_cast<std::size_t>(1.8 * kStep));
    CHECK_MESSAGE(loud >= 1.7 * plain, "an accented step (accent 1) must be about twice as loud as a plain one; ratio " << loud / plain);

    auto s = seqSub();
    spec::set(s, "glide_ms", 40);
    spec::set(s, "glide_mode", 1);
    spec::set(s, "amp_attack", 30);
    spec::set(s, "amp_decay", 30);
    spec::set(s, "amp_sustain", 0.4f);
    undertow::Pattern slide = notes({0, 12, 0, 0});
    slide.steps[0].slide = true;
    undertow::Engine glide;
    prepared(glide, s, slide);
    const auto g = spec::render(glide, static_cast<std::size_t>(4 * kStep), {}, host());
    // Into step 1 the pitch glides from 27.5 to 55 Hz over 40 ms, and the level never jumps back up to the attack peak.
    const double before = spec::rms(g.left, static_cast<std::size_t>(0.6 * kStep), static_cast<std::size_t>(kStep));
    const double after = spec::rms(g.left, static_cast<std::size_t>(kStep), static_cast<std::size_t>(1.4 * kStep));
    CHECK_MESSAGE(after <= 1.3 * before, "a slide must not retrigger the envelope: the level went from " << before << " to " << after);
    CHECK_MESSAGE(std::fabs(subHzInStep(g.left, 1, 0.5, 0.88) - 55.0) <= 1.5,
                  "after the 40 ms slide, step 1 must sound its own note (sub 55 Hz); it sounds " << subHzInStep(g.left, 1, 0.5, 0.88)
                                                                                                 << " Hz");
    const double early = spec::zeroCrossingHz(g.left, static_cast<std::size_t>(kStep), static_cast<std::size_t>(kStep + 1440), kRate);
    CHECK_MESSAGE(early < 50.0, "in the first 30 ms of a 40 ms slide the pitch must still be on its way (sub below 50 Hz); it is "
                                    << early << " Hz");
}

TEST_CASE("P3.12 seq_sync Internal runs at seq_bpm; without a host it falls back to the internal clock; stop releases") {
    auto p = seqSub();
    spec::set(p, "seq_sync", 1);
    spec::set(p, "seq_bpm", 120);
    undertow::Engine engine;
    prepared(engine, p, notes({0, 12, 0, 12}));
    // The host says 128 BPM at beat 37; Internal ignores it and runs at 120 BPM from reset (a 1/16 is 6000 samples).
    const auto out = spec::render(engine, 24000, {}, undertow::Transport{true, true, 128.0, 37.0});
    CHECK_MESSAGE(std::fabs(spec::zeroCrossingHz(out.left, 6900, 11100, kRate) - 55.0) <= 1.5,
                  "with seq_sync Internal, step 1 (12 semitones) must play from sample 6000 (120 BPM from reset)");

    auto h = seqSub();
    spec::set(h, "seq_bpm", 120);
    undertow::Engine fallback;
    prepared(fallback, h, notes({0, 12, 0, 12}));
    const auto own = spec::render(fallback, 24000);
    CHECK_MESSAGE(std::fabs(spec::zeroCrossingHz(own.left, 6900, 11100, kRate) - 55.0) <= 1.5,
                  "with seq_sync Host but no host transport (Standalone), the sequencer runs on the internal clock at seq_bpm");

    undertow::Engine stopper;
    prepared(stopper, seqSub(), notes({0}));
    spec::render(stopper, static_cast<std::size_t>(2 * kStep), {}, host());
    const auto silence = spec::render(stopper, 9600, {}, undertow::Transport{true, false, 128.0, 0.5});
    CHECK_MESSAGE(spec::peakAbs(silence.left, 4800) <= 1e-4, "when the host stops, the sequencer's note must be released "
                                                             "(silent within 100 ms with a 5 ms release)");
    CHECK_MESSAGE(stopper.meters().step == -1, "a stopped sequencer shows step -1");
}

TEST_CASE("P3.13 patterns cross threads without locks: setPattern and pattern() while the audio runs") {
    undertow::Engine engine;
    prepared(engine, seqSub(), notes({0}));
    const auto fives = notes({5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5});
    engine.setPattern(fives);
    CHECK_MESSAGE(engine.pattern() == fives, "pattern() must return a pattern set with setPattern at once, before the next process "
                                             "call: a caller reads back what it set (until the audio thread takes it, return the "
                                             "pattern that is waiting)");
    spec::render(engine, 512, {}, host());
    CHECK_MESSAGE(engine.pattern().steps[3].semitones == 5, "a pattern set with setPattern must be in use (and visible through "
                                                            "pattern()) after the next process call");

    std::atomic<bool> done{false};
    std::atomic<int> torn{0};
    std::thread ui([&] {
        int k = 0;
        while (!done.load()) {
            const int v = (k++ % 24) - 12;
            undertow::Pattern p;
            for (auto& step : p.steps)
                step = undertow::Step{v, true, false, false, 1.0f};
            engine.setPattern(p);
            const auto seen = engine.pattern();
            for (const auto& step : seen.steps)
                if (step.semitones != seen.steps[0].semitones)
                    torn.fetch_add(1);
        }
    });
    std::vector<float> left(512), right(512);
    std::size_t allocations = 0;
    double ppq = 0;
    for (int block = 0; block < 400; ++block) {
        spec::AllocationCounter counter;
        engine.process(left.data(), right.data(), 512, nullptr, 0, host(ppq));
        allocations += counter.count();
        ppq += 512.0 / 22500.0;
    }
    done.store(true);
    ui.join();
    CHECK_MESSAGE(torn.load() == 0, torn.load() << " torn patterns: pattern() returned a mix of two patterns. Hand patterns "
                                                   "between threads with a seqlock or a double buffer");
    CHECK_MESSAGE(allocations == 0, allocations << " heap allocations in process while another thread set patterns");
    const auto last = engine.pattern();
    bool uniform = true;
    for (const auto& step : last.steps)
        uniform = uniform && step.semitones == last.steps[0].semitones;
    CHECK(uniform);
}

TEST_CASE("P3.14 a new engine starts with the default pattern") {
    undertow::Engine engine;
    engine.prepare(kRate, 512);
    CHECK_MESSAGE(engine.pattern() == undertow::defaultPattern(), "a prepared engine's pattern() must be defaultPattern()");
    const auto p = undertow::defaultPattern();
    const int semitones[] = {0, 0, 12, 0, 0, 0, 12, 10, 0, 0, 12, 0, 3, 0, 7, 12};
    for (int i = 0; i < 16; ++i) {
        const auto& step = p.steps[static_cast<std::size_t>(i)];
        INFO("default step " << i);
        CHECK(step.semitones == semitones[i]);
        CHECK(step.gate == (i != 5));
        CHECK(step.accent == (i == 2 || i == 7 || i == 10 || i == 15));
        CHECK(step.slide == (i == 6 || i == 14));
        CHECK(step.probability == (i == 11 ? 0.5f : 1.0f));
    }
}

TEST_CASE("P3.15 with the sequencer on, the full engine still renders 15 times faster than real time") {
    auto p = spec::defaults();
    spec::set(p, "seq_on", 1);
    spec::set(p, "seq_evolve", 1);
    undertow::Engine engine;
    engine.prepare(kRate, 512);
    spec::apply(engine, p);
    engine.reset();
    const double start = spec::threadSeconds();
    const auto out = spec::render(engine, 60 * 48000, {}, host(), 512);
    const double seconds = spec::threadSeconds() - start;
    CHECK_MESSAGE(spec::rms(out.left) > 0.01, "the default pattern rendered silence");
    CHECK_MESSAGE(seconds <= 4.0, "60 s with the sequencer took " << seconds << " s of CPU; it must take at most 4 s (15x real time)");
}
