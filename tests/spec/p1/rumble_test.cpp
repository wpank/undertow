// SPDX-License-Identifier: AGPL-3.0-only
// Phase 1 spec tests: the rumble, kept from the first version, played through the engine
// (docs/specs/phase-1-foundation.md, check P1.09).
// spec-needs: core/include/undertow/engine.h
// spec-needs: core/include/undertow/rumble.h

#include "doctest.h"

#include "spec_analysis.h"
#include "spec_engine.h"
#include "undertow/engine.h"
#include "undertow/params.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kRate = 48000.0;
constexpr double kBpm = 130.0;
constexpr double kBeat = 60.0 / kBpm * kRate; // samples

undertow::Transport host(double ppq, bool playing = true) { return undertow::Transport{true, playing, kBpm, ppq}; }

void prepared(undertow::Engine& engine, const spec::Params& p) {
    engine.prepare(kRate, 512);
    spec::apply(engine, p);
    engine.reset();
}

// The largest ratio, over beats [from, to), of the level in the 40 ms after a beat to the level in 40 ms at mid-beat.
double duckRatio(const spec::Signal& x, int from, int to) {
    double worst = 0;
    for (int beat = from; beat < to; ++beat) {
        const auto at = static_cast<std::size_t>(beat * kBeat), middle = static_cast<std::size_t>((beat + 0.45) * kBeat);
        if (middle + 1920 > x.size())
            break;
        worst = std::max(worst, spec::rms(x, at, at + 1920) / std::max(spec::rms(x, middle, middle + 1920), 1e-12));
    }
    return worst;
}

} // namespace

TEST_CASE("P1.09 the rumble pumps with the host's beat, through loops, and with the engine's own clock") {
    undertow::Engine engine;
    prepared(engine, spec::rumblePatch());
    const auto out = spec::render(engine, static_cast<std::size_t>(32 * kBeat), {}, host(0.0));
    CHECK_MESSAGE(engine.meters().rumble > 0.001f, "meters().rumble must show the rumble bus level; it is "
                                                       << engine.meters().rumble);
    CHECK_MESSAGE(spec::rms(out.left, static_cast<std::size_t>(4 * kBeat)) > 0.005,
                  "at its defaults the rumble is (nearly) silent in the engine: RMS "
                      << spec::rms(out.left, static_cast<std::size_t>(4 * kBeat))
                      << "; wire Rumble::process into the output stage");
    CHECK_MESSAGE(duckRatio(out.left, 4, 32) <= 0.7, "the rumble must be clearly quieter in the 40 ms after each beat "
                                                     "of the host transport than at mid-beat; worst ratio "
                                                         << duckRatio(out.left, 4, 32) << " (at most 0.7)");
    // Play on to mid-beat (32.5), then the host loops back to beat 0: only a rumble that follows the host's
    // position ducks right at the jump.
    spec::render(engine, static_cast<std::size_t>(0.5 * kBeat), {}, host(32.0));
    const auto looped = spec::render(engine, static_cast<std::size_t>(8 * kBeat), {}, host(0.0));
    CHECK_MESSAGE(duckRatio(looped.left, 0, 1) <= 0.7, "when the host loops back to beat 0 the ghost kick must fire "
                                                       "there: the 40 ms after the jump are "
                                                           << duckRatio(looped.left, 0, 1)
                                                           << " times as loud as mid-beat (at most 0.7)");

    undertow::Engine standalone;
    auto p = spec::rumblePatch();
    spec::set(p, undertow::param::SeqBpm, static_cast<float>(kBpm));
    prepared(standalone, p);
    const auto own = spec::render(standalone, static_cast<std::size_t>(16 * kBeat));
    CHECK_MESSAGE(spec::rms(own.left, static_cast<std::size_t>(4 * kBeat)) > 0.005,
                  "without a host the rumble must run on the engine's own clock");
    CHECK_MESSAGE(duckRatio(own.left, 4, 16) <= 0.7, "on the engine's own clock (seq_bpm 130) the rumble must pump on "
                                                     "its beats; worst ratio "
                                                         << duckRatio(own.left, 4, 16));
}

TEST_CASE("P1.09 no ghost kick fires while the host is stopped") {
    undertow::Engine engine;
    prepared(engine, spec::rumblePatch());
    const auto out = spec::render(engine, static_cast<std::size_t>(8 * kBeat), {}, host(4.0, false));
    CHECK_MESSAGE(spec::peakAbs(out.left) == 0.0, "with the host's transport stopped no ghost kick may fire; the "
                                                  "output reached "
                                                      << spec::peakAbs(out.left));
}

TEST_CASE("P1.09 the rumble lives below 200 Hz, and rumble_level 0 is silence") {
    undertow::Engine engine;
    prepared(engine, spec::rumblePatch());
    const auto out = spec::render(engine, static_cast<std::size_t>(32 * kBeat), {}, host(0.0));
    const auto s = spec::spectrum(out.left, static_cast<std::size_t>(8 * kBeat), 1u << 19, kRate);
    const double low = spec::bandPower(s, 0.0, 200.0), all = spec::bandPower(s, 0.0, 24000.0);
    CHECK_MESSAGE(low / all >= 0.9, "only " << 100.0 * low / all << "% of the rumble's energy is below 200 Hz at its "
                                                                    "defaults; want 90% or more");

    auto p = spec::rumblePatch();
    spec::set(p, undertow::param::RumbleLevel, 0);
    undertow::Engine silent;
    prepared(silent, p);
    const auto none = spec::render(silent, static_cast<std::size_t>(8 * kBeat), {}, host(0.0));
    CHECK_MESSAGE(spec::peakAbs(none.left) == 0.0, "rumble_level 0, with the voice's sources off, must output exact "
                                                   "silence; got "
                                                       << spec::peakAbs(none.left));
}
