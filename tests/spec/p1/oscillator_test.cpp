// Phase 1 spec tests: the band-limited oscillator and the sub's sine (docs/specs/phase-1-voice.md, P1.01 to P1.04).
// spec-needs: core/include/undertow/oscillator.h

#include "doctest.h"

#include "spec_analysis.h"
#include "undertow/oscillator.h"

#include <cmath>
#include <string>

namespace {

constexpr double kRate = 48000.0;
constexpr std::size_t kSize = 1u << 16;

spec::Signal run(undertow::Oscillator& osc, std::size_t samples) {
    spec::Signal out(samples);
    for (auto& v : out)
        v = osc.process();
    return out;
}

spec::Signal tone(undertow::Wave wave, double hz, double width = 0.5) {
    undertow::Oscillator osc;
    osc.prepare(kRate);
    osc.setWave(wave);
    osc.setPulseWidth(width);
    osc.setFrequency(hz);
    osc.reset(0.0);
    run(osc, 1000);
    return run(osc, kSize);
}

bool near(double value, double want, double tolerance) { return std::fabs(value - want) <= tolerance; }

double harmonicDb(const spec::Spectrum& s, double f0, int harmonic) {
    return spec::toDb(spec::amplitudeAt(s, f0 * harmonic) / spec::amplitudeAt(s, f0));
}

} // namespace

TEST_CASE("P1.01 the oscillators play the frequency they are given") {
    for (double hz : {55.0, 110.0, 1234.5}) {
        for (auto wave : {undertow::Wave::Saw, undertow::Wave::Square}) {
            const auto s = spec::spectrum(tone(wave, hz), 0, kSize, kRate);
            const double measured = spec::peakHz(s, hz * 0.7, hz * 1.3);
            CHECK_MESSAGE(near(measured, hz, 0.002 * hz),
                          std::string(wave == undertow::Wave::Saw ? "saw" : "square")
                              << " set to " << hz << " Hz plays " << measured
                              << " Hz: the phase must advance by frequency / sampleRate per sample");
        }
    }
    undertow::SineOscillator sine;
    sine.prepare(kRate);
    sine.setFrequency(55.0);
    spec::Signal out(48000);
    for (auto& v : out)
        v = sine.process();
    const double measured = spec::zeroCrossingHz(out, 0, out.size(), kRate);
    CHECK_MESSAGE(near(measured, 55.0, 0.02), "the sine set to 55 Hz plays " << measured << " Hz");
}

TEST_CASE("P1.02 the saw and the square are band-limited (PolyBLEP), with a saw's and a square's harmonics") {
    const double f0 = 1234.5;
    for (auto wave : {undertow::Wave::Saw, undertow::Wave::Square}) {
        const std::string name = wave == undertow::Wave::Saw ? "saw" : "square";
        const auto s = spec::spectrum(tone(wave, f0), 0, kSize, kRate);
        const double fundamental = spec::amplitudeAt(s, f0);
        const double alias = spec::powerAwayFromHarmonics(s, f0, 20.0, 4000.0, 6.0);
        const double aliasDb = 10.0 * std::log10(std::max(alias, 1e-30) / (fundamental * fundamental));
        CHECK_MESSAGE(aliasDb <= -50.0, name << " at " << f0 << " Hz: the energy between its harmonics below 4 kHz is "
                                             << aliasDb << " dB under the fundamental; it must be -50 dB or less. A naive "
                                                           "waveform aliases at about -20 dB: correct each discontinuity "
                                                           "with a PolyBLEP residual");
    }
    const auto saw = spec::spectrum(tone(undertow::Wave::Saw, 220.0), 0, kSize, kRate);
    CHECK_MESSAGE(near(harmonicDb(saw, 220.0, 2), -6.02, 1.0),
                  "a saw's 2nd harmonic is half its fundamental (-6 dB); measured " << harmonicDb(saw, 220.0, 2) << " dB");
    CHECK_MESSAGE(near(harmonicDb(saw, 220.0, 3), -9.54, 1.0),
                  "a saw's 3rd harmonic is a third of its fundamental (-9.5 dB); measured " << harmonicDb(saw, 220.0, 3) << " dB");
}

TEST_CASE("P1.03 the square's pulse width shapes its harmonics and its mean") {
    const auto even = tone(undertow::Wave::Square, 220.0, 0.5);
    const auto s = spec::spectrum(even, 0, kSize, kRate);
    CHECK_MESSAGE(harmonicDb(s, 220.0, 2) <= -40.0,
                  "a 50% square has no even harmonics; its 2nd harmonic is " << harmonicDb(s, 220.0, 2) << " dB");
    CHECK_MESSAGE(near(harmonicDb(s, 220.0, 3), -9.54, 1.0),
                  "a 50% square's 3rd harmonic is a third of its fundamental; measured " << harmonicDb(s, 220.0, 3) << " dB");
    CHECK_MESSAGE(std::fabs(spec::mean(even)) < 0.01, "a 50% square has no DC; its mean is " << spec::mean(even));

    const auto narrow = tone(undertow::Wave::Square, 220.0, 0.25);
    const auto n = spec::spectrum(narrow, 0, kSize, kRate);
    CHECK_MESSAGE(harmonicDb(n, 220.0, 2) >= -6.0,
                  "a 25% pulse has a strong 2nd harmonic (-3 dB); measured " << harmonicDb(n, 220.0, 2)
                                                                             << " dB: setPulseWidth must move the falling edge");
    CHECK_MESSAGE(near(spec::mean(narrow), -0.5, 0.02),
                  "a 25% pulse is +1 for a quarter of the cycle, so its mean is -0.5; measured " << spec::mean(narrow));
}

TEST_CASE("P1.04 the sine is pure, full scale, and starts at the phase it is reset to") {
    undertow::SineOscillator sine;
    sine.prepare(kRate);
    sine.setFrequency(55.0);
    sine.reset(0.0);
    CHECK_MESSAGE(near(sine.phase(), 0.0, 1e-12), "reset(0) puts the phase at 0");
    const float first = sine.process();
    const float second = sine.process();
    CHECK_MESSAGE(std::fabs(first) < 1e-6f, "after reset(0) the first sample is sin(0) = 0; got " << first);
    CHECK_MESSAGE(near(second, std::sin(2.0 * 3.141592653589793 * 55.0 / kRate), 1e-6),
                  "the second sample is sin(2 pi 55 / 48000); got " << second);
    sine.reset(0.25);
    CHECK_MESSAGE(near(sine.process(), 1.0, 1e-6), "after reset(0.25) the first sample is 1");

    spec::Signal out(kSize);
    sine.reset(0.0);
    for (auto& v : out)
        v = sine.process();
    CHECK_MESSAGE(near(spec::peakAbs(out), 1.0, 1e-3), "the sine's peak is " << spec::peakAbs(out));
    const double distortion = spec::thd(spec::spectrum(out, 0, kSize, kRate), 55.0, 10);
    CHECK_MESSAGE(spec::toDb(distortion) <= -80.0,
                  "the sub's sine must be pure: harmonics 2 to 10 at " << spec::toDb(distortion) << " dB, want -80 dB or less");
}
