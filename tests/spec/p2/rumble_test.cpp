// Phase 2 spec tests: the rumble engine (docs/specs/phase-2-rumble.md, P2.04 to P2.08).
// spec-needs: core/include/undertow/rumble.h

#include "doctest.h"

#include "spec_alloc.h"
#include "spec_analysis.h"
#include "spec_engine.h"
#include "undertow/rumble.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace {

constexpr double kRate = 48000.0;
constexpr double kBpm = 128.0;
constexpr double kBeat = 60.0 / kBpm * kRate; // 22500 samples

spec::Params rumblePatch() {
    auto p = spec::defaults();
    spec::set(p, "rumble_level", 1);
    spec::set(p, "rumble_send", 0);
    spec::set(p, "rumble_kick", 1);
    spec::set(p, "rumble_rate", 0);
    spec::set(p, "rumble_decay", 2.5f);
    spec::set(p, "rumble_tone", 110);
    spec::set(p, "rumble_drive", 0.4f);
    spec::set(p, "rumble_duck", 0.8f);
    return p;
}

struct Run {
    spec::Signal out;
    std::vector<double> duck;
};

// Run the rumble for `samples` samples of a transport at 128 BPM starting at `ppq`; `running` false holds the clock.
Run run(undertow::Rumble& rumble, std::size_t samples, double ppq, bool running, const spec::Signal* send = nullptr) {
    Run r;
    r.out.resize(samples);
    r.duck.resize(samples);
    for (std::size_t n = 0; n < samples; ++n) {
        const double position = running ? ppq + static_cast<double>(n) / kBeat : ppq;
        r.out[n] = rumble.process(send != nullptr && n < send->size() ? (*send)[n] : 0.0f, position, kBpm, running);
        r.duck[n] = rumble.duckGain();
    }
    return r;
}

undertow::Rumble make(const spec::Params& p) {
    undertow::Rumble rumble;
    rumble.prepare(kRate);
    rumble.setParams(p.data());
    rumble.reset();
    return rumble;
}

} // namespace

TEST_CASE("P2.04 the ghost kick and the duck follow the beat grid: the rumble is quietest on the beat") {
    auto rumble = make(rumblePatch());
    const auto r = run(rumble, static_cast<std::size_t>(32 * kBeat), 0.0, true); // 8 bars
    for (int beat = 1; beat < 32; ++beat) {
        const auto at = static_cast<std::size_t>(beat * kBeat);
        INFO("beat " << beat);
        CHECK_MESSAGE(r.duck[at + 240] <= 0.3, "5 ms after a kick the duck gain is " << r.duck[at + 240]
                                                                                    << "; with rumble_duck 0.8 it must be down "
                                                                                       "near 0.2 (it falls within 2 ms)");
        CHECK_MESSAGE(r.duck[at + static_cast<std::size_t>(0.75 * kBeat)] >= 0.93,
                      "three quarters of a beat after a kick the duck gain is "
                          << r.duck[at + static_cast<std::size_t>(0.75 * kBeat)] << "; it must have come back above 0.93");
        CHECK_MESSAGE(r.duck[at - 10] >= 0.96, "just before a kick the duck must be open; it is " << r.duck[at - 10]);
    }
    // Heard as level: the 40 ms after each beat (from bar 2 on) is clearly quieter than 40 ms at mid-beat.
    double worst = 0;
    int worstBeat = 0;
    for (int beat = 4; beat < 32; ++beat) {
        const auto at = static_cast<std::size_t>(beat * kBeat), middle = static_cast<std::size_t>((beat + 0.45) * kBeat);
        const double ratio = spec::rms(r.out, at, at + 1920) / std::max(spec::rms(r.out, middle, middle + 1920), 1e-12);
        if (ratio > worst) {
            worst = ratio;
            worstBeat = beat;
        }
    }
    CHECK_MESSAGE(worst <= 0.7, "on beat " << worstBeat << " the 40 ms after the kick are " << worst
                                           << " times as loud as 40 ms at mid-beat; the duck must make the rumble clearly "
                                              "quieter on the beat (at most 0.7)");
    CHECK_MESSAGE(spec::rms(r.out, static_cast<std::size_t>(4 * kBeat)) > 0.01, "the rumble is (nearly) silent");

    auto stopped = make(rumblePatch());
    const auto s = run(stopped, static_cast<std::size_t>(4 * kBeat), 0.0, false);
    CHECK_MESSAGE(spec::peakAbs(s.out) == 0.0, "with the transport stopped no ghost kick may fire; the rumble output "
                                               << spec::peakAbs(s.out));
}

TEST_CASE("P2.05 the rumble lives below 200 Hz, from the ghost kick and from a bright send alike") {
    // A bright send (the voice's saw, here white noise) fills the reverb with highs that only the rumble's low-passes
    // take out; the ghost kick is low already.
    spec::Signal bright(static_cast<std::size_t>(32 * kBeat));
    std::uint32_t state = 5;
    for (auto& v : bright) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        v = static_cast<float>(0.25 * (static_cast<double>(state) / 4294967295.0 * 2.0 - 1.0));
    }
    for (const bool kick : {true, false}) {
        for (const float drive : {0.4f, 1.0f}) {
            auto p = rumblePatch();
            spec::set(p, "rumble_drive", drive);
            spec::set(p, "rumble_kick", kick ? 1.0f : 0.0f);
            spec::set(p, "rumble_send", kick ? 0.0f : 1.0f);
            auto rumble = make(p);
            const auto r = run(rumble, static_cast<std::size_t>(32 * kBeat), 0.0, true, kick ? nullptr : &bright);
            const auto s = spec::spectrum(r.out, static_cast<std::size_t>(8 * kBeat), 1u << 19, kRate);
            const double low = spec::bandPower(s, 0.0, 200.0), all = spec::bandPower(s, 0.0, 24000.0);
            CHECK_MESSAGE(low / all >= 0.9, "fed by " << std::string(kick ? "the ghost kick" : "a bright send (white noise)")
                                                      << " at rumble_drive " << drive << ", only " << 100.0 * low / all
                                                      << "% of the rumble's energy is below 200 Hz (tone 110 Hz); want 90% or "
                                                         "more: low-pass before and after the saturation");
        }
    }
}

TEST_CASE("P2.06 the rumble's tail decays in rumble_decay seconds (RT60)") {
    auto tailDrop = [](float decay) {
        auto p = rumblePatch();
        spec::set(p, "rumble_decay", decay);
        spec::set(p, "rumble_drive", 0);
        spec::set(p, "rumble_duck", 0);
        auto rumble = make(p);
        run(rumble, static_cast<std::size_t>(16 * kBeat), 0.0, true);
        // Stop the transport just after a kick's sound has died: the tail alone remains.
        const auto tail = run(rumble, static_cast<std::size_t>(12.0 * kRate), 16.0, false);
        const auto envelope = spec::rmsEnvelope(tail.out, 2400, 2400); // 50 ms
        const double start = envelope[2];
        double tenDown = -1, fortyDown = -1;
        for (std::size_t i = 2; i < envelope.size(); ++i) {
            const double db = spec::toDb(envelope[i] / start);
            if (tenDown < 0 && db <= -10.0)
                tenDown = 0.05 * static_cast<double>(i);
            if (fortyDown < 0 && db <= -40.0)
                fortyDown = 0.05 * static_cast<double>(i);
        }
        return fortyDown > 0 && tenDown > 0 ? fortyDown - tenDown : 1e9;
    };
    for (float decay : {2.0f, 6.0f}) {
        const double thirty = tailDrop(decay);
        CHECK_MESSAGE(thirty >= 0.5 * decay * 0.65, "with rumble_decay " << decay << " s the tail took " << thirty
                                                                       << " s to fall from -10 to -40 dB; a 60 dB decay in "
                                                                          "rumble_decay seconds takes "
                                                                       << 0.5 * decay << " s for 30 dB (within 35%)");
        CHECK_MESSAGE(thirty <= 0.5 * decay * 1.35, "with rumble_decay " << decay << " s the tail took " << thirty
                                                                       << " s to fall from -10 to -40 dB; a 60 dB decay in "
                                                                          "rumble_decay seconds takes "
                                                                       << 0.5 * decay << " s for 30 dB (within 35%)");
    }
}

TEST_CASE("P2.07 the ghost kick is never heard dry; the send feeds the same reverb; level 0 is silence") {
    auto rumble = make(rumblePatch());
    const auto r = run(rumble, static_cast<std::size_t>(8 * kBeat), 0.0, true);
    const double peak = spec::peakAbs(r.out);
    CHECK_MESSAGE(spec::peakAbs(r.out, 0, 480) <= 0.01 * peak, "the first 10 ms after the first ghost kick already reach "
                                                                   << spec::peakAbs(r.out, 0, 480) / peak
                                                                   << " of the rumble's peak: the kick must feed the reverb "
                                                                      "only, never the output");

    auto p = rumblePatch();
    spec::set(p, "rumble_kick", 0);
    auto sent = make(p);
    spec::Signal burst(9600);
    for (std::size_t n = 0; n < burst.size(); ++n)
        burst[n] = static_cast<float>(0.5 * (2.0 * std::fmod(n * 110.0 / kRate, 1.0) - 1.0));
    const auto t = run(sent, static_cast<std::size_t>(kRate), 0.0, false, &burst);
    CHECK_MESSAGE(spec::rms(t.out, 14400, 38400) > 0.001, "a 200 ms burst sent into the rumble must leave a tail 0.3 to 0.8 s "
                                                          "later (RMS "
                                                              << spec::rms(t.out, 14400, 38400) << ")");

    spec::set(p, "rumble_kick", 1);
    spec::set(p, "rumble_level", 0);
    auto silent = make(p);
    const auto z = run(silent, static_cast<std::size_t>(8 * kBeat), 0.0, true, &burst);
    CHECK_MESSAGE(spec::peakAbs(z.out) == 0.0, "rumble_level 0 must output exact silence; got " << spec::peakAbs(z.out));
}

TEST_CASE("P2.08 the rumble stays bounded at full settings, never allocates, and its tail ends in silence") {
    auto p = rumblePatch();
    spec::set(p, "rumble_decay", 8);
    spec::set(p, "rumble_drive", 1);
    spec::set(p, "rumble_duck", 0);
    spec::set(p, "rumble_tone", 250);
    spec::set(p, "rumble_rate", 3);
    auto rumble = make(p);
    std::uint32_t state = 99;
    double peak = 0;
    int nonFinite = 0;
    std::size_t allocations = 0;
    for (int block = 0; block < 1875; ++block) { // 10 s in blocks of 256
        spec::AllocationCounter counter;
        rumble.setParams(p.data());
        for (int n = 0; n < 256; ++n) {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            const float noise = static_cast<float>(state & 0xFFFF) / 32768.0f - 1.0f;
            const double ppq = (block * 256 + n) / (60.0 / kBpm * kRate);
            const float y = rumble.process(noise, ppq, kBpm, true);
            nonFinite += std::isfinite(y) ? 0 : 1;
            if (std::isfinite(y))
                peak = std::max(peak, static_cast<double>(std::fabs(y)));
        }
        allocations += counter.count();
    }
    CHECK_MESSAGE(nonFinite == 0, nonFinite << " non-finite rumble samples at full settings");
    CHECK_MESSAGE(peak <= 2.0, "the rumble reached " << peak << " at full settings with full-scale noise sent in; keep it within 2");
    CHECK_MESSAGE(allocations == 0, allocations << " heap allocations in Rumble::process or setParams: size the reverb in prepare()");

    // When the transport stops, the tail dies away to exact silence, never through denormals.
    auto tailPatch = rumblePatch();
    auto fading = make(tailPatch);
    run(fading, static_cast<std::size_t>(16 * kBeat), 0.0, true);
    const auto tail = run(fading, static_cast<std::size_t>(40 * kRate), 16.0, false);
    CHECK_MESSAGE(spec::countSubnormal(tail.out) == 0, spec::countSubnormal(tail.out)
                                                           << " denormal samples in the rumble's tail: flush the reverb and "
                                                              "filter state below about 1e-20 to zero");
    CHECK_MESSAGE(spec::peakAbs(tail.out, tail.out.size() - static_cast<std::size_t>(5 * kRate)) == 0.0,
                  "35 s after the transport stopped (rumble_decay 2.5 s) the rumble still outputs "
                      << spec::peakAbs(tail.out, tail.out.size() - static_cast<std::size_t>(5 * kRate))
                      << ": its tail must end in exact zeros");
}
