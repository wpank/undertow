// SPDX-License-Identifier: AGPL-3.0-only
// Phase 1 spec tests: the mono output stage, its soft clipper, DC blocker, master gain and clip meter
// (docs/specs/phase-1-foundation.md, check P1.08).
// spec-needs: core/include/undertow/output_stage.h

#include "doctest.h"

#include "spec_analysis.h"
#include "spec_engine.h"
#include "undertow/output_stage.h"

#include <cmath>
#include <limits>

namespace {

constexpr double kRate = 48000.0;

spec::Params outputPatch(float masterDb) {
    auto p = spec::defaults();
    spec::set(p, undertow::param::MasterGain, masterDb);
    return p;
}

undertow::OutputStage make(const spec::Params& p) {
    undertow::OutputStage stage;
    stage.prepare(kRate);
    stage.setParams(p.data());
    stage.reset();
    return stage;
}

spec::Signal run(undertow::OutputStage& stage, const spec::Signal& in) {
    spec::Signal out(in.size());
    for (std::size_t n = 0; n < in.size(); ++n)
        out[n] = stage.process(in[n]);
    return out;
}

} // namespace

TEST_CASE("P1.08 the soft clipper is transparent to -6 dBFS and never passes -0.3 dBFS") {
    for (float x = -0.5f; x <= 0.5f; x += 0.001f)
        REQUIRE_MESSAGE(undertow::softClip(x) == x, "softClip(" << x << ") is " << undertow::softClip(x)
                                                                << ": it must be exactly x for |x| <= 0.5");
    float previous = undertow::softClip(0.0f);
    float highest = 0.0f;
    for (float x = 0.0f; x <= 64.0f; x += 0.0005f) {
        const float y = undertow::softClip(x);
        REQUIRE_MESSAGE(y >= previous, "softClip must never fall as its input rises (at " << x << ")");
        REQUIRE_MESSAGE(undertow::softClip(-x) == -y, "softClip must be odd: softClip(-x) = -softClip(x)");
        previous = y;
        highest = std::max(highest, y);
    }
    CHECK_MESSAGE(highest <= 0.966f, "softClip reached " << highest << "; the ceiling is 0.966 (-0.3 dBFS)");
    CHECK_MESSAGE(undertow::softClip(1e9f) <= 0.966f, "softClip of a huge value must stay at the ceiling");
    CHECK_MESSAGE(undertow::softClip(std::numeric_limits<float>::infinity()) <= 0.966f,
                  "softClip(inf) must be the ceiling");
    CHECK_MESSAGE(std::fabs(undertow::softClip(1.0f) - 0.869f) <= 0.005f,
                  "softClip(1.0) is " << undertow::softClip(1.0f)
                                      << "; the knee 0.5 + 0.466 * tanh((x - 0.5) / 0.466) gives 0.869");
}

TEST_CASE("P1.08 at 0 dB the stage keeps a signal under the clipper's knee as it is: its level and its shape") {
    auto stage = make(outputPatch(0.0f));
    const auto in = spec::sine(55.0, 0.45, static_cast<std::size_t>(2 * kRate), kRate);
    const auto out = run(stage, in);
    const double level = spec::toDb(spec::rms(out, 48000) / spec::rms(in, 48000));
    CHECK_MESSAGE(std::fabs(level) <= 0.1, "at master_gain 0 dB a 0.45 sine at 55 Hz changed level by "
                                               << level << " dB: below the clipper's knee the stage is the DC blocker "
                                                           "and the gain, nothing more");
    const auto s = spec::spectrum(out, 48000, 1u << 15, kRate);
    const double distortion = spec::toDb(spec::thd(s, 55.0, 10));
    CHECK_MESSAGE(distortion <= -60.0, "a 0.45 sine at 0 dB came out with harmonics at " << distortion
                                                                                         << " dB: no saturation is "
                                                                                            "left in the output stage, "
                                                                                            "and the clipper is the "
                                                                                            "identity below 0.5");
    CHECK_MESSAGE(spec::peakAbs(run(stage, spec::sine(55.0, 4.0, 24000, kRate))) <= 0.966,
                  "a 4.0 sine must leave the stage within the clipper's ceiling, 0.966");
}

TEST_CASE("P1.08 a DC blocker: a constant offset leaves the output centred, and 30 Hz keeps its level") {
    auto stage = make(outputPatch(0.0f));
    const auto offset = run(stage, spec::Signal(static_cast<std::size_t>(0.5 * kRate), 0.4f));
    const double centre = spec::mean(offset, static_cast<std::size_t>(0.2 * kRate), offset.size());
    CHECK_MESSAGE(std::fabs(centre) <= 0.01, "a constant 0.4 at the input leaves the output's mean at "
                                                 << centre
                                                 << " after 0.2 s (want within 0.01): block DC first in the output "
                                                    "stage, a one-pole high-pass of about 5 Hz");
    auto bass = make(outputPatch(0.0f));
    const auto sub = run(bass, spec::sine(30.0, 0.4, static_cast<std::size_t>(kRate), kRate));
    const double level = spec::toDb(spec::rms(sub, 24000) / (0.4 / std::sqrt(2.0)));
    CHECK_MESSAGE(std::fabs(level) <= 0.5, "a 30 Hz sine changed level by " << level
                                                                           << " dB: the DC blocker must leave the sub "
                                                                              "alone (keep it near 5 Hz)");
}

TEST_CASE("P1.08 master_gain is smoothed: an 18 dB step never jumps the output, and settles within 50 ms") {
    // A held 187.5 Hz sine (256 samples a cycle), master_gain stepped between -18 and 0 dB every 0.25 s. Each step
    // lands 45 degrees earlier in the cycle than the one before, so one of the four lands near a peak, where an
    // unsmoothed gain jumps.
    auto p = outputPatch(-18.0f);
    auto stage = make(p);
    const auto in = spec::sine(187.5, 0.4, 60000, kRate);
    spec::Signal out(in.size());
    const std::size_t steps[] = {12000, 24000, 36000, 48000};
    std::size_t next = 0;
    for (std::size_t n = 0; n < in.size(); ++n) {
        if (next < 4 && n == steps[next]) {
            spec::set(p, undertow::param::MasterGain, next % 2 == 0 ? 0.0f : -18.0f);
            stage.setParams(p.data());
            ++next;
        }
        out[n] = stage.process(in[n]);
    }
    const double slope = 0.4 * 2.0 * 3.141592653589793 * 187.5 / kRate; // the sine's own steepest move at 0 dB
    for (int k = 0; k < 4; ++k) {
        const std::size_t at = steps[k];
        double jump = 0.0;
        for (std::size_t n = at - 2; n < at + 48; ++n)
            jump = std::max(jump, static_cast<double>(std::fabs(out[n] - out[n - 1])));
        INFO("step " << k << " (to " << (k % 2 == 0 ? 0 : -18) << " dB) at sample " << at);
        CHECK_MESSAGE(jump <= 2.0 * slope, "master_gain moved the output " << jump
                                                                           << " in one sample, where the sine itself "
                                                                              "moves at most "
                                                                           << slope
                                                                           << ": smooth master_gain over a few "
                                                                              "milliseconds");
        const double want = k % 2 == 0 ? 0.4 : 0.4 * std::pow(10.0, -18.0 / 20.0);
        const double settled = spec::toDb(spec::peakAbs(out, at + 2400, at + 2400 + 256) / want);
        CHECK_MESSAGE(std::fabs(settled) <= 0.2, "50 ms after the step the level is "
                                                     << settled << " dB from its target: master_gain must settle "
                                                                   "within 50 ms");
    }
}

TEST_CASE("P1.08 the clip meter lights for half a second when the signal goes over full scale") {
    auto stage = make(outputPatch(0.0f));
    run(stage, spec::sine(100.0, 0.9, 24000, kRate));
    CHECK_MESSAGE(!stage.clipped(), "a 0.9 sine never goes over full scale: clipped() must be false");
    const auto hot = run(stage, spec::sine(100.0, 1.5, 4800, kRate));
    CHECK_MESSAGE(stage.clipped(), "a 1.5 sine goes over full scale: clipped() must be true");
    CHECK_MESSAGE(spec::peakAbs(hot) <= 0.966, "the output passed the ceiling: " << spec::peakAbs(hot));
    run(stage, spec::Signal(static_cast<std::size_t>(0.4 * kRate), 0.0f));
    CHECK_MESSAGE(stage.clipped(), "clipped() must hold for 0.5 s after the last sample over full scale (0.4 s "
                                   "later it is off)");
    run(stage, spec::Signal(static_cast<std::size_t>(0.2 * kRate), 0.0f));
    CHECK_MESSAGE(!stage.clipped(), "clipped() must fall 0.5 s after the last sample over full scale (0.6 s later "
                                    "it is on)");
}

TEST_CASE("P1.08 silence in is exact silence out, and reset() clears the stage") {
    auto stage = make(outputPatch(0.0f));
    run(stage, spec::sine(40.0, 0.8, 24000, kRate));
    const auto tail = run(stage, spec::Signal(static_cast<std::size_t>(3.0 * kRate), 0.0f));
    const std::size_t from = static_cast<std::size_t>(2.5 * kRate);
    CHECK_MESSAGE(spec::peakAbs(tail, from) == 0.0, "2.5 s after the input fell silent the output is still "
                                                        << spec::peakAbs(tail, from)
                                                        << ": flush the DC blocker's state to exact zero once it is "
                                                           "negligible (below about 1e-20)");
    CHECK_MESSAGE(spec::countSubnormal(tail) == 0, "the silent tail holds " << spec::countSubnormal(tail)
                                                                            << " denormal samples");
    run(stage, spec::Signal(4800, 1.5f));
    REQUIRE_MESSAGE(stage.clipped(), "a constant 1.5 goes over full scale: clipped() must be true");
    stage.reset();
    const auto after = run(stage, spec::Signal(480, 0.0f));
    CHECK_MESSAGE(spec::peakAbs(after) == 0.0, "after reset() silence in must be silence out at once; got "
                                                   << spec::peakAbs(after));
    CHECK_MESSAGE(!stage.clipped(), "reset() must clear the clip meter");
}
