// Phase 2 spec tests: saturation, the mono-sub crossover with width, the output clipper, the master's smoothing and
// the DC blocker (docs/specs/phase-2-rumble.md, P2.09 to P2.12, P2.20 and P2.21).
// spec-needs: core/include/undertow/output_stage.h

#include "doctest.h"

#include "spec_analysis.h"
#include "spec_engine.h"
#include "undertow/output_stage.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace {

constexpr double kRate = 48000.0;

spec::Params outputPatch(float drive, float width) {
    auto p = spec::defaults();
    spec::set(p, "sat_drive", drive);
    spec::set(p, "width", width);
    spec::set(p, "mono_freq", 120);
    spec::set(p, "master_gain", 0);
    return p;
}

undertow::OutputStage make(const spec::Params& p) {
    undertow::OutputStage stage;
    stage.prepare(kRate);
    stage.setParams(p.data());
    stage.reset();
    return stage;
}

spec::Stereo run(undertow::OutputStage& stage, const spec::Signal& in) {
    spec::Stereo out;
    out.left.resize(in.size());
    out.right.resize(in.size());
    for (std::size_t n = 0; n < in.size(); ++n)
        stage.process(in[n], out.left[n], out.right[n]);
    return out;
}

spec::Signal noise(std::size_t samples, double amplitude) {
    spec::Signal x(samples);
    std::uint32_t state = 2024;
    for (auto& v : x) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        v = static_cast<float>(amplitude * (static_cast<double>(state) / 4294967295.0 * 2.0 - 1.0));
    }
    return x;
}

} // namespace

TEST_CASE("P2.09 the soft clipper is transparent to -6 dBFS and never passes -0.3 dBFS") {
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
    CHECK_MESSAGE(undertow::softClip(std::numeric_limits<float>::infinity()) <= 0.966f, "softClip(inf) must be the ceiling");
    CHECK_MESSAGE(std::fabs(undertow::softClip(1.0f) - 0.869f) <= 0.005f,
                  "softClip(1.0) is " << undertow::softClip(1.0f)
                                      << "; the knee 0.5 + 0.466 * tanh((x - 0.5) / 0.466) gives 0.869");
}

TEST_CASE("P2.10 saturation: linear at 0, rounded and odd at 1, unity gain for quiet signals") {
    auto distortion = [](float drive, double amplitude) {
        auto stage = make(outputPatch(drive, 0.0f));
        const auto out = run(stage, spec::sine(1000.0, amplitude, (1u << 16) + 4800, kRate));
        spec::Signal tail(out.left.begin() + 4800, out.left.end());
        return std::pair<spec::Spectrum, spec::Signal>(spec::spectrum(tail, 0, 1u << 16, kRate), tail);
    };
    const auto clean = distortion(0.0f, 0.4);
    CHECK_MESSAGE(spec::toDb(spec::thd(clean.first, 1000.0, 10)) <= -80.0,
                  "sat_drive 0 must be linear: THD " << spec::toDb(spec::thd(clean.first, 1000.0, 10)) << " dB");
    const auto hot = distortion(1.0f, 0.4);
    CHECK_MESSAGE(spec::toDb(spec::thd(hot.first, 1000.0, 10)) >= -26.0,
                  "sat_drive 1 must distort a 0.4 sine (THD at least 5%): THD " << spec::toDb(spec::thd(hot.first, 1000.0, 10)) << " dB");
    CHECK_MESSAGE(spec::amplitudeAt(hot.first, 3000.0) >= 10.0 * spec::amplitudeAt(hot.first, 2000.0),
                  "saturation must be symmetric (odd harmonics, no 2nd)");
    CHECK_MESSAGE(std::fabs(spec::mean(hot.second)) <= 1e-3, "saturation must add no DC; the mean is " << spec::mean(hot.second));
    const auto quiet = distortion(1.0f, 0.01);
    const double gain = spec::toDb(spec::amplitudeAt(quiet.first, 1000.0) / 0.01);
    CHECK_MESSAGE(std::fabs(gain) <= 0.5, "at sat_drive 1 a quiet signal must pass at unity gain (within 0.5 dB); the gain is "
                                              << gain << " dB");
}

TEST_CASE("P2.11 below mono_freq the output is mono; width decorrelates only the band above it") {
    const auto in = noise(1u << 18, 0.25);
    auto mono = make(outputPatch(0.0f, 0.0f));
    const auto zero = run(mono, in);
    CHECK_MESSAGE(zero.left == zero.right, "at width 0 both channels must be the same signal, sample for sample");
    // At width 0 no band gains or loses level.
    const double bands[][2] = {{40, 80}, {80, 160}, {160, 320}, {320, 640}, {640, 1280}, {1280, 5000}, {5000, 15000}};
    const auto inSpectrum = spec::spectrum(in, 4096, 1u << 17, kRate);
    const auto outSpectrum = spec::spectrum(zero.left, 4096, 1u << 17, kRate);
    for (const auto& band : bands) {
        const double db = 10.0 * std::log10(spec::bandPower(outSpectrum, band[0], band[1]) / spec::bandPower(inSpectrum, band[0], band[1]));
        CHECK_MESSAGE(std::fabs(db) <= 0.5, "at width 0 the band " << band[0] << "-" << band[1] << " Hz changed by " << db
                                                                  << " dB: at width 0 every band must keep its level");
    }
    for (float width : {0.5f, 1.0f}) {
        auto stage = make(outputPatch(0.0f, width));
        const auto out = run(stage, in);
        const auto lowLeft = spec::lowpass(out.left, 60.0, kRate, 4), lowRight = spec::lowpass(out.right, 60.0, kRate, 4);
        const auto highLeft = spec::highpass(out.left, 500.0, kRate, 4), highRight = spec::highpass(out.right, 500.0, kRate, 4);
        const double low = spec::correlation(lowLeft, lowRight, 9600);
        const double high = spec::correlation(highLeft, highRight, 9600);
        INFO("width " << width);
        CHECK_MESSAGE(low >= 0.98, "below mono_freq / 2 the channels must stay mono; their correlation is " << low);
        if (width == 1.0f) {
            CHECK_MESSAGE(high <= 0.3, "at width 1 the band above 2 x mono_freq must be decorrelated; correlation " << high);
            // Phase 2.2 makes width a side signal added to the untouched mono signal: on broadband noise a channel
            // then gains up to 3 dB at width 1 (bass material much less).
            const double level = spec::toDb(spec::rms(out.left, 9600) / spec::rms(zero.left, 9600));
            CHECK_MESSAGE(level >= -1.5, "width must not turn a channel down by more than 1.5 dB; it moved " << level << " dB");
            CHECK_MESSAGE(level <= 3.5, "width may add its side, at most 3.5 dB on noise; it moved " << level << " dB");
        } else {
            CHECK_MESSAGE(high >= 0.4, "at width 0.5 the high band's correlation must lie between those of width 0 and 1 "
                                       "(0.4 .. 0.9); it is "
                                           << high);
            CHECK_MESSAGE(high <= 0.9, "at width 0.5 the high band's correlation must lie between those of width 0 and 1 "
                                       "(0.4 .. 0.9); it is "
                                           << high);
        }
    }
}

TEST_CASE("P2.12 the clip meter lights for half a second when the signal goes over full scale") {
    auto stage = make(outputPatch(0.0f, 0.0f));
    run(stage, spec::sine(100.0, 0.9, 24000, kRate));
    CHECK_MESSAGE(!stage.clipped(), "a 0.9 sine never goes over full scale: clipped() must be false");
    const auto hot = run(stage, spec::sine(100.0, 1.5, 4800, kRate));
    CHECK_MESSAGE(stage.clipped(), "a 1.5 sine goes over full scale: clipped() must be true");
    CHECK_MESSAGE(spec::peakAbs(hot.left) <= 0.966, "the output passed the ceiling: " << spec::peakAbs(hot.left));
    run(stage, spec::Signal(static_cast<std::size_t>(0.4 * kRate), 0.0f));
    CHECK_MESSAGE(stage.clipped(), "clipped() must hold for 0.5 s after the last sample over full scale (0.4 s later it is off)");
    run(stage, spec::Signal(static_cast<std::size_t>(0.2 * kRate), 0.0f));
    CHECK_MESSAGE(!stage.clipped(), "clipped() must fall 0.5 s after the last sample over full scale (0.6 s later it is on)");
}

TEST_CASE("P2.20 master_gain is smoothed: an 18 dB step never jumps the output, and settles within 50 ms") {
    // A held 187.5 Hz sine (256 samples a cycle), master_gain stepped between -18 and 0 dB every 0.25 s. Each step
    // lands 45 degrees earlier in the cycle than the one before, so whatever phase the crossover gives the sine, one
    // of the four lands near a peak, where an unsmoothed gain jumps.
    auto p = outputPatch(0.0f, 0.0f);
    spec::set(p, "master_gain", -18);
    auto stage = make(p);
    const auto in = spec::sine(187.5, 0.4, 60000, kRate);
    spec::Signal out(in.size());
    const std::size_t steps[] = {12000, 24000, 36000, 48000};
    float right = 0.0f;
    std::size_t next = 0;
    for (std::size_t n = 0; n < in.size(); ++n) {
        if (next < 4 && n == steps[next]) {
            spec::set(p, "master_gain", next % 2 == 0 ? 0.0f : -18.0f);
            stage.setParams(p.data());
            ++next;
        }
        stage.process(in[n], out[n], right);
    }
    const double slope = 0.4 * 2.0 * 3.141592653589793 * 187.5 / kRate; // the sine's own steepest move at 0 dB
    for (int k = 0; k < 4; ++k) {
        const std::size_t at = steps[k];
        double jump = 0.0;
        for (std::size_t n = at - 2; n < at + 48; ++n)
            jump = std::max(jump, static_cast<double>(std::fabs(out[n] - out[n - 1])));
        INFO("step " << k << " (to " << (k % 2 == 0 ? 0 : -18) << " dB) at sample " << at);
        CHECK_MESSAGE(jump <= 2.0 * slope, "master_gain moved the output " << jump << " in one sample, where the sine itself moves at most "
                                                                           << slope << ": smooth master_gain over a few milliseconds");
        const double want = k % 2 == 0 ? 0.4 : 0.4 * std::pow(10.0, -18.0 / 20.0);
        const double settled = spec::toDb(spec::peakAbs(out, at + 2400, at + 2400 + 256) / want);
        CHECK_MESSAGE(std::fabs(settled) <= 0.2, "50 ms after the step the level is " << settled << " dB from its target: master_gain must "
                                                                                       "settle within 50 ms");
    }
}

TEST_CASE("P2.21 a DC blocker: a constant offset leaves the output centred, and the sub keeps its level") {
    auto stage = make(outputPatch(0.0f, 0.0f));
    const auto offset = run(stage, spec::Signal(static_cast<std::size_t>(0.5 * kRate), 0.4f));
    const double centre = spec::mean(offset.left, static_cast<std::size_t>(0.2 * kRate), offset.left.size());
    CHECK_MESSAGE(std::fabs(centre) <= 0.01, "a constant 0.4 at the input leaves the output's mean at "
                                                 << centre << " after 0.2 s (want within 0.01): block DC first in the output "
                                                              "stage, a one-pole high-pass of about 5 Hz");
    auto bass = make(outputPatch(0.0f, 0.0f));
    const auto sub = run(bass, spec::sine(30.0, 0.4, static_cast<std::size_t>(kRate), kRate));
    const double level = spec::toDb(spec::rms(sub.left, 24000) / (0.4 / std::sqrt(2.0)));
    CHECK_MESSAGE(std::fabs(level) <= 0.5, "a 30 Hz sine changed level by " << level << " dB: the DC blocker must leave the sub alone "
                                                                                    "(keep it near 5 Hz)");
}
