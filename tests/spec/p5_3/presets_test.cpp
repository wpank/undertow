// Phase 5.3 spec tests: SUB Low Pressure as named, "sub at -2 oct with a little saturation, a slow swell", and apart
// from SUB Pure Foundation (docs/specs/phase-5.3-presets.md, P5.3.01 to P5.3.04). Each reads a preset's voice alone:
// the preset as P5.09 renders it, holding A2 at velocity 100, with its rumble layer muted, since a rumble lands on the
// sub's harmonics and would read as saturation.
// spec-needs: core/include/undertow/presets.h

#include "doctest.h"

#include "spec_analysis.h"
#include "spec_engine.h"
#include "undertow/engine.h"
#include "undertow/presets.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {

constexpr double kRate = 48000.0;

// A preset's voice: 2 bars at 128 BPM from beat 0, A2 (45) held at velocity 100 when its sequencer is off,
// rumble_level 0.
spec::Stereo voiceOf(const char* name) {
    const int index = undertow::findFactoryPreset(name);
    REQUIRE_MESSAGE(index >= 0, "there must be a preset named " << std::string(name));
    spec::Params values{};
    undertow::Pattern pattern;
    REQUIRE(undertow::factoryPreset(index, values.data(), &pattern));
    values[static_cast<std::size_t>(spec::id("rumble_level"))] = 0.0f;
    undertow::Engine engine;
    engine.prepare(kRate, 512);
    spec::apply(engine, values);
    engine.setPattern(pattern);
    engine.reset();
    std::vector<undertow::MidiEvent> events;
    if (values[static_cast<std::size_t>(spec::id("seq_on"))] < 0.5f)
        events.push_back(spec::noteOn(0, 45, 100));
    return spec::render(engine, 180000, events, undertow::Transport{true, true, 128.0, 0.0}, 512);
}

const spec::Stereo& lowPressureVoice() {
    static const spec::Stereo out = voiceOf("SUB Low Pressure");
    return out;
}

// The power-weighted mean frequency below 200 Hz of the held tone, from its first half second on.
double centroidBelow200(const spec::Signal& x) {
    const auto s = spec::spectrum(x, 24000, 1u << 17, kRate);
    double weighted = 0.0, total = 0.0;
    for (std::size_t k = 1; k < s.amplitude.size(); ++k) {
        const double hz = static_cast<double>(k) * s.binHz();
        if (hz > 200.0)
            break;
        const double power = s.amplitude[k] * s.amplitude[k];
        weighted += hz * power;
        total += power;
    }
    return total > 0.0 ? weighted / total : 0.0;
}

// The share of a render's energy in [loHz, hiHz], as P5.03 measures it.
double share(const spec::Signal& x, double loHz, double hiHz) {
    const auto s = spec::spectrum(x, 0, 1u << 17, kRate);
    return spec::bandPower(s, loHz, hiHz) / std::max(spec::bandPower(s, 0.0, 24000.0), 1e-30);
}

} // namespace

TEST_CASE("P5.3.01 SUB Low Pressure sounds its sub at -2 oct: holding A2, at least 97% of its voice's energy at 27.5 Hz") {
    const auto& voice = lowPressureVoice();
    const double atSub = share(voice.left, 25.0, 30.0);
    CHECK_MESSAGE(atSub >= 0.97, "holding A2, SUB Low Pressure's voice puts " << 100.0 * atSub
                                                                            << "% of its energy at 25 to 30 Hz; \"sub at -2 oct\" sounds "
                                                                               "27.5 Hz, so at least 97% there (sub_octave -2 oct). At "
                                                                               "-1 oct it is the 55 Hz sine of SUB Pure Foundation");
}

TEST_CASE("P5.3.02 SUB Low Pressure has a little saturation: its 3rd harmonic 25 to 45 dB under its fundamental") {
    const auto& voice = lowPressureVoice();
    // The held tone after its first half second. The output saturation is symmetric, so it adds odd harmonics: read
    // the 3rd, against the voice's own fundamental (27.5 Hz at -2 oct, 55 Hz at -1 oct).
    const auto held = spec::spectrum(voice.left, 24000, 1u << 17, kRate);
    const double f0 = spec::peakHz(held, 20.0, 80.0);
    const double fundamental = spec::amplitudeAt(held, f0);
    REQUIRE_MESSAGE(fundamental > 0.0, "SUB Low Pressure holding A2 must sound its sub");
    const double third = spec::toDb(spec::amplitudeAt(held, 3.0 * f0) / fundamental);
    CHECK_MESSAGE(third >= -45.0, "the 3rd harmonic (" << 3.0 * f0 << " Hz) sits " << third
                                                       << " dB re the " << f0
                                                       << " Hz fundamental; \"a little saturation\" puts it within 45 dB (sat_drive about "
                                                          "0.1 or more). A clean sine leaves it near -138 dB");
    CHECK_MESSAGE(third <= -25.0, "the 3rd harmonic (" << 3.0 * f0 << " Hz) sits " << third
                                                       << " dB re the " << f0
                                                       << " Hz fundamental; a little saturation keeps it 25 dB under (sat_drive about "
                                                          "0.4 or less). Above that the sub growls: it is no longer a little");
}

TEST_CASE("P5.3.03 SUB Low Pressure swells in slowly: its first 40 ms at least 12 dB under its level at 300 ms") {
    const auto& voice = lowPressureVoice();
    spec::Signal mid(voice.left.size());
    for (std::size_t i = 0; i < mid.size(); ++i)
        mid[i] = 0.5f * (voice.left[i] + voice.right[i]);
    // 40 ms windows: one cycle of 27.5 Hz is 36 ms.
    const double onset = spec::rms(mid, 0, 1920), swelled = spec::rms(mid, 13440, 15360);
    REQUIRE_MESSAGE(swelled > 0.0, "SUB Low Pressure must sound by 300 ms");
    const double rise = spec::toDb(onset / swelled);
    CHECK_MESSAGE(rise <= -12.0, "the voice's first 40 ms sit " << rise
                                                               << " dB re its level at 280 to 320 ms; \"a slow swell\" starts at least 12 dB "
                                                                  "under (an amp attack of about 100 ms or more). A 3 ms attack starts within "
                                                                  "2 dB");
}

TEST_CASE("P5.3.04 SUB Low Pressure is not SUB Pure Foundation: their voices sit at least half an octave apart") {
    const auto& low = lowPressureVoice();
    const auto pure = voiceOf("SUB Pure Foundation");
    const double lowHz = centroidBelow200(low.left), pureHz = centroidBelow200(pure.left);
    REQUIRE_MESSAGE((lowHz > 0.0 && pureHz > 0.0), "both voices must sound holding A2");
    const double octaves = std::abs(std::log2(lowHz / pureHz));
    CHECK_MESSAGE(octaves >= 0.5, "SUB Low Pressure's voice centres at " << lowHz << " Hz and SUB Pure Foundation's at " << pureHz
                                                                       << " Hz below 200 Hz, " << octaves
                                                                       << " octaves apart; two presets named apart sound at least half "
                                                                          "an octave apart, beyond their rumble layers");
}
