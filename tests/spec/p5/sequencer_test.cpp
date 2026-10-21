// Phase 5 spec tests: a probability-1 step always plays (docs/specs/phase-5-ship.md, P5.13).
// spec-needs: core/include/undertow/presets.h
// (The sequencer exists from phase 3; the line above only keeps this test out of tools/check.sh until phase 5 runs.)

#include "doctest.h"

#include "spec_engine.h"
#include "undertow/sequencer.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr double kRate = 48000.0;
constexpr double kBpm = 128.0;
constexpr double kStep = 5625.0; // a 1/16 at 128 BPM, in samples

// The step numbers (k) that play in the four beats from `startBeat`, at 1/16 with every gate on at probability 1.
std::vector<long long> stepsPlayed(int seed, double startBeat) {
    auto p = spec::defaults();
    spec::set(p, "seq_on", 1);
    spec::set(p, "seq_rate", 1); // 1/16
    spec::set(p, "seq_length", 16);
    spec::set(p, "seq_gate", 0.5f);
    spec::set(p, "seq_swing", 0);
    spec::set(p, "seq_seed", static_cast<float>(seed));
    spec::set(p, "seq_evolve", 0);
    undertow::Pattern every;
    for (auto& step : every.steps)
        step = undertow::Step{0, true, false, false, 1.0f};
    undertow::Sequencer seq;
    seq.prepare(kRate);
    seq.setParams(p.data());
    seq.setPattern(every);
    seq.reset();
    std::vector<long long> played;
    undertow::SeqEvent out[64];
    const long long total = static_cast<long long>(16 * kStep);
    for (long long start = 0; start < total; start += 512) {
        const int n = static_cast<int>(std::min<long long>(512, total - start));
        const double ppq = startBeat + static_cast<double>(start) / (60.0 / kBpm * kRate);
        const int count = seq.process(ppq, kBpm, true, n, out, 64);
        for (int i = 0; i < count; ++i)
            if (out[i].type == undertow::SeqEvent::Type::NoteOn)
                played.push_back(std::llround(startBeat * 4.0 + static_cast<double>(start + out[i].sampleOffset) / kStep));
    }
    return played;
}

struct Drop {
    int seed;
    double startBeat;
    long long step;
};

} // namespace

TEST_CASE("P5.13 a probability-1 step always plays: seed 1089 plays step 3692, and seed 2443 step 3787") {
    // Phase 3's rule: a step plays when random(seed, k) < probability, with random in [0, 1). Phase 3's sequencer
    // divided its 32-bit hash in float, which rounds the top 128 hash values up to exactly 1.0, so a step at
    // probability 1 could drop: seed 1089 lost step 3692 (beat 923, about 7 minutes in at 128 BPM), seed 2443 step 3787.
    const Drop drops[] = {{1089, 920.0, 3692}, {2443, 944.0, 3787}};
    for (const Drop& drop : drops) {
        INFO("seed " << drop.seed << ", the four beats from beat " << drop.startBeat);
        const auto played = stepsPlayed(drop.seed, drop.startBeat);
        CHECK_MESSAGE(played.size() == 16u, "every gate on at probability 1: all 16 steps of four beats must play; "
                                                << played.size() << " did");
        const bool found = std::find(played.begin(), played.end(), drop.step) != played.end();
        CHECK_MESSAGE(found, "step " << drop.step << " (beat " << static_cast<double>(drop.step) / 4.0
                                     << ") must play: probability 1 always plays, so the step's random draw must stay "
                                        "below 1 (a float division of a 32-bit hash rounds its top values up to 1.0)");
    }
}
