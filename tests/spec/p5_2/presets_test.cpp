// Phase 5.2 spec tests: the presets as named. SUB Pure Foundation is the clean sine sub alone, every SUB preset is
// the sub, no preset spends its energy below 20 Hz, and RUMBLE Sidechain Fog keeps what it is named for
// (docs/specs/phase-5.2-presets.md, P5.2.01 to P5.2.04).
// spec-needs: core/include/undertow/presets.h

#include "doctest.h"

#include "spec_analysis.h"
#include "spec_engine.h"
#include "undertow/engine.h"
#include "undertow/presets.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr double kRate = 48000.0;

// A preset rendered as P5.09 renders it: 2 bars at 128 BPM from beat 0, holding A2 (45) at velocity 100 when its
// sequencer is off.
spec::Stereo renderPreset(int index) {
    spec::Params values{};
    undertow::Pattern pattern;
    REQUIRE(undertow::factoryPreset(index, values.data(), &pattern));
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

const spec::Stereo& cachedRender(int index) {
    static std::map<int, spec::Stereo> renders;
    auto found = renders.find(index);
    if (found == renders.end())
        found = renders.emplace(index, renderPreset(index)).first;
    return found->second;
}

// The share of a render's energy in [loHz, hiHz], as P5.03 measures it.
double share(const spec::Signal& x, double loHz, double hiHz) {
    const auto s = spec::spectrum(x, 0, 1u << 17, kRate);
    return spec::bandPower(s, loHz, hiHz) / std::max(spec::bandPower(s, 0.0, 24000.0), 1e-30);
}

} // namespace

TEST_CASE("P5.2.01 SUB Pure Foundation is the clean sine sub alone: its energy at 55 Hz, nothing two octaves down, and "
          "its harmonics 40 dB under") {
    const int index = undertow::findFactoryPreset("SUB Pure Foundation");
    REQUIRE_MESSAGE(index >= 0, "there must be a preset named SUB Pure Foundation");
    const auto& out = cachedRender(index);
    const double fundamental = share(out.left, 50.0, 60.0);
    CHECK_MESSAGE(fundamental >= 0.98, "holding A2, SUB Pure Foundation puts " << 100.0 * fundamental
                                                                               << "% of its energy at 50 to 60 Hz, the -1 oct sine at 55 Hz; "
                                                                                  "the clean sine sub alone puts at least 98% there. Turn the "
                                                                                  "main oscillator off (osc_level 0): its saw two octaves down "
                                                                                  "adds 27.5 Hz and harmonics to 440 Hz");
    // The held tone after its first half second: the sine, anything an octave under it, and its harmonics.
    const auto held = spec::spectrum(out.left, 24000, 1u << 17, kRate);
    const double sub = spec::amplitudeAt(held, 55.0);
    REQUIRE_MESSAGE(sub > 0.0, "SUB Pure Foundation holding A2 must sound its 55 Hz sine");
    const double under = spec::toDb(spec::amplitudeAt(held, 27.5) / sub);
    CHECK_MESSAGE(under <= -60.0, "27.5 Hz, two octaves under A2, sits " << under
                                                                         << " dB re the 55 Hz sine; nothing sounds there in the clean sine "
                                                                            "sub alone (at least 60 dB under). An oscillator two octaves down "
                                                                            "is still on");
    const double harmonics = spec::toDb(spec::thd(held, 55.0, 10));
    CHECK_MESSAGE(harmonics <= -40.0, "the 55 Hz sine's harmonics 2 to 10 sum to " << harmonics
                                                                                  << " dB re the sine; a clean sine keeps them 40 dB under (docs/PRD.md "
                                                                                     "1.1). The output saturation adds them: keep sat_drive at 0.1 or "
                                                                                     "less for this preset");
}

TEST_CASE("P5.2.02 every SUB preset is the sub: at least 95% of its energy below 80 Hz") {
    int found = 0;
    for (int i = 0; i < undertow::numFactoryPresets(); ++i) {
        const std::string name = undertow::factoryPresetName(i);
        if (name.rfind("SUB ", 0) != 0)
            continue;
        ++found;
        const auto& out = cachedRender(i);
        const double below = share(out.left, 0.0, 80.0);
        CHECK_MESSAGE(below >= 0.95, name << " keeps " << 100.0 * below
                                          << "% of its energy below 80 Hz; a SUB preset is the sub (docs/PRD.md 1.1: a clean "
                                             "sine foundation), so at least 95%. P5.03's 80% lets a full saw two octaves down through");
    }
    CHECK_MESSAGE(found >= 3, "the factory presets hold three SUB presets; found " << found);
}

TEST_CASE("P5.2.03 no preset spends its energy below 20 Hz: at most 2% of it there") {
    for (int i = 0; i < undertow::numFactoryPresets(); ++i) {
        const std::string name = undertow::factoryPresetName(i);
        const auto& out = cachedRender(i);
        const double infrasonic = share(out.left, 0.0, 20.0);
        CHECK_MESSAGE(infrasonic <= 0.02, name << " puts " << 100.0 * infrasonic
                                              << "% of its energy below 20 Hz, where no one hears it and it only takes headroom; at most "
                                                 "2%. Keep the preset's character and move its lowest notes up (seq_root), or lower its "
                                                 "sub, its send or its rumble tone. A RUMBLE preset keeps its decay and duck (P5.2.04)");
    }
}

TEST_CASE("P5.2.04 RUMBLE Sidechain Fog keeps its sidechain and its fog: a deep duck, the voice sent in, a long tail, "
          "and its sequence") {
    const int index = undertow::findFactoryPreset("RUMBLE Sidechain Fog");
    REQUIRE_MESSAGE(index >= 0, "there must be a preset named RUMBLE Sidechain Fog");
    spec::Params values{};
    undertow::Pattern pattern;
    REQUIRE(undertow::factoryPreset(index, values.data(), &pattern));
    auto value = [&](const char* id) { return values[static_cast<std::size_t>(spec::id(id))]; };
    // Phase 5's table: "full duck, long decay, the voice sent into the rumble". Floors as P5.03 pins rumble_level.
    CHECK_MESSAGE(value("rumble_duck") >= 0.8f, "Sidechain Fog's duck is " << value("rumble_duck")
                                                                         << "; its sidechain is a deep duck: rumble_duck 0.8 or more");
    CHECK_MESSAGE(value("rumble_send") >= 0.15f, "Sidechain Fog's send is " << value("rumble_send")
                                                                          << "; its fog is the voice sent into the rumble: rumble_send 0.15 "
                                                                             "or more");
    CHECK_MESSAGE(value("rumble_decay") >= 4.0f, "Sidechain Fog's decay is " << value("rumble_decay")
                                                                           << " s; its fog is a long tail: rumble_decay 4 s or more");
    CHECK_MESSAGE(value("seq_on") >= 0.5f, "Sidechain Fog plays its sequence: seq_on 1");
}
