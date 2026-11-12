// SPDX-License-Identifier: AGPL-3.0-only
// Phase 5 spec tests: the darker rumble and the output stage's sub-sonic high-pass (docs/specs/phase-5-sound.md,
// checks P5.01 to P5.10).
// spec-needs: core/include/undertow/rumble.h

#include "doctest.h"

#include "spec_alloc.h"
#include "spec_analysis.h"
#include "spec_engine.h"
#include "spec_sound.h"
#include "undertow/engine.h"
#include "undertow/output_stage.h"
#include "undertow/rumble.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr double kRate = 48000.0;
constexpr double kBpm = 130.0;
constexpr double kBeat = 60.0 / kBpm * kRate; // samples
using namespace undertow::param;

// The rumble alone in the engine, as the PRD measures it: the voice's sources and its send off, rumble_level 1, the
// host playing 9 bars at 130 BPM; the first bar, where the bed builds, is left out.
spec::Signal rumbleAlone(spec::Params p = spec::defaults()) {
    for (const int i : {Vco1Level, Vco2Level, SubLevel, NoiseLevel, RumbleSend})
        spec::set(p, i, 0);
    spec::set(p, RumbleLevel, 1);
    undertow::Engine e;
    e.prepare(kRate, 512);
    spec::apply(e, p);
    e.reset();
    const auto all = spec::render(e, static_cast<std::size_t>(36 * kBeat), {}, {true, true, kBpm, 0.0}).left;
    return spec::Signal(all.begin() + static_cast<long>(4 * kBeat), all.end());
}

// The beats of the measured part, but its first and last.
std::vector<std::size_t> beats(const spec::Signal& x) {
    std::vector<std::size_t> out;
    for (int i = 1; i < static_cast<int>(static_cast<double>(x.size()) / kBeat) - 1; ++i)
        out.push_back(static_cast<std::size_t>(std::lround(i * kBeat)));
    return out;
}

undertow::Rumble made(const spec::Params& p) {
    undertow::Rumble r;
    r.prepare(kRate);
    r.setParams(p.data());
    r.reset();
    return r;
}

} // namespace

TEST_CASE("P5.01 the rumble is dark: 97% of its power below 150 Hz, its centroid at most 85 Hz, nothing above 500 Hz") {
    const auto x = rumbleAlone();
    REQUIRE_MESSAGE(spec::rms(x) > 1e-3, "the rumble alone at its defaults is silent: RMS " << spec::rms(x));
    const auto l = spec::ltas(x, kRate);
    const double below = spec::share(l, 0, 150), centroid = spec::centroidHz(l);
    const double above = 10 * std::log10(spec::share(l, 500, 24001));
    CHECK_MESSAGE(below >= 0.97, (100 * below) << "% of the rumble's power is below 150 Hz; want 97% or more");
    CHECK_MESSAGE(centroid <= 85.0, "the rumble's centroid is " << centroid << " Hz; want 85 Hz at most");
    CHECK_MESSAGE(above <= -60.0, "the band above 500 Hz is " << above
                                                              << " dB re the total; want -60 dB or less: the low-pass "
                                                                 "at rumble_tone comes after the drive");
}

TEST_CASE("P5.02 the rumble breathes: 10 dB down 12 ms after each kick, back within 3 dB by 400 ms") {
    const auto x = rumbleAlone();
    const auto env = spec::envelopeDb(x, kRate, 0.010);
    const auto on = beats(x);
    auto average = [&](double ms) { // the mean power over beats, in dB
        double sum = 0;
        for (const auto o : on)
            sum += std::pow(10.0, env[static_cast<std::size_t>(static_cast<double>(o) + ms * kRate / 1000.0)] / 10.0);
        return 10 * std::log10(sum / static_cast<double>(on.size()));
    };
    const double before = average(-12), dip = average(12) - before, back = average(400) - before;
    CHECK_MESSAGE(dip <= -10.0, "12 ms after the kicks the rumble is " << dip
                                                                       << " dB from its level 12 ms before them; want "
                                                                          "-10 dB or less: the duck");
    CHECK_MESSAGE(std::fabs(back) <= 3.0, "400 ms after the kicks the rumble is " << back
                                                                                  << " dB from its level before them; "
                                                                                     "want it back within 3 dB");
}

TEST_CASE("P5.03 the rumble moves: from beat to beat its bed differs by at least 1 dB") {
    const auto x = rumbleAlone();
    const auto env = spec::envelopeDb(x, kRate, 0.010);
    const auto on = beats(x);
    std::vector<double> spreads;
    for (auto k = static_cast<std::size_t>(0.15 * kRate); k < static_cast<std::size_t>(0.45 * kRate); ++k) {
        double mean = 0, square = 0;
        for (std::size_t i = 1; i < on.size(); ++i)
            mean += env[on[i] + k];
        mean /= static_cast<double>(on.size() - 1);
        for (std::size_t i = 1; i < on.size(); ++i)
            square += (env[on[i] + k] - mean) * (env[on[i] + k] - mean);
        spreads.push_back(std::sqrt(square / static_cast<double>(on.size() - 1)));
    }
    std::sort(spreads.begin(), spreads.end());
    const double spread = spreads[spreads.size() / 2];
    CHECK_MESSAGE(spread >= 1.0, "150 to 450 ms after each kick the bed's 10 ms envelope spreads only "
                                     << spread
                                     << " dB from beat to beat (median over the window); want 1 dB or more: a reverb "
                                        "that never moves repeats every beat exactly, notches and all; let each "
                                        "line's delay drift slowly");
}

TEST_CASE("P5.04 the duck never splatters: no 20 ms frame puts more than -25 dB of its power above 300 Hz") {
    const auto x = rumbleAlone();
    double worst = -300;
    for (std::size_t s = 0; s + 1024 <= x.size(); s += 512) {
        const auto f = spec::hannSpectrum(x, s, 1024, kRate);
        double total = 0, high = 0;
        for (std::size_t k = 0; k < f.amplitude.size(); ++k) {
            const double hz = static_cast<double>(k) * f.binHz(), p = f.amplitude[k] * f.amplitude[k];
            total += hz >= 20 ? p : 0;
            high += hz >= 300 ? p : 0;
        }
        if (total > 1e-18)
            worst = std::max(worst, 10 * std::log10(std::max(high, 1e-30) / total));
    }
    CHECK_MESSAGE(worst <= -25.0, "a 21 ms frame of the rumble puts " << worst
                                                                      << " dB of its power above 300 Hz; want -25 dB "
                                                                         "or less: the duck's attack falls along half "
                                                                         "a cosine over 8 ms");
}

TEST_CASE("P5.05 the duck: down to 1 - rumble_duck along half a cosine in 8 ms on every grid point, back in 150 ms") {
    const double bpm = 120.0, beat = 60.0 / bpm * kRate; // 24000 samples
    auto duckOf = [&](const spec::Params& p, std::size_t samples, bool running = true, double from = 0.0) {
        auto r = made(p);
        std::vector<double> g(samples);
        for (std::size_t i = 0; i < samples; ++i) {
            r.process(0.0f, from + (running ? static_cast<double>(i) / beat : 0.0), bpm, running);
            g[i] = r.duckGain();
        }
        return g;
    };
    auto p = spec::defaults();
    spec::set(p, RumbleKick, 0); // the duck follows the grid with the ghost kick off too
    const auto g = duckOf(p, static_cast<std::size_t>(3.5 * beat));
    const auto at = [&](double ms) { return g[static_cast<std::size_t>(2 * beat + ms * kRate / 1000.0)]; };
    const double floor = 1.0 - 0.88, start = at(-0.05);
    CHECK_MESSAGE(at(8.0) == doctest::Approx(floor).epsilon(0.03), "8 ms after the grid point the duck must reach 1 - "
                                                                   "rumble_duck (0.12); it is "
                                                                       << at(8.0));
    CHECK_MESSAGE(at(4.0) == doctest::Approx(0.5 * (start + floor)).epsilon(0.05),
                  "halfway through its 8 ms the duck must be halfway down (half a cosine): " << at(4.0) << ", want "
                                                                                            << 0.5 * (start + floor));
    CHECK_MESSAGE(at(158.0) == doctest::Approx(1.0 - 0.88 / std::exp(1.0)).epsilon(0.02),
                  "150 ms after the duck's floor its gain must be 1 - 0.88 / e (a 150 ms time constant); it is "
                      << at(158.0));
    double largest = 0;
    for (std::size_t i = 1; i < g.size(); ++i)
        largest = std::max(largest, std::fabs(g[i] - g[i - 1]));
    CHECK_MESSAGE(largest <= 0.01, "the duck's gain stepped by " << largest << " in one sample: it must glide");

    // The grid: every rumble_rate step, from the host's position; a jump back to a grid point ducks at once; nothing
    // while the clock is stopped; rumble_duck 0 never ducks.
    const double gridBeats[4] = {1, 2, 4, 0.5};
    for (int rate = 0; rate < 4; ++rate) {
        auto q = p;
        spec::set(q, RumbleRate, static_cast<float>(rate));
        const auto d = duckOf(q, static_cast<std::size_t>(8.5 * beat));
        int dips = 0, wrong = 0;
        for (std::size_t i = 1; i < d.size(); ++i)
            if (d[i - 1] > 0.5 && d[i] <= 0.5) { // halfway down: 4 ms after a grid point
                ++dips;
                const double beatAt = (static_cast<double>(i) - 0.004 * kRate) / beat;
                const double off = std::fabs(beatAt / gridBeats[rate] - std::round(beatAt / gridBeats[rate]));
                wrong += off * gridBeats[rate] * beat > 48.0 ? 1 : 0;
            }
        INFO("rumble_rate " << rate);
        CHECK_MESSAGE(dips == static_cast<int>(std::ceil(8.5 / gridBeats[rate])),
                      dips << " ducks in 8.5 beats; want one on every grid point of " << gridBeats[rate]
                           << " beats, from beat 0");
        CHECK_MESSAGE(wrong == 0, wrong << " ducks fell more than 1 ms from their grid point");
    }
    const auto stopped = duckOf(p, static_cast<std::size_t>(4 * beat), false, 2.0);
    CHECK_MESSAGE(*std::min_element(stopped.begin(), stopped.end()) == 1.0, "with the clock stopped nothing ducks");
    auto none = p;
    spec::set(none, RumbleDuck, 0);
    const auto open = duckOf(none, static_cast<std::size_t>(4 * beat));
    CHECK_MESSAGE(*std::min_element(open.begin(), open.end()) == 1.0, "rumble_duck 0 never ducks");
    // A jump back to beat 0 from mid-beat ducks there at once.
    auto r = made(p);
    for (int i = 0; i < static_cast<int>(beat * 1.5); ++i)
        r.process(0.0f, static_cast<double>(i) / beat, bpm, true);
    double lowest = 1.0;
    for (int i = 0; i < static_cast<int>(0.010 * kRate); ++i) {
        r.process(0.0f, static_cast<double>(i) / beat, bpm, true);
        lowest = std::min(lowest, r.duckGain());
    }
    CHECK_MESSAGE(lowest <= 0.2, "a jump back to beat 0 must duck there at once; the gain only fell to " << lowest);
}

TEST_CASE("P5.06 rumble_decay sets the tail: one ghost kick rings down 60 dB in that time, within 15%") {
    const double bpm = 120.0, beat = 60.0 / bpm * kRate;
    for (const float decay : {1.0f, 2.0f, 4.5f, 8.0f}) {
        auto p = spec::defaults();
        spec::set(p, RumbleDecay, decay);
        spec::set(p, RumbleDuck, 0);
        spec::set(p, RumbleLevel, 1);
        auto r = made(p);
        const auto n = static_cast<std::size_t>((1.6 * decay + 1.0) * kRate);
        std::vector<double> y(n);
        for (std::size_t i = 0; i < n; ++i) { // the clock runs half a beat from beat 0, one kick, then stops
            const bool running = static_cast<double>(i) < 0.5 * beat;
            y[i] = r.process(0.0f, running ? static_cast<double>(i) / beat : 0.5, bpm, running);
        }
        std::vector<double> energy(n + 1, 0.0); // Schroeder's backward integral
        for (std::size_t i = n; i-- > 0;)
            energy[i] = energy[i + 1] + y[i] * y[i];
        REQUIRE(energy[0] > 0.0);
        auto when = [&](double db) {
            for (std::size_t i = 0; i < n; ++i)
                if (10 * std::log10(energy[i] / energy[0]) <= db)
                    return static_cast<double>(i) / kRate;
            return -1.0;
        };
        const double t60 = 2.0 * (when(-35) - when(-5));
        INFO("rumble_decay " << decay << " s");
        CHECK_MESSAGE(t60 == doctest::Approx(decay).epsilon(0.15), "the tail falls 60 dB in "
                                                                       << t60
                                                                       << " s (from its fall from -5 to -35 dB); want "
                                                                          "rumble_decay within 15%");
    }
}

TEST_CASE("P5.07 rumble_tone moves the darkness: the centroid rises with it, nothing reaches 500 Hz, and the low-pass "
          "falls 24 dB an octave") {
    const double bpm = 120.0, beat = 60.0 / bpm * kRate;
    double last = 0;
    for (const float tone : {40.0f, 70.0f, 100.0f, 160.0f, 250.0f}) {
        auto p = spec::defaults();
        spec::set(p, RumbleTone, tone);
        spec::set(p, RumbleLevel, 1);
        auto r = made(p);
        spec::Signal y(static_cast<std::size_t>(36 * beat));
        for (std::size_t i = 0; i < y.size(); ++i)
            y[i] = r.process(0.0f, static_cast<double>(i) / beat, bpm, true);
        const auto l = spec::ltas(spec::Signal(y.begin() + static_cast<long>(4 * beat), y.end()), kRate);
        const double centroid = spec::centroidHz(l), above = 10 * std::log10(spec::share(l, 500, 24001));
        INFO("rumble_tone " << tone << " Hz");
        CHECK_MESSAGE(centroid > last, "the centroid " << centroid << " Hz must rise with the tone (it was " << last
                                                       << " Hz one setting lower)");
        CHECK_MESSAGE(above <= -50.0, "the band above 500 Hz is " << above << " dB re the total; want -50 dB or less");
        last = centroid;
    }
    // The slope: white noise sent in, with the drive, the duck and the ghost kick out of the way and the clock
    // stopped. Against the band from 0.4 to 1 times the tone, two to four times it is 20 dB under and four to eight
    // times 40 dB under: a fourth-order low-pass; a second-order one leaves about 11 and 20 dB.
    for (const float tone : {60.0f, 100.0f, 160.0f}) {
        auto p = spec::defaults();
        spec::set(p, RumbleTone, tone);
        spec::set(p, RumbleDrive, 0);
        spec::set(p, RumbleDuck, 0);
        spec::set(p, RumbleDecay, 1.0f);
        spec::set(p, RumbleKick, 0);
        spec::set(p, RumbleLevel, 1);
        auto r = made(p);
        std::uint32_t state = 7;
        spec::Signal y(static_cast<std::size_t>(6 * kRate));
        for (auto& v : y) {
            state = state * 1664525u + 1013904223u;
            const auto noise = static_cast<float>((state >> 8) / 16777216.0 - 0.5) * 0.02f;
            v = r.process(noise, 0.0, 120.0, false);
        }
        const auto l = spec::ltas(spec::Signal(y.begin() + static_cast<long>(kRate), y.end()), kRate);
        const double below = spec::share(l, 0.4 * tone, tone);
        const double one = 10 * std::log10(spec::share(l, 2 * tone, 4 * tone) / below);
        const double two = 10 * std::log10(spec::share(l, 4 * tone, 8 * tone) / below);
        INFO("rumble_tone " << tone << " Hz");
        CHECK_MESSAGE(one <= -20.0, "two to four times the tone the noise is only " << one
                                                                                    << " dB under the band below it; "
                                                                                       "want 20 dB: a 24 dB/octave "
                                                                                       "low-pass, two biquads");
        CHECK_MESSAGE(two <= -40.0, "four to eight times the tone the noise is only " << two
                                                                                      << " dB under the band below "
                                                                                         "it; want 40 dB");
    }
}

TEST_CASE("P5.08 under random settings the rumble stays finite and bounded, never allocates, fits its CPU budget, "
          "and is exactly silent at level 0 and after reset") {
    undertow::Rumble r;
    r.prepare(kRate);
    auto p = spec::defaults();
    r.setParams(p.data());
    r.reset();
    std::uint32_t state = 31;
    auto next = [&]() {
        state = state * 1664525u + 1013904223u;
        return static_cast<double>(state >> 8) / 16777216.0;
    };
    int nonFinite = 0;
    double peak = 0, ppq = 0, bpm = 130;
    bool running = true;
    spec::AllocationCounter counter;
    const double start = spec::threadSeconds();
    for (int block = 0; block < 900; ++block) { // 9.6 s in blocks of 512
        if (block % 10 == 0) {
            for (const int i : {RumbleLevel, RumbleKick, RumbleRate, RumbleDecay, RumbleTone, RumbleDrive, RumbleDuck}) {
                const auto& spec = undertow::paramSpec(i);
                p[static_cast<std::size_t>(i)] = spec.min + static_cast<float>(next()) * (spec.max - spec.min);
            }
            p[RumbleRate] = std::round(p[RumbleRate]);
            r.setParams(p.data());
            bpm = 60 + 140 * next();
            running = next() < 0.85;
            if (next() < 0.3)
                ppq = 64 * next();
        }
        for (int n = 0; n < 512; ++n) {
            const float send = static_cast<float>(0.8 * std::sin(0.002 * (block * 512 + n)) * next());
            const double y = r.process(send, ppq, bpm, running);
            nonFinite += std::isfinite(y) ? 0 : 1;
            peak = std::max(peak, std::fabs(y));
            if (running)
                ppq += bpm / (60.0 * kRate);
        }
    }
    const double seconds = spec::threadSeconds() - start;
    const std::size_t allocations = counter.count();
    CHECK_MESSAGE(nonFinite == 0, nonFinite << " non-finite samples under random settings");
    CHECK_MESSAGE(peak <= 4.0, "the rumble reached " << peak << " under random settings; its drive bounds it");
    CHECK_MESSAGE(allocations == 0, allocations << " heap allocations in process() or setParams(): size the delay "
                                                   "lines in prepare()");
    CHECK_MESSAGE(seconds <= 0.3, "9.6 s of the rumble took " << seconds << " s of CPU; the budget is 0.3 s");

    // Level 0 is exact silence; reset() silences the tail at once.
    auto q = spec::defaults();
    spec::set(q, RumbleLevel, 0);
    auto quiet = made(q);
    double loudest = 0;
    for (int i = 0; i < 48000; ++i)
        loudest = std::max(loudest, std::fabs(static_cast<double>(quiet.process(0.5f, i / 22153.8, kBpm, true))));
    CHECK_MESSAGE(loudest == 0.0, "rumble_level 0 must output exact zeros; it reached " << loudest);
    auto ringing = made(spec::defaults());
    for (int i = 0; i < 48000; ++i)
        ringing.process(0.0f, i / 22153.8, kBpm, true);
    ringing.reset();
    double after = 0;
    for (int i = 0; i < 4800; ++i) // the clock stopped: no new kick
        after = std::max(after, std::fabs(static_cast<double>(ringing.process(0.0f, 0.5, kBpm, false))));
    CHECK_MESSAGE(after == 0.0, "after reset() the tail must be gone at once: the output reached " << after);
}

TEST_CASE("P5.09 the output stage cuts what lies under the sub: a 20 Hz fourth-order high-pass, 30 Hz kept") {
    // The steady-state gain of the whole stage at 0 dB master, from sines well under the clipper's knee.
    auto gainAt = [](double hz) {
        undertow::OutputStage stage;
        stage.prepare(kRate);
        auto p = spec::defaults();
        spec::set(p, MasterGain, 0);
        stage.setParams(p.data());
        stage.reset();
        const auto in = spec::sine(hz, 0.3, static_cast<std::size_t>(4 * kRate), kRate);
        spec::Signal out(in.size());
        for (std::size_t n = 0; n < in.size(); ++n)
            out[n] = stage.process(in[n]);
        return spec::toDb(spec::rms(out, static_cast<std::size_t>(2 * kRate)) /
                          spec::rms(in, static_cast<std::size_t>(2 * kRate)));
    };
    const double g10 = gainAt(10), g15 = gainAt(15), g20 = gainAt(20), g30 = gainAt(30), g41 = gainAt(41.2);
    CHECK_MESSAGE(g10 <= -20.0, "10 Hz passes at " << g10 << " dB; want -20 dB or less: a fourth-order Butterworth "
                                                           "high-pass at 20 Hz after the DC blocker");
    CHECK_MESSAGE(g15 <= -8.0, "15 Hz passes at " << g15 << " dB; want -8 dB or less");
    CHECK_MESSAGE((g20 >= -4.5 && g20 <= -2.5), "20 Hz passes at " << g20 << " dB; want about -3 dB (-4.5 to -2.5)");
    CHECK_MESSAGE(g30 >= -0.5, "30 Hz passes at " << g30 << " dB; want within 0.5 dB: the sub's register is kept");
    CHECK_MESSAGE(g41 >= -0.2, "E1 (41.2 Hz), the bottom of the sub's range, passes at " << g41
                                                                                       << " dB; want within 0.2 dB");
    // Silence in is still exact silence out.
    undertow::OutputStage stage;
    stage.prepare(kRate);
    auto p = spec::defaults();
    stage.setParams(p.data());
    stage.reset();
    for (int n = 0; n < 48000; ++n)
        stage.process(0.4f * std::sin(0.01f * static_cast<float>(n)));
    double tail = 0;
    for (int n = 0; n < static_cast<int>(3 * kRate); ++n) {
        const float y = stage.process(0.0f);
        if (n >= static_cast<int>(2.5 * kRate))
            tail = std::max(tail, std::fabs(static_cast<double>(y)));
    }
    CHECK_MESSAGE(tail == 0.0, "2.5 s after the input fell silent the stage still outputs " << tail
                                                                                        << ": flush the high-pass's "
                                                                                           "state to exact zero");
}

TEST_CASE("P5.10 the default rumble never fills the roll: Init's whole output still falls 8 dB before each next hit") {
    undertow::Engine e;
    e.prepare(kRate, 512);
    spec::apply(e, spec::defaults());
    e.reset();
    const auto samples = static_cast<std::size_t>(32 * kBeat);
    const auto out = spec::render(e, samples, {spec::noteOn(0, 43)}, {true, true, kBpm, 0.0}).left;
    const auto p = spec::defaults();
    std::vector<std::size_t> onsets; // the home steps that sound, at 1/16
    for (int k = 0; k < 128; ++k)
        if (p[static_cast<std::size_t>(Step1Vel + k % 8)] > 0.0f)
            onsets.push_back(static_cast<std::size_t>(std::ceil(k * 0.25 * kBeat - 1e-6)));
    const double drop = spec::medianDropBeforeNextDb(out, onsets, kRate);
    CHECK_MESSAGE(drop >= 8.0, "with the default rumble in, Init's output falls only " << drop
                                                                                     << " dB (median) before the next "
                                                                                        "hit; want 8 dB or more: the "
                                                                                        "rumble sits under the roll");
}
