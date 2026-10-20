// Phase 2.1 spec tests: a zero-length block changes nothing, and the saturation keeps phase 2's blend
// (docs/specs/phase-2.1-voice-fixes.md, P2.1.04 and P2.1.09).
// spec-needs: core/include/undertow/engine.h

#include "doctest.h"

#include "spec_engine.h"
#include "undertow/engine.h"

#include <cmath>
#include <vector>

namespace {

constexpr double kRate = 48000.0;
constexpr int kBlock = 256;

bool finite(const undertow::Meters& m) {
    return std::isfinite(m.peak) && std::isfinite(m.rms) && std::isfinite(m.noteHz) && std::isfinite(m.rumble) &&
           std::isfinite(m.ppq);
}

bool same(const undertow::Meters& a, const undertow::Meters& b) {
    return a.peak == b.peak && a.rms == b.rms && a.clipping == b.clipping && a.note == b.note && a.noteHz == b.noteHz &&
           a.step == b.step && a.rumble == b.rumble && a.ppq == b.ppq;
}

// Phase 2's saturation: a blend of the dry signal and tanh(g x) / g, with g = 1 + 3 d (docs/specs/phase-2-rumble.md).
double blend(double x, double d) {
    const double g = 1.0 + 3.0 * d;
    return x + d * (std::tanh(g * x) / g - x);
}

// The curve alone, without the dry signal: what phase 2's spec said first.
double curve(double x, double d) {
    const double g = 1.0 + 3.0 * d;
    return std::tanh(g * x) / g;
}

// The fundamental's amplitude of shape(amplitude sin t, d), over one cycle.
template <class Shape>
double fundamental(Shape&& shape, double amplitude, double d) {
    const int steps = 4096;
    double sum = 0;
    for (int i = 0; i < steps; ++i) {
        const double t = 2.0 * 3.14159265358979323846 * i / steps;
        sum += shape(amplitude * std::sin(t), d) * std::sin(t);
    }
    return 2.0 * sum / steps;
}

// The engine's output at 110 Hz for a held sub sine of amplitude 2 (sub_level 1, accented at accent 1, note 57), at
// sat_drive d and master_gain -24 dB, scaled back by the master: the saturation's fundamental. 110 Hz has exactly
// 110 cycles in a second, so a plain DFT over one second reads it without leakage.
double saturatedFundamental(float d) {
    auto patch = spec::subPatch();
    spec::set(patch, "sub_level", 1.0f);
    spec::set(patch, "accent", 1.0f);
    spec::set(patch, "sat_drive", d);
    spec::set(patch, "master_gain", -24.0f);
    undertow::Engine engine;
    engine.prepare(kRate, 512);
    spec::apply(engine, patch);
    engine.reset();
    const auto out = spec::render(engine, 72000, {spec::noteOn(0, 57, 127)});
    double re = 0, im = 0;
    for (std::size_t n = 24000; n < 72000; ++n) {
        const double t = 2.0 * 3.14159265358979323846 * 110.0 * static_cast<double>(n) / kRate;
        re += out.left[n] * std::cos(t);
        im += out.left[n] * std::sin(t);
    }
    return 2.0 * std::sqrt(re * re + im * im) / 48000.0 / std::pow(10.0, -24.0 / 20.0);
}

} // namespace

TEST_CASE("P2.1.04 a zero-length block changes nothing: the meters stay finite and as they were, and the audio goes on the same") {
    // Two engines play the default patch through the same notes, blocks and moving 128 BPM transport. One of them also
    // gets a zero-length process call before every fourth block, as some hosts send with the output bus attached.
    undertow::Engine plain, poked;
    for (auto* engine : {&plain, &poked}) {
        engine->prepare(kRate, 512);
        spec::apply(*engine, spec::defaults());
    }
    const std::vector<undertow::MidiEvent> events = {spec::noteOn(0, 45, 110), spec::noteOn(24000, 52, 90),
                                                     spec::noteOff(26000, 45), spec::noteOff(60000, 52),
                                                     spec::noteOn(72000, 40, 90), spec::noteOff(90000, 40)};
    undertow::Transport transport;
    transport.valid = true;
    transport.playing = true;
    transport.bpm = 128.0;
    const std::size_t total = 120000;
    std::vector<float> l1(kBlock), r1(kBlock), l2(kBlock), r2(kBlock), empty(1, 0.0f);
    const undertow::MidiEvent none{};
    std::vector<undertow::MidiEvent> blockEvents;
    std::size_t next = 0, zeroCalls = 0, differing = 0, changedMeters = 0, notFinite = 0;
    for (std::size_t start = 0, block = 0; start < total; start += kBlock, ++block) {
        if (block % 4 == 1) {
            const auto before = poked.meters();
            poked.process(empty.data(), empty.data(), 0, &none, 0, transport);
            ++zeroCalls;
            const auto after = poked.meters();
            notFinite += finite(after) ? 0 : 1;
            changedMeters += same(before, after) ? 0 : 1;
        }
        blockEvents.clear();
        while (next < events.size() && static_cast<std::size_t>(events[next].sampleOffset) < start + kBlock) {
            auto e = events[next++];
            e.sampleOffset -= static_cast<int>(start);
            blockEvents.push_back(e);
        }
        plain.process(l1.data(), r1.data(), kBlock, blockEvents.data(), static_cast<int>(blockEvents.size()), transport);
        poked.process(l2.data(), r2.data(), kBlock, blockEvents.data(), static_cast<int>(blockEvents.size()), transport);
        for (int n = 0; n < kBlock; ++n)
            differing += (l1[static_cast<std::size_t>(n)] != l2[static_cast<std::size_t>(n)] ||
                          r1[static_cast<std::size_t>(n)] != r2[static_cast<std::size_t>(n)])
                             ? 1
                             : 0;
        notFinite += finite(poked.meters()) ? 0 : 1;
        changedMeters += same(plain.meters(), poked.meters()) ? 0 : 1;
        transport.ppq += kBlock / kRate * transport.bpm / 60.0;
    }
    CHECK_MESSAGE(notFinite == 0, "meters() held a NaN or an infinity after " << notFinite << " of the calls (with " << zeroCalls
                                                                              << " zero-length blocks): a block of 0 samples "
                                                                                 "must not divide by its length");
    CHECK_MESSAGE(changedMeters == 0, "a zero-length block changed meters(): " << changedMeters
                                                                              << " readings differ from an engine that never "
                                                                                 "got one. Return at once when there are no "
                                                                                 "samples");
    CHECK_MESSAGE(differing == 0, differing << " output samples differ from an engine that never got a zero-length block: "
                                               "a block of 0 samples must change no state");
}

TEST_CASE("P2.1.09 the saturation keeps phase 2's blend: right at drive 0.2 and 0.5, and smooth as it leaves 0") {
    // A regression check: phase 2 built the saturation as x + d (tanh(g x) / g - x), and the default level
    // (-14 dB, P2.22) rests on it. A sine of amplitude 2 is about how hot an accented note reaches the saturation.
    const double dry = saturatedFundamental(0.0f);
    CHECK_MESSAGE(std::fabs(spec::toDb(dry / 2.0)) <= 0.1, "at sat_drive 0 the saturation must pass the signal unchanged; the "
                                                            "fundamental is " << dry << ", want 2");
    for (const float d : {0.2f, 0.5f}) {
        const double got = saturatedFundamental(d);
        const double want = fundamental(blend, 2.0, d);
        CHECK_MESSAGE(std::fabs(spec::toDb(got / want)) <= 0.3,
                      "at sat_drive " << d << " a sine of amplitude 2 comes out with a fundamental of " << got
                                      << "; the blend x + d (tanh(g x) / g - x) gives " << want << ", the curve alone "
                                      << fundamental(curve, 2.0, d));
    }
    const double barely = saturatedFundamental(0.01f);
    CHECK_MESSAGE(std::fabs(spec::toDb(barely / dry)) <= 0.1, "sat_drive 0.01 moves the level " << spec::toDb(barely / dry)
                                                              << " dB from drive 0: the saturation must leave 0 smoothly");
}
