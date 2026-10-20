// Phase 2.1 spec tests: a zero-length block changes nothing (docs/specs/phase-2.1-voice-fixes.md, P2.1.04).
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
