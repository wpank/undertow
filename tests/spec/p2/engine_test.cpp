// Phase 2 spec tests: modulation, rumble and the output stage inside the engine
// (docs/specs/phase-2-rumble.md, P2.13 to P2.19).
// spec-needs: core/include/undertow/lfo.h
// spec-needs: core/include/undertow/rumble.h
// spec-needs: core/include/undertow/output_stage.h

#include "doctest.h"

#include "spec_alloc.h"
#include "spec_analysis.h"
#include "spec_engine.h"
#include "undertow/engine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr double kRate = 48000.0;

undertow::Engine& prepared(undertow::Engine& engine, const spec::Params& patch) {
    engine.prepare(kRate, 512);
    spec::apply(engine, patch);
    engine.reset();
    return engine;
}

undertow::Transport host(double bpm, double ppq = 0.0) { return undertow::Transport{true, true, bpm, ppq}; }

// Energy above 1.5 kHz in 20 ms windows: how bright the voice is over time.
std::vector<double> brightness(const spec::Signal& x) {
    return spec::rmsEnvelope(spec::highpass(x, 1500.0, kRate, 4), 960, 960);
}

std::size_t loudest(const std::vector<double>& v, std::size_t first, std::size_t last) {
    std::size_t best = first;
    for (std::size_t i = first; i < last && i < v.size(); ++i)
        if (v[i] > v[best])
            best = i;
    return best;
}

spec::Params rumbleOnly() {
    auto p = spec::cleanPatch();
    spec::set(p, "rumble_level", 1);
    spec::set(p, "rumble_kick", 1);
    spec::set(p, "rumble_send", 0);
    spec::set(p, "rumble_rate", 0);
    spec::set(p, "rumble_decay", 2.5f);
    spec::set(p, "rumble_tone", 110);
    spec::set(p, "rumble_drive", 0.4f);
    spec::set(p, "rumble_duck", 0.8f);
    return p;
}

// The worst ratio, over beats 4 .. beats-1, of the rumble's RMS in the 40 ms after a beat to its RMS in 40 ms at
// mid-beat: the duck makes the first clearly smaller than the second.
double duckRatio(const spec::Signal& x, double samplesPerBeat, int firstBeat, int beats) {
    double worst = 0;
    for (int beat = firstBeat; beat < beats; ++beat) {
        const auto at = static_cast<std::size_t>(beat * samplesPerBeat), middle = static_cast<std::size_t>((beat + 0.45) * samplesPerBeat);
        worst = std::max(worst, spec::rms(x, at, at + 1920) / std::max(spec::rms(x, middle, middle + 1920), 1e-12));
    }
    return worst;
}

} // namespace

TEST_CASE("P2.13 LFO 1 moves the cutoff in time with the host's bar, and with the internal clock without a host") {
    auto p = spec::cleanPatch();
    spec::set(p, "filter_cutoff", 300);
    spec::set(p, "lfo1_cutoff", 3);
    spec::set(p, "lfo1_rate", 2); // 1 bar
    spec::set(p, "lfo1_shape", 0);
    spec::set(p, "lfo1_phase", 0);
    undertow::Engine engine;
    prepared(engine, p);
    // 120 BPM: a bar is 2 s. A 1-bar sine peaks at ppq 1 (0.5 s) and bottoms out at ppq 3 (1.5 s).
    const auto first = spec::render(engine, 192000, {spec::noteOn(0, 45)}, host(120.0, 0.0));
    const auto b = brightness(first.left);
    const double peakAt = 0.02 * static_cast<double>(loudest(b, 0, 100));
    CHECK_MESSAGE(std::fabs(peakAt - 0.5) <= 0.1, "the voice is brightest " << peakAt << " s into the bar; a 1-bar sine LFO on "
                                                                                       "the cutoff peaks at ppq 1 (0.5 s at "
                                                                                       "120 BPM): the LFO must follow the "
                                                                                       "host's beat position");
    const double swing = spec::toDb(b[25] / std::max(b[75], 1e-12));
    CHECK_MESSAGE(swing >= 20.0, "lfo1_cutoff 3 octaves must swing the brightness by 20 dB or more between ppq 1 and 3; it "
                                 "swings "
                                     << swing << " dB");
    // The host loops back to the start of the bar: the same motion again.
    const auto again = spec::render(engine, 96000, {}, host(120.0, 0.0));
    const auto b2 = brightness(again.left);
    std::vector<double> one(b.begin(), b.begin() + 100), two(b2.begin(), b2.begin() + 100);
    spec::Signal x(one.begin(), one.end()), y(two.begin(), two.end());
    CHECK_MESSAGE(spec::correlation(x, y) >= 0.98, "after the host loops back to ppq 0 the cutoff must move exactly as "
                                                   "before (correlation "
                                                       << spec::correlation(x, y) << ")");

    undertow::Engine standalone;
    spec::set(p, "seq_bpm", 120);
    prepared(standalone, p);
    const auto own = spec::render(standalone, 96000, {spec::noteOn(0, 45)});
    const double ownPeak = 0.02 * static_cast<double>(loudest(brightness(own.left), 0, 100));
    CHECK_MESSAGE(std::fabs(ownPeak - 0.5) <= 0.1, "without a host transport the LFO runs on the internal clock (seq_bpm 120 "
                                                   "from reset): the peak must come at 0.5 s; it came at "
                                                       << ownPeak << " s");
}

TEST_CASE("P2.14 LFO 2 reaches its destinations: pitch, amp, pulse width and cutoff") {
    auto lfo2 = [](spec::Params p, int dest, float depth) {
        spec::set(p, "lfo2_dest", static_cast<float>(dest));
        spec::set(p, "lfo2_depth", depth);
        spec::set(p, "lfo2_shape", 4); // Square: +1 for the first half of the cycle, -1 for the second
        spec::set(p, "lfo2_rate", 3);  // 1/2: 2 beats, so +1 on beat 1 and -1 on beat 2
        spec::set(p, "lfo2_phase", 0);
        undertow::Engine engine;
        prepared(engine, p);
        return spec::render(engine, 48000, {spec::noteOn(0, 45)}, host(120.0, 0.0)).left; // beat = 0.5 s
    };
    const auto pitch = lfo2(spec::subPatch(), 4, 1.0f);
    const double up = spec::zeroCrossingHz(pitch, 4800, 21600, kRate), down = spec::zeroCrossingHz(pitch, 28800, 45600, kRate);
    CHECK_MESSAGE(std::fabs(up / std::max(down, 1e-9) - 4.0) <= 0.08,
                  "LFO 2 on Pitch at depth 1 moves +-12 semitones: a square LFO must put the sub at 110 Hz then 27.5 Hz (ratio "
                  "4); measured "
                      << up << " and " << down << " Hz");

    const auto amp = lfo2(spec::subPatch(), 5, 1.0f);
    const double open = spec::rms(amp, 4800, 21600), closed = spec::rms(amp, 28800, 45600);
    CHECK_MESSAGE(spec::toDb(closed / std::max(open, 1e-12)) <= -40.0,
                  "LFO 2 on Amp at depth 1 must take the level from full to silence; the low half is only "
                      << spec::toDb(closed / std::max(open, 1e-12)) << " dB down");

    auto square = spec::cleanPatch();
    spec::set(square, "osc_wave", 1);
    spec::set(square, "osc_pw", 0.5f);
    const auto width = lfo2(square, 2, 1.0f);
    CHECK_MESSAGE(spec::mean(width, 4800, 21600) >= 0.3, "LFO 2 on Pulse Width at depth 1 must widen the pulse to 0.95 (mean "
                                                         "+0.45 at level 0.5); the mean is "
                                                             << spec::mean(width, 4800, 21600));
    CHECK_MESSAGE(spec::mean(width, 28800, 45600) <= -0.3, "and narrow it to 0.05 on the other half; the mean is "
                                                               << spec::mean(width, 28800, 45600));

    auto dark = spec::cleanPatch();
    spec::set(dark, "filter_cutoff", 400);
    const auto cutoff = lfo2(dark, 0, 0.5f);
    const auto b = brightness(cutoff);
    CHECK_MESSAGE(spec::toDb(b[15] / std::max(b[40], 1e-12)) >= 12.0,
                  "LFO 2 on Cutoff at depth 0.5 (+-2 octaves) must swing the brightness by 12 dB or more; it swings "
                      << spec::toDb(b[15] / std::max(b[40], 1e-12)) << " dB");
}

TEST_CASE("P2.15 in the engine the rumble pumps with the host's beat, through loops, and with the internal clock") {
    const double beat = 60.0 / 128.0 * kRate;
    undertow::Engine engine;
    prepared(engine, rumbleOnly());
    const auto out = spec::render(engine, static_cast<std::size_t>(32 * beat), {}, host(128.0, 0.0));
    CHECK_MESSAGE(engine.meters().rumble > 0.001f, "meters().rumble must show the rumble bus level; it is " << engine.meters().rumble);
    CHECK_MESSAGE(out.left == out.right, "the rumble is mono: at width 0 both channels must be the same");
    CHECK_MESSAGE(spec::rms(out.left, static_cast<std::size_t>(4 * beat)) > 0.005, "the rumble is (nearly) silent in the engine");
    CHECK_MESSAGE(duckRatio(out.left, beat, 4, 32) <= 0.7, "the engine's rumble must be clearly quieter in the 40 ms after each "
                                                           "beat of the host transport than at mid-beat; worst ratio "
                                                               << duckRatio(out.left, beat, 4, 32) << " (at most 0.7)");
    // Play on to mid-beat (32.5), then the host loops back to beat 0: only a rumble that follows the host's
    // position ducks right at the jump.
    spec::render(engine, static_cast<std::size_t>(0.5 * beat), {}, host(128.0, 32.0));
    const auto looped = spec::render(engine, static_cast<std::size_t>(8 * beat), {}, host(128.0, 0.0));
    CHECK_MESSAGE(duckRatio(looped.left, beat, 0, 1) <= 0.7, "when the host loops back to beat 0 the ghost kick must fire there: "
                                                             "the 40 ms after the jump are "
                                                                 << duckRatio(looped.left, beat, 0, 1)
                                                                 << " times as loud as mid-beat (at most 0.7)");

    undertow::Engine standalone;
    auto p = rumbleOnly();
    spec::set(p, "seq_bpm", 128);
    prepared(standalone, p);
    const auto own = spec::render(standalone, static_cast<std::size_t>(16 * beat));
    CHECK_MESSAGE(spec::rms(own.left, static_cast<std::size_t>(4 * beat)) > 0.005, "without a host the rumble must run on the "
                                                                                   "internal clock");
    CHECK_MESSAGE(duckRatio(own.left, beat, 4, 16) <= 0.7, "on the internal clock (seq_bpm 128) the rumble must pump on its beats; "
                                                          "worst ratio "
                                                              << duckRatio(own.left, beat, 4, 16));
}

TEST_CASE("P2.16 in the full mix the low end is mono at any width, and width spreads what is above it") {
    auto p = spec::defaults();
    spec::set(p, "seq_on", 0);
    spec::set(p, "width", 1);
    spec::set(p, "mono_freq", 120);
    undertow::Engine engine;
    prepared(engine, p);
    const auto out = spec::render(engine, 192000, {spec::noteOn(0, 33, 100)}, host(128.0, 0.0));
    spec::Signal mid(out.left.size()), side(out.left.size());
    for (std::size_t n = 0; n < mid.size(); ++n) {
        mid[n] = 0.5f * (out.left[n] + out.right[n]);
        side[n] = 0.5f * (out.left[n] - out.right[n]);
    }
    // Below half the crossover (60 Hz: the sub and the A1 fundamental) the mix must be mono.
    const double lowSide = spec::rms(spec::lowpass(side, 60.0, kRate, 4), 24000);
    const double lowMid = spec::rms(spec::lowpass(mid, 60.0, kRate, 4), 24000);
    CHECK_MESSAGE(spec::toDb(lowSide / lowMid) <= -25.0, "at width 1 the side signal below 60 Hz (mono_freq / 2) is only "
                                                             << spec::toDb(lowSide / lowMid)
                                                             << " dB under the mid: the band under the crossover must stay mono");
    const double highSide = spec::rms(spec::highpass(side, 500.0, kRate, 4), 24000);
    const double highMid = spec::rms(spec::highpass(mid, 500.0, kRate, 4), 24000);
    CHECK_MESSAGE(spec::toDb(highSide / highMid) >= -12.0, "at width 1 the side signal above 500 Hz is "
                                                               << spec::toDb(highSide / highMid)
                                                               << " dB under the mid: width must spread the upper band");
}

TEST_CASE("P2.17 whatever is played, the output never passes -0.3 dBFS, and the clip meter says when it is pushed") {
    auto hot = spec::defaults();
    spec::set(hot, "seq_on", 0);
    spec::set(hot, "osc_level", 1);
    spec::set(hot, "sub_level", 1);
    spec::set(hot, "accent", 1);
    spec::set(hot, "filter_res", 1);
    spec::set(hot, "filter_drive", 1);
    spec::set(hot, "sat_drive", 0); // saturation rounds peaks down; with it off the stack goes well over full scale
    spec::set(hot, "master_gain", 6);
    spec::set(hot, "rumble_level", 1);
    spec::set(hot, "rumble_kick", 1);
    spec::set(hot, "rumble_send", 1);
    spec::set(hot, "width", 1);
    undertow::Engine engine;
    prepared(engine, hot);
    std::vector<undertow::MidiEvent> events;
    for (int i = 0; i < 80; ++i)
        events.push_back(spec::noteOn(i * 6000, 24 + (i * 7) % 48, 127));
    const auto out = spec::render(engine, 480000, events, host(128.0, 0.0));
    const double peak = std::max(spec::peakAbs(out.left), spec::peakAbs(out.right));
    CHECK_MESSAGE(peak <= 0.966 + 1e-6, "the output reached " << peak << "; the soft clipper must hold every sample within "
                                                                         "0.966 (-0.3 dBFS)");
    CHECK_MESSAGE(spec::allFinite(out.left), "the hot patch produced non-finite samples");
    CHECK_MESSAGE(engine.meters().clipping, "meters().clipping must be true while the output stage is pushed over full scale");

    spec::apply(engine, spec::cleanPatch());
    spec::render(engine, 48000, {{0, 0xB0, 120, 0}}, host(128.0, 0.0));
    CHECK_MESSAGE(!engine.meters().clipping, "meters().clipping must clear 0.5 s after the output stops going over full scale");
}

TEST_CASE("P2.18 with LFOs, rumble, saturation and width on, the engine still never allocates") {
    auto p = spec::defaults();
    spec::set(p, "seq_on", 0);
    spec::set(p, "lfo2_depth", 0.5f);
    spec::set(p, "lfo2_shape", 5);
    undertow::Engine engine;
    prepared(engine, p);
    std::vector<float> left(512), right(512);
    const std::vector<undertow::MidiEvent> on = {spec::noteOn(0, 33, 110)}, off = {spec::noteOff(300, 33)};
    std::size_t allocations = 0;
    double ppq = 0;
    for (int block = 0; block < 1000; ++block) {
        const auto& events = block % 8 == 0 ? on : (block % 8 == 5 ? off : std::vector<undertow::MidiEvent>{});
        spec::AllocationCounter counter;
        engine.setParam(spec::id("lfo2_dest"), static_cast<float>(block % 6));
        engine.setParam(spec::id("rumble_decay"), 0.5f + static_cast<float>(block % 10) * 0.7f);
        engine.setParam(spec::id("mono_freq"), 60.0f + static_cast<float>(block % 19) * 10.0f);
        engine.process(left.data(), right.data(), 512, events.data(), static_cast<int>(events.size()), host(128.0, ppq));
        (void)engine.meters();
        allocations += counter.count();
        ppq += 512.0 / kRate * 128.0 / 60.0;
    }
    CHECK_MESSAGE(allocations == 0, allocations << " heap allocations with phase 2's stages running (changing rumble_decay and "
                                                   "mono_freq must not reallocate)");
}

TEST_CASE("P2.19 the full engine renders at least 15 times faster than real time") {
    auto p = spec::defaults();
    spec::set(p, "seq_on", 0);
    spec::set(p, "lfo2_depth", 0.5f);
    spec::set(p, "width", 0.7f);
    undertow::Engine engine;
    prepared(engine, p);
    std::vector<undertow::MidiEvent> events;
    const int notes[] = {33, 33, 45, 33, 36, 33, 45, 40};
    for (int i = 0; i * 5625 < 60 * 48000; ++i) {
        events.push_back(spec::noteOn(i * 5625, notes[i % 8], i % 4 == 2 ? 120 : 90));
        events.push_back(spec::noteOff(i * 5625 + 2800, notes[i % 8]));
    }
    std::sort(events.begin(), events.end(), [](const auto& a, const auto& b) { return a.sampleOffset < b.sampleOffset; });
    const double start = spec::threadSeconds();
    const auto out = spec::render(engine, 60 * 48000, events, host(128.0, 0.0), 512);
    const double seconds = spec::threadSeconds() - start;
    CHECK_MESSAGE(spec::rms(out.left) > 0.01, "the full engine rendered silence");
    CHECK_MESSAGE(seconds <= 4.0, "60 s of the full engine took " << seconds << " s of CPU; it must take at most 4 s (15x real time)");
}
