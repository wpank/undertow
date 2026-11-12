// SPDX-License-Identifier: AGPL-3.0-only
// Phase 3 spec tests: the attack-decay envelope (docs/specs/phase-3-voice.md, checks P3.01 to P3.05).
// spec-needs: core/include/undertow/decay.h

#include "doctest.h"

#include "spec_alloc.h"
#include "undertow/decay.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr double kRate = 48000.0;

undertow::Decay make(double attackMs, double decayMs) {
    undertow::Decay d;
    d.prepare(kRate);
    d.setAttack(attackMs);
    d.setDecay(decayMs);
    d.reset();
    return d;
}

std::vector<double> run(undertow::Decay& d, std::size_t samples) {
    std::vector<double> out(samples);
    for (auto& v : out)
        v = d.process();
    return out;
}

// The first sample at which `level` reaches `fraction` of the way from `from` to `to`.
double firstAt(const std::vector<double>& level, double from, double to, double fraction) {
    const double target = from + fraction * (to - from);
    for (std::size_t n = 0; n < level.size(); ++n)
        if ((to > from && level[n] >= target) || (to < from && level[n] <= target))
            return static_cast<double>(n);
    return -1;
}

double largestStep(const std::vector<double>& level, double before) {
    double step = std::fabs(level.front() - before);
    for (std::size_t n = 1; n < level.size(); ++n)
        step = std::max(step, std::fabs(level[n] - level[n - 1]));
    return step;
}

} // namespace

TEST_CASE("P3.01 a hit rises along half a cosine: 10 to 90% of the way in the attack time, smooth at both ends") {
    for (const double ms : {1.0, 100.0, 500.0}) {
        auto d = make(ms, 20000.0);
        d.trigger(1.0f);
        const auto level = run(d, static_cast<std::size_t>((2.0 * ms / 1000.0 + 0.01) * kRate));
        const double t10 = firstAt(level, 0, 1, 0.1), t90 = firstAt(level, 0, 1, 0.9);
        const double rise = (t90 - t10) / kRate * 1000.0;
        INFO("attack " << ms << " ms");
        CHECK_MESSAGE(std::fabs(rise / ms - 1.0) <= 0.05, "the level rose from 10% to 90% in " << rise
                                                                                              << " ms; the attack "
                                                                                                 "time is that rise, "
                                                                                                 "within 5%");
        // Half a cosine over its full length T = attack / 0.5903: halfway at T / 2, nearly flat at both ends.
        const double length = ms / 0.59033 / 1000.0 * kRate;
        const double half = level[static_cast<std::size_t>(std::lround(length / 2.0)) - 1];
        CHECK_MESSAGE(std::fabs(half - 0.5) <= 0.02, "halfway through the rise the level is " << half
                                                                                              << "; half a cosine is "
                                                                                                 "0.5 there");
        const auto fifth = static_cast<std::size_t>(0.05 * length); // 5% of the way through the rise, or just before
        const double early = level[fifth - 1];
        CHECK_MESSAGE(early <= 0.01, "5% into the rise the level is already " << early
                                                                             << "; half a cosine starts flat (0.006 "
                                                                                "there), where a straight line would "
                                                                                "be at 0.05: the VCA comes after the "
                                                                                "ladder, and a line's corners splash "
                                                                                "highs past it");
        CHECK_MESSAGE(*std::max_element(level.begin(), level.end()) > 0.999, "the rise must reach the peak");
    }
}

TEST_CASE("P3.02 the decay is exponential and falls 40 dB in the decay time") {
    for (const double ms : {30.0, 300.0, 3000.0}) {
        auto d = make(0.0, ms);
        d.trigger(1.0f);
        const auto level = run(d, static_cast<std::size_t>(1.5 * ms / 1000.0 * kRate));
        const double t40 = firstAt(level, 1, 0, 0.99) / kRate * 1000.0;
        INFO("decay " << ms << " ms");
        CHECK_MESSAGE(std::fabs(t40 / ms - 1.0) <= 0.05, "the level fell to 1% (-40 dB) in " << t40
                                                                                            << " ms; want the decay "
                                                                                               "time within 5%");
        const double middle = 20.0 * std::log10(level[static_cast<std::size_t>(ms / 2000.0 * kRate)]);
        CHECK_MESSAGE(std::fabs(middle + 20.0) <= 0.5, "halfway through the decay the level is at "
                                                           << middle << " dB; an exponential is at -20 dB there");
    }
}

TEST_CASE("P3.03 a hit rises from wherever the level is, and a softer one glides down over at least 4 ms") {
    auto d = make(1.0, 300.0);
    d.trigger(1.0f);
    auto level = run(d, static_cast<std::size_t>(0.06 * kRate));
    const double before = level.back();
    d.trigger(0.8f);
    level = run(d, static_cast<std::size_t>(0.01 * kRate));
    CHECK_MESSAGE(largestStep(level, before) <= 0.042, "a retrigger moved the level "
                                                           << largestStep(level, before)
                                                           << " in one sample: rise from where the level is, never "
                                                              "from 0 or by a jump");
    CHECK_MESSAGE(std::fabs(*std::max_element(level.begin(), level.end()) - 0.8) <= 0.01,
                  "a retrigger to 0.8 must peak at 0.8");

    const double high = level.back();
    d.trigger(0.2f);
    level = run(d, static_cast<std::size_t>(0.02 * kRate));
    CHECK_MESSAGE(largestStep(level, high) <= 0.042, "a softer hit moved the level " << largestStep(level, high)
                                                                                     << " in one sample");
    const double reached = firstAt(level, high, 0.2, 0.99) / kRate * 1000.0;
    CHECK_MESSAGE(reached >= 3.5, "a softer hit came within 1% of its peak in " << reached
                                                                              << " ms; it glides down over at least "
                                                                                 "4 ms (half a cosine is within 1% "
                                                                                 "at 0.94 of its length)");
    CHECK_MESSAGE(reached <= 5.0, "a softer hit came within 1% of its peak in " << reached
                                                                              << " ms; with a 1 ms attack it glides "
                                                                                 "over 4 ms");

    auto r = make(100.0, 3000.0);
    r.trigger(1.0f);
    level = run(r, static_cast<std::size_t>(0.05 * kRate));
    const double mid = level.back();
    r.trigger(1.0f);
    const auto again = run(r, 48);
    CHECK_MESSAGE(std::fabs(again.front() - mid) <= 0.01, "a hit during a rise must rise on from where it is ("
                                                              << mid << "), not start over; it went to "
                                                              << again.front());
}

TEST_CASE("P3.04 below -100 dB the level is exact zero and the envelope is idle; reset silences it at once") {
    auto d = make(1.0, 50.0);
    CHECK_MESSAGE(!d.active(), "a new envelope is idle");
    CHECK_MESSAGE(d.process() == 0.0f, "an idle envelope is 0");
    d.trigger(1.0f);
    CHECK_MESSAGE(d.active(), "a hit makes it active");
    const auto level = run(d, static_cast<std::size_t>(0.5 * kRate));
    CHECK_MESSAGE(level.back() == 0.0, "half a second after a 50 ms decay the level is " << level.back()
                                                                                         << "; under -100 dB "
                                                                                            "(1e-5) it is exact 0");
    CHECK_MESSAGE(!d.active(), "under -100 dB the envelope is idle");
    d.trigger(0.5f);
    run(d, 100);
    d.reset();
    CHECK_MESSAGE(d.level() == 0.0f, "reset() puts the level at 0 at once");
    CHECK_MESSAGE(!d.active(), "reset() makes the envelope idle");
    CHECK_MESSAGE(d.process() == 0.0f, "after reset() it stays at 0");
}

TEST_CASE("P3.05 with no attack a hit jumps to its peak at once, and nothing allocates") {
    auto d = make(0.0, 75.0);
    spec::AllocationCounter counter;
    d.trigger(0.7f);
    const float first = d.process();
    for (int n = 0; n < 48000; ++n)
        d.process();
    d.setAttack(1.0);
    d.setDecay(300.0);
    d.trigger(1.0f);
    for (int n = 0; n < 48000; ++n)
        d.process();
    const std::size_t allocations = counter.count(); // read before CHECK: doctest's first assertion allocates
    CHECK_MESSAGE(first >= 0.699f, "with attack 0 the first sample after a hit is " << first
                                                                                    << "; it jumps to the peak, 0.7");
    CHECK_MESSAGE(first <= 0.7f, "with attack 0 the first sample is the peak at most");
    CHECK_MESSAGE(allocations == 0, allocations << " heap allocations in the envelope");
}
