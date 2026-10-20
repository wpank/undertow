// Phase 5 spec tests: the factory presets (docs/specs/phase-5-ship.md, P5.01 to P5.03 and P5.09).
// spec-needs: core/include/undertow/presets.h

#include "doctest.h"

#include "spec_analysis.h"
#include "spec_engine.h"
#include "undertow/engine.h"
#include "undertow/presets.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

constexpr double kRate = 48000.0;
const char* const kCategories[] = {"SUB ", "BASS ", "RUMBLE ", "SEQ "};

std::string categoryOf(const std::string& name) {
    for (const char* c : kCategories)
        if (name.rfind(c, 0) == 0)
            return std::string(c, std::string(c).size() - 1);
    return "";
}

// Render a preset for 2 bars at 128 BPM from beat 0, holding A2 (45) when its sequencer is off.
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

// Each preset's render, made once and shared by the test cases.
const spec::Stereo& cachedRender(int index) {
    static std::map<int, spec::Stereo> renders;
    auto found = renders.find(index);
    if (found == renders.end())
        found = renders.emplace(index, renderPreset(index)).first;
    return found->second;
}

// A render's level: the RMS of both channels together, in dBFS.
double levelDb(const spec::Stereo& out) {
    const double left = spec::rms(out.left), right = spec::rms(out.right);
    return 10.0 * std::log10(std::max(0.5 * (left * left + right * right), 1e-30));
}

double share(const spec::Signal& x, double loHz, double hiHz) {
    const auto s = spec::spectrum(x, 0, 1u << 17, kRate);
    return spec::bandPower(s, loHz, hiHz) / std::max(spec::bandPower(s, 0.0, 24000.0), 1e-30);
}

} // namespace

TEST_CASE("P5.01 there are at least 12 factory presets: unique names, four categories, valid values") {
    const int count = undertow::numFactoryPresets();
    REQUIRE_MESSAGE(count >= 12, "there must be at least 12 factory presets; there are " << count);
    std::set<std::string> names, slugs, categories;
    for (int i = 0; i < count; ++i) {
        const std::string name = undertow::factoryPresetName(i), slug = undertow::factoryPresetSlug(i);
        INFO("preset " << i << " \"" << name << "\"");
        CHECK_MESSAGE(names.insert(name).second, "preset names must be unique");
        CHECK_MESSAGE(slugs.insert(slug).second, "preset slugs must be unique");
        const std::string category = categoryOf(name);
        CHECK_MESSAGE(!category.empty(), "a preset name starts with its category: SUB, BASS, RUMBLE or SEQ, then a space");
        categories.insert(category);
        std::string expected;
        for (char c : name)
            expected += c == ' ' ? '-' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        CHECK_MESSAGE(slug == expected, "the slug is the name in lower case with spaces as dashes (\"" << expected
                                                                                                        << "\"); got \"" << slug << "\"");
        CHECK_MESSAGE(undertow::findFactoryPreset(name) == i, "findFactoryPreset finds a preset by its name");
        CHECK_MESSAGE(undertow::findFactoryPreset(slug) == i, "findFactoryPreset finds a preset by its slug");
        spec::Params values{};
        undertow::Pattern pattern;
        REQUIRE(undertow::factoryPreset(i, values.data(), &pattern));
        for (int p = 0; p < undertow::kNumParams; ++p) {
            const auto& s = undertow::paramSpec(p);
            CHECK_MESSAGE(values[static_cast<std::size_t>(p)] >= s.min, std::string(s.id) << " is under its range");
            CHECK_MESSAGE(values[static_cast<std::size_t>(p)] <= s.max, std::string(s.id) << " is over its range");
        }
    }
    CHECK_MESSAGE(categories.size() == 4, "every category (SUB, BASS, RUMBLE, SEQ) needs presets; found " << categories.size());
    for (const char* showcase : {"SUB Pure Foundation", "BASS Hypnotic Roller", "RUMBLE Dark Room"})
        CHECK_MESSAGE(undertow::findFactoryPreset(showcase) >= 0, "the showcase preset \"" << std::string(showcase) << "\" is missing");
    spec::Params scratch{};
    undertow::Pattern pattern;
    CHECK_MESSAGE(!undertow::factoryPreset(count, scratch.data(), &pattern), "factoryPreset(numFactoryPresets()) returns false");
    CHECK_MESSAGE(undertow::findFactoryPreset("no such preset") == -1, "findFactoryPreset of an unknown name is -1");
}

TEST_CASE("P5.02 every preset renders clean, in level, and the same every time") {
    for (int i = 0; i < undertow::numFactoryPresets(); ++i) {
        INFO("preset \"" << std::string(undertow::factoryPresetName(i)) << "\"");
        const auto& out = cachedRender(i);
        CHECK_MESSAGE(spec::allFinite(out.left), "the render has non-finite samples");
        const double peak = std::max(spec::peakAbs(out.left), spec::peakAbs(out.right));
        CHECK_MESSAGE(peak <= 0.966 + 1e-6, "the render peaks at " << peak << ", over -0.3 dBFS");
        const double rms = spec::toDb(spec::rms(out.left));
        CHECK_MESSAGE(rms >= -30.0, "the render's RMS is " << rms << " dBFS: a preset must be heard (-30 dBFS or louder)");
        const auto again = renderPreset(i);
        CHECK_MESSAGE(again.left == out.left, "rendering the same preset twice must give the same samples");
    }
}

TEST_CASE("P5.03 each category sounds like itself") {
    for (int i = 0; i < undertow::numFactoryPresets(); ++i) {
        const std::string name = undertow::factoryPresetName(i);
        const std::string category = categoryOf(name);
        INFO("preset \"" << name << "\"");
        spec::Params values{};
        undertow::Pattern pattern;
        REQUIRE(undertow::factoryPreset(i, values.data(), &pattern));
        const auto& out = cachedRender(i);
        if (category == "SUB")
            CHECK_MESSAGE(share(out.left, 0.0, 80.0) >= 0.8, "a SUB preset is the sub: at least 80% of its energy below 80 Hz (the "
                                                                 "held A2 itself is 110 Hz); this one "
                                                                     << 100.0 * share(out.left, 0.0, 80.0) << "%");
        if (category == "BASS")
            CHECK_MESSAGE(share(out.left, 300.0, 24000.0) >= 0.05, "a BASS preset has bite: at least 5% of its energy above 300 Hz; "
                                                                   "this one "
                                                                       << 100.0 * share(out.left, 300.0, 24000.0) << "%");
        if (category == "RUMBLE") {
            CHECK_MESSAGE(values[static_cast<std::size_t>(spec::id("rumble_level"))] >= 0.4f, "a RUMBLE preset has rumble_level 0.4 or more");
            CHECK_MESSAGE(share(out.left, 0.0, 200.0) >= 0.6, "a RUMBLE preset keeps at least 60% of its energy below 200 Hz; this one "
                                                                  << 100.0 * share(out.left, 0.0, 200.0) << "%");
        }
        if (category == "SEQ" || name == "BASS Hypnotic Roller")
            CHECK_MESSAGE(values[static_cast<std::size_t>(spec::id("seq_on"))] == 1.0f, "this preset plays its pattern: seq_on 1");
    }
}

TEST_CASE("P5.09 the presets are loudness-matched: each renders within 3 dB of -18 dBFS RMS, the init patch's level") {
    // Browsing from one preset to the next, none should sound weak or hot. -18 dBFS RMS is where the init patch
    // renders (at the -14 dB master default), and it keeps most peaks under the clipper's knee.
    for (int i = 0; i < undertow::numFactoryPresets(); ++i) {
        const std::string name = undertow::factoryPresetName(i);
        INFO("preset \"" << name << "\"");
        const double level = levelDb(cachedRender(i));
        CHECK_MESSAGE(level >= -21.0, "the render is " << level << " dBFS RMS, " << -18.0 - level
                                                       << " dB under -18: raise this preset's master_gain by that much");
        CHECK_MESSAGE(level <= -15.0, "the render is " << level << " dBFS RMS, " << level + 18.0
                                                       << " dB over -18: lower this preset's master_gain by that much");
    }
}
