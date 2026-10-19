// Phase 1 spec tests: the voice inside the engine: MIDI, parameters, realtime safety and speed
// (docs/specs/phase-1-voice.md, P1.16 to P1.20).
// spec-needs: core/include/undertow/voice.h

#include "doctest.h"

#include "spec_alloc.h"
#include "spec_analysis.h"
#include "spec_engine.h"
#include "undertow/engine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr double kRate = 48000.0;

bool near(double value, double want, double tolerance) { return std::fabs(value - want) <= tolerance; }

undertow::Engine& prepared(undertow::Engine& engine, const spec::Params& patch) {
    engine.prepare(kRate, 512);
    spec::apply(engine, patch);
    engine.reset();
    return engine;
}

// A deterministic pseudo-random source for the fuzz test.
struct Random {
    std::uint32_t state;
    std::uint32_t next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    double unit() { return (next() & 0xFFFFFF) / static_cast<double>(0x1000000); }
};

} // namespace

TEST_CASE("P1.16 the engine plays MIDI: note on and off on any channel, velocity 0 as off, bend, and controllers 120 and 123") {
    undertow::Engine engine;
    prepared(engine, spec::subPatch());
    auto out = spec::render(engine, 24000, {{0, 0x93, 45, 100}}); // channel 4
    CHECK_MESSAGE(engine.meters().note == 45, "meters().note is " << engine.meters().note << " while note 45 plays");
    CHECK_MESSAGE(near(engine.meters().noteHz, 110.0, 0.05), "meters().noteHz is " << engine.meters().noteHz << " for note 45");
    CHECK_MESSAGE(near(spec::zeroCrossingHz(out.left, 4800, 24000, kRate), 55.0, 0.2),
                  "a note-on on channel 4 must play the voice (the sub at 55 Hz)");
    CHECK_MESSAGE(out.left == out.right, "with width 0 both channels carry the same signal, sample for sample");

    out = spec::render(engine, 24000, {spec::bend(0, 1.0)});
    CHECK_MESSAGE(near(engine.meters().noteHz, 110.0 * std::exp2(2.0 / 12.0), 0.1),
                  "a full-up pitch-bend message (0xE0, 16383) must bend note 45 up bend_range (2) semitones; meters().noteHz is "
                      << engine.meters().noteHz);
    spec::render(engine, 480, {spec::bend(0, 0.0)});

    spec::render(engine, 24000, {{0, 0x93, 45, 0}}); // velocity 0 = note off
    CHECK_MESSAGE(engine.meters().note == -1, "a note-on with velocity 0 is a note-off; the voice is still playing note "
                                                  << engine.meters().note);

    spec::render(engine, 4800, {spec::noteOn(0, 50)});
    spec::render(engine, 24000, {spec::controller(0, 123, 0)});
    CHECK_MESSAGE(engine.meters().note == -1, "controller 123 (all notes off) must release the voice");

    spec::render(engine, 4800, {spec::noteOn(0, 50)});
    out = spec::render(engine, 480, {spec::controller(0, 120, 0)});
    CHECK_MESSAGE(spec::peakAbs(out.left, 1) == 0.0, "controller 120 (all sound off) must silence the voice at once");
}

TEST_CASE("P1.17 the engine's parameters reach the voice") {
    auto brightness = [](float cutoff) {
        undertow::Engine engine;
        auto p = spec::cleanPatch();
        spec::set(p, "filter_cutoff", cutoff);
        prepared(engine, p);
        const auto out = spec::render(engine, 1u << 15, {spec::noteOn(0, 45)});
        const auto s = spec::spectrum(out.left, 1u << 14, 1u << 14, kRate);
        return spec::bandPower(s, 1000.0, 20000.0);
    };
    const double ratio = 10.0 * std::log10(brightness(8000.0) / std::max(brightness(200.0), 1e-30));
    CHECK_MESSAGE(ratio >= 20.0, "filter_cutoff 8 kHz must leave much more energy above 1 kHz than 200 Hz; the difference is "
                                     << ratio << " dB: setParam must reach the voice's filter");

    undertow::Engine engine;
    auto p = spec::subPatch();
    spec::set(p, "sub_octave", 1);
    prepared(engine, p);
    auto out = spec::render(engine, 72000, {spec::noteOn(0, 45)});
    CHECK_MESSAGE(near(spec::zeroCrossingHz(out.left, 24000, 72000, kRate), 27.5, 0.1), "sub_octave set through the engine must "
                                                                                       "reach the voice (27.5 Hz for note 45)");
    const double full = spec::rms(out.left, 48000, 72000);
    engine.setParam(spec::id("master_gain"), -6.0206f);
    out = spec::render(engine, 24000);
    CHECK_MESSAGE(near(spec::rms(out.left, 9600, 24000) / full, 0.5, 0.01),
                  "master_gain -6 dB must halve the output; the ratio is " << spec::rms(out.left, 9600, 24000) / full);
}

TEST_CASE("P1.18 the engine makes no heap allocation while it plays") {
    undertow::Engine engine;
    auto p = spec::defaults();
    spec::set(p, "seq_on", 0);
    prepared(engine, p);
    std::vector<float> left(512), right(512);
    std::vector<std::vector<undertow::MidiEvent>> script = {
        {spec::noteOn(0, 33, 100)},
        {spec::noteOn(100, 40, 120), spec::bend(300, 0.5)},
        {spec::noteOff(50, 40), spec::noteOff(60, 33)},
        {spec::noteOn(0, 45, 64), spec::controller(400, 123, 0)},
        {},
    };
    std::size_t allocations = 0;
    for (int block = 0; block < 1000; ++block) { // 10.7 s
        const auto& events = script[static_cast<std::size_t>(block % 5)];
        spec::AllocationCounter counter;
        engine.setParam(spec::id("filter_cutoff"), 100.0f + 50.0f * static_cast<float>(block % 40));
        engine.setParam(spec::id("glide_ms"), static_cast<float>(block % 7) * 20.0f);
        engine.setParam(spec::id("osc_wave"), static_cast<float>(block % 2));
        engine.process(left.data(), right.data(), 512, events.data(), static_cast<int>(events.size()), {});
        (void)engine.meters();
        allocations += counter.count();
    }
    CHECK_MESSAGE(allocations == 0, allocations << " heap allocations while playing notes: the voice must allocate nothing "
                                                   "after prepare()");
}

TEST_CASE("P1.19 under random notes and parameters the output stays finite and bounded, and silence ends in zeros") {
    undertow::Engine engine;
    prepared(engine, spec::cleanPatch());
    Random random{12345};
    std::vector<float> left(256), right(256);
    double peak = 0;
    int nonFinite = 0;
    for (int block = 0; block < 3750; ++block) { // 20 s
        for (int i = 0; i < undertow::kNumParams; ++i) {
            if (i == spec::id("seq_on") || random.unit() > 0.05)
                continue;
            const auto& s = undertow::paramSpec(i);
            engine.setParam(i, static_cast<float>(s.min + (s.max - s.min) * random.unit()));
        }
        std::vector<undertow::MidiEvent> events;
        if (random.unit() < 0.3)
            events.push_back(spec::noteOn(static_cast<int>(random.unit() * 100), static_cast<int>(random.unit() * 128),
                                          1 + static_cast<int>(random.unit() * 126)));
        if (random.unit() < 0.2)
            events.push_back(spec::noteOff(150, static_cast<int>(random.unit() * 128)));
        if (random.unit() < 0.1)
            events.push_back(spec::bend(200, random.unit() * 2.0 - 1.0));
        engine.process(left.data(), right.data(), 256, events.data(), static_cast<int>(events.size()), {});
        for (int n = 0; n < 256; ++n) {
            nonFinite += (std::isfinite(left[static_cast<std::size_t>(n)]) ? 0 : 1) +
                         (std::isfinite(right[static_cast<std::size_t>(n)]) ? 0 : 1);
            peak = std::max(peak, static_cast<double>(std::fabs(left[static_cast<std::size_t>(n)])));
        }
    }
    CHECK_MESSAGE(nonFinite == 0, nonFinite << " non-finite samples under random notes and parameters");
    CHECK_MESSAGE(peak < 100.0, "the output reached " << peak << " under random notes and parameters: something is unstable");

    // Release every note the fuzz left held (controller 123), play one resonant note with a long release, then
    // 30 s of silence: no denormals on the way down, and exact zeros at the end.
    spec::apply(engine, spec::cleanPatch());
    engine.setParam(spec::id("filter_res"), 1.0f);
    engine.setParam(spec::id("amp_release"), 4000.0f);
    spec::render(engine, 48000, {spec::controller(0, 123, 0), spec::noteOn(10, 40, 127), spec::noteOff(24000, 40)});
    const auto tail = spec::render(engine, 30 * 48000);
    CHECK_MESSAGE(spec::countSubnormal(tail.left) == 0,
                  spec::countSubnormal(tail.left) << " denormal output samples in 30 s after the last note: let envelopes end at "
                                                     "exactly 0 and flush filter state");
    CHECK_MESSAGE(spec::peakAbs(tail.left, tail.left.size() - 48000) == 0.0,
                  "29 s after the last note the output is still " << spec::peakAbs(tail.left, tail.left.size() - 48000)
                                                                   << ": an idle voice must output exact zeros");
}

TEST_CASE("P1.20 the engine renders a busy bassline at least 24 times faster than real time") {
    undertow::Engine engine;
    auto p = spec::defaults();
    spec::set(p, "filter_res", 0.8f);
    spec::set(p, "filter_drive", 0.5f);
    spec::set(p, "glide_ms", 60);
    spec::set(p, "seq_on", 0);
    prepared(engine, p);
    // 60 s of 1/16 notes at 128 BPM (5625 samples each), overlapping every other note for legato glides.
    std::vector<undertow::MidiEvent> events;
    const int notes[] = {33, 33, 45, 33, 36, 33, 45, 40};
    for (int i = 0; i * 5625 < 60 * 48000; ++i) {
        const int note = notes[i % 8];
        events.push_back(spec::noteOn(i * 5625, note, i % 4 == 2 ? 120 : 90));
        events.push_back(spec::noteOff(i * 5625 + (i % 2 == 0 ? 6000 : 2800), note));
    }
    std::sort(events.begin(), events.end(), [](const auto& a, const auto& b) { return a.sampleOffset < b.sampleOffset; });
    const auto start = std::chrono::steady_clock::now();
    const auto out = spec::render(engine, 60 * 48000, events, {}, 512);
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    CHECK_MESSAGE(spec::rms(out.left) > 0.01, "the busy bassline rendered silence");
    CHECK_MESSAGE(seconds <= 2.5, "60 s of audio took " << seconds
                                                        << " s to render; it must take at most 2.5 s (24x real time). Avoid "
                                                           "per-sample allocation, locks, and recomputing what only changes "
                                                           "per block");
}
