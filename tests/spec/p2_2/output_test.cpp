// Phase 2.2 spec tests: width without reshaping the mono signal (docs/specs/phase-2.2-gain-staging.md, P2.2.01 and
// P2.2.02).
// spec-needs: core/include/undertow/output_stage.h

#include "doctest.h"

#include "spec_analysis.h"
#include "spec_engine.h"
#include "undertow/output_stage.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>

namespace {

constexpr double kRate = 48000.0;

spec::Params outputPatch(float width) {
    auto p = spec::defaults();
    spec::set(p, "sat_drive", 0);
    spec::set(p, "width", width);
    spec::set(p, "mono_freq", 120);
    spec::set(p, "master_gain", 0);
    return p;
}

spec::Stereo run(float width, const spec::Signal& in) {
    undertow::OutputStage stage;
    stage.prepare(kRate);
    stage.setParams(outputPatch(width).data());
    stage.reset();
    spec::Stereo out;
    out.left.resize(in.size());
    out.right.resize(in.size());
    for (std::size_t n = 0; n < in.size(); ++n)
        stage.process(in[n], out.left[n], out.right[n]);
    return out;
}

spec::Signal wave(bool square, double hz, std::size_t samples) {
    spec::Signal x(samples);
    for (std::size_t n = 0; n < samples; ++n) {
        const double phase = std::fmod(static_cast<double>(n) * hz / kRate, 1.0);
        x[n] = static_cast<float>(0.4 * (square ? (phase < 0.5 ? 1.0 : -1.0) : 2.0 * phase - 1.0));
    }
    return x;
}

double crestDb(const spec::Signal& x, std::size_t from) { return spec::toDb(spec::peakAbs(x, from) / spec::rms(x, from)); }

} // namespace

TEST_CASE("P2.2.01 at width 0 the output stage keeps a waveform's peaks: a saw's and a square's crest factor within 0.5 dB") {
    // A saw at 110 and 220 Hz and a square at 220 and 440 Hz, at 0.4 (under the clipper's knee), saturation off,
    // master 0 dB. A crossover whose bands are summed back is an all-pass: it rotates the phase around mono_freq and
    // raises these crest factors by 4 to 7 dB, at the same RMS.
    const std::pair<bool, double> waves[] = {{false, 110.0}, {false, 220.0}, {true, 220.0}, {true, 440.0}};
    for (const auto& w : waves) {
        const bool square = w.first;
        const double hz = w.second;
        const auto in = wave(square, hz, 96000);
        const auto out = run(0.0f, in);
        INFO(std::string(square ? "square" : "saw") << " at " << hz << " Hz");
        CHECK_MESSAGE(out.left == out.right, "at width 0 both channels must be the same signal");
        const double change = crestDb(out.left, 48000) - crestDb(in, 48000);
        CHECK_MESSAGE(std::fabs(change) <= 0.5, "the output stage changed the crest factor by " << change
                                                << " dB at width 0: pass the mono signal through no filter but the DC "
                                                   "blocker, and make width from a side signal alone");
    }
}

TEST_CASE("P2.2.02 width adds a high-passed side and leaves the mono signal alone; the low end stays mono") {
    // Noise at 0.25 (under the knee once the side is added). At width 0.5 and 1 the mid, (L + R) / 2, must be the
    // width-0 output itself, and the side, (L - R) / 2, must stay 30 dB under the mid below mono_freq / 2.
    spec::Signal in(1u << 17);
    std::uint32_t state = 2024;
    for (auto& v : in) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        v = static_cast<float>(0.25 * (static_cast<double>(state) / 4294967295.0 * 2.0 - 1.0));
    }
    const auto plain = run(0.0f, in);
    for (const float width : {0.5f, 1.0f}) {
        const auto out = run(width, in);
        spec::Signal mid(in.size()), side(in.size());
        double error = 0, power = 0;
        for (std::size_t n = 0; n < in.size(); ++n) {
            mid[n] = 0.5f * (out.left[n] + out.right[n]);
            side[n] = 0.5f * (out.left[n] - out.right[n]);
            error += (static_cast<double>(mid[n]) - plain.left[n]) * (static_cast<double>(mid[n]) - plain.left[n]);
            power += static_cast<double>(plain.left[n]) * plain.left[n];
        }
        INFO("width " << width);
        const double midError = 10.0 * std::log10(std::max(error, 1e-30) / power);
        CHECK_MESSAGE(midError <= -60.0, "the mid differs from the width-0 output by " << midError
                                         << " dB: width must add a side signal, never filter or scale the mono signal");
        const double lowSeparation = spec::toDb(spec::rms(spec::lowpass(mid, 60.0, kRate, 4), 9600) /
                                                std::max(spec::rms(spec::lowpass(side, 60.0, kRate, 4), 9600), 1e-30));
        CHECK_MESSAGE(lowSeparation >= 30.0, "below 60 Hz the side is only " << lowSeparation
                                             << " dB under the mid: high-pass the side signal steeply at mono_freq");
    }
}
