// Phase 0 spec tests: the engine skeleton and the beat clock (docs/specs/phase-0-foundation.md,
// checks P0.03 to P0.07).
// spec-needs: core/include/undertow/engine.h

#include "doctest.h"

#include "spec_alloc.h"
#include "spec_paths.h"
#include "undertow/engine.h"
#include "undertow/params.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace {

constexpr double kRate = 48000.0;
constexpr int kMaxBlock = 512;

undertow::MidiEvent midi(int offset, int status, int data1, int data2) {
    return {offset, static_cast<std::uint8_t>(status), static_cast<std::uint8_t>(data1),
            static_cast<std::uint8_t>(data2)};
}

} // namespace

TEST_CASE("P0.03 parameters start at their defaults; setParam clamps, rounds and ignores bad indices") {
    undertow::Engine engine;
    engine.prepare(kRate, kMaxBlock);
    for (int i = 0; i < undertow::kNumParams; ++i) {
        const auto& spec = undertow::paramSpec(i);
        INFO("parameter " << i << " `" << std::string(spec.id) << "`");
        CHECK_MESSAGE(engine.getParam(i) == doctest::Approx(spec.def),
                      "a new Engine must hold every parameter at its default");
        engine.setParam(i, spec.max + 1000.0f);
        CHECK_MESSAGE(engine.getParam(i) == doctest::Approx(spec.max), "setParam above max must clamp to max");
        engine.setParam(i, spec.min - 1000.0f);
        CHECK_MESSAGE(engine.getParam(i) == doctest::Approx(spec.min), "setParam below min must clamp to min");
        if (spec.kind == undertow::ParamKind::Int || spec.kind == undertow::ParamKind::Choice) {
            engine.setParam(i, spec.min + 0.6f);
            CHECK_MESSAGE(engine.getParam(i) == doctest::Approx(spec.min + 1.0f),
                          "Int and Choice parameters round to the nearest whole value");
            engine.setParam(i, spec.min + 0.4f);
            CHECK_MESSAGE(engine.getParam(i) == doctest::Approx(spec.min),
                          "Int and Choice parameters round to the nearest whole value");
        }
        if (spec.kind == undertow::ParamKind::Bool) {
            engine.setParam(i, 0.7f);
            CHECK_MESSAGE(engine.getParam(i) == doctest::Approx(1.0f), "a Bool parameter is 1 from 0.5 up");
            engine.setParam(i, 0.3f);
            CHECK_MESSAGE(engine.getParam(i) == doctest::Approx(0.0f), "a Bool parameter is 0 below 0.5");
        }
        engine.setParam(i, spec.def);
    }
    engine.setParam(-1, 1.0f);
    engine.setParam(undertow::kNumParams, 1.0f);
    CHECK_MESSAGE(engine.getParam(-1) == 0.0f, "getParam of an index out of range returns 0");
    CHECK_MESSAGE(engine.getParam(undertow::kNumParams) == 0.0f, "getParam of an index out of range returns 0");
}

TEST_CASE("P0.04 process writes finite samples to both channels for any block size, with or without MIDI") {
    undertow::Engine engine;
    engine.prepare(kRate, kMaxBlock);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    for (int size : {0, 1, 7, 64, 333, 512, 1536}) {
        INFO("block of " << size << " samples (prepared for at most " << kMaxBlock << ")");
        std::vector<float> left(static_cast<std::size_t>(size) + 1, nan), right(static_cast<std::size_t>(size) + 1, nan);
        left[static_cast<std::size_t>(size)] = 7.0f;
        right[static_cast<std::size_t>(size)] = 7.0f;
        std::vector<undertow::MidiEvent> events;
        if (size > 0) {
            events.push_back(midi(0, 0x90, 45, 100));
            events.push_back(midi(size / 2, 0xE0, 0, 96));
            events.push_back(midi(size - 1, 0x80, 45, 0));
        }
        const undertow::Transport transport{true, true, 128.0, 4.0};
        int nonFinite = 0;
        for (int pass = 0; pass < 3; ++pass) {
            engine.process(left.data(), right.data(), size, events.data(), static_cast<int>(events.size()), transport);
            for (int n = 0; n < size; ++n)
                nonFinite += (std::isfinite(left[static_cast<std::size_t>(n)]) ? 0 : 1) +
                             (std::isfinite(right[static_cast<std::size_t>(n)]) ? 0 : 1);
        }
        CHECK_MESSAGE(nonFinite == 0, nonFinite << " output samples were left non-finite (the buffers start as NaN): "
                                                   "process must write every sample of both channels");
        CHECK_MESSAGE(left[static_cast<std::size_t>(size)] == 7.0f, "process wrote past numSamples");
        CHECK_MESSAGE(right[static_cast<std::size_t>(size)] == 7.0f, "process wrote past numSamples");
    }
}

TEST_CASE("P0.05 process, setParam, getParam, meters and reset never allocate") {
    undertow::Engine engine;
    engine.prepare(kRate, kMaxBlock);
    std::vector<float> left(kMaxBlock), right(kMaxBlock);
    std::vector<undertow::MidiEvent> events = {
        midi(0, 0x90, 40, 90),  midi(10, 0x90, 47, 120), midi(100, 0xE0, 0, 80),
        midi(200, 0x80, 47, 0), midi(255, 0xB0, 123, 0),
    };
    double ppq = 0;
    spec::AllocationCounter counter;
    for (int block = 0; block < 1000; ++block) {
        for (int i = 0; i < undertow::kNumParams; ++i) {
            const auto& spec = undertow::paramSpec(i);
            engine.setParam(i, spec.min + (spec.max - spec.min) * static_cast<float>((block * 7 + i) % 11) / 10.0f);
            (void)engine.getParam(i);
        }
        const undertow::Transport transport{block % 3 != 0, block % 5 != 0, 128.0, ppq};
        engine.process(left.data(), right.data(), 256, events.data(), static_cast<int>(events.size()), transport);
        ppq += 256.0 / kRate * 128.0 / 60.0;
        (void)engine.meters();
        if (block % 100 == 99)
            engine.reset();
    }
    const std::size_t allocations = counter.count(); // read before CHECK: doctest's first assertion allocates
    CHECK_MESSAGE(allocations == 0, allocations << " heap allocations happened inside process/setParam/getParam/meters/"
                                                   "reset: allocate everything in prepare() and use fixed-size storage");
}

TEST_CASE("P0.06 the beat clock follows the host transport, and runs on its own at seq_bpm without one") {
    undertow::Engine engine;
    engine.prepare(kRate, kMaxBlock);
    std::vector<float> left(kMaxBlock), right(kMaxBlock);

    engine.process(left.data(), right.data(), 480, nullptr, 0, undertow::Transport{true, true, 120.0, 8.0});
    CHECK_MESSAGE(engine.meters().ppq == doctest::Approx(8.02).epsilon(1e-9),
                  "with a playing host transport (120 BPM, ppq 8 at the block start), meters().ppq after a "
                  "480-sample block at 48 kHz must be 8.02 (the position at the block's end); got "
                      << engine.meters().ppq);

    engine.process(left.data(), right.data(), 480, nullptr, 0, undertow::Transport{true, false, 120.0, 3.5});
    CHECK_MESSAGE(engine.meters().ppq == doctest::Approx(3.5).epsilon(1e-9),
                  "with a stopped host transport, meters().ppq must stay at the host's position (3.5); got "
                      << engine.meters().ppq);

    engine.setParam(undertow::findParam("seq_bpm"), 120.0f);
    engine.reset();
    CHECK_MESSAGE(engine.meters().ppq == doctest::Approx(0.0), "reset() must put the internal clock at 0");
    for (int block = 0; block < 100; ++block)
        engine.process(left.data(), right.data(), 480, nullptr, 0, undertow::Transport{});
    CHECK_MESSAGE(engine.meters().ppq == doctest::Approx(2.0).epsilon(1e-6),
                  "without a host transport (Transport::valid false, as in the Standalone app) the clock runs "
                  "by itself at seq_bpm from 0 after reset(): 1 s at 120 BPM is 2 beats; got "
                      << engine.meters().ppq);
}

TEST_CASE("P0.07 the core builds without JUCE") {
#if defined(JUCE_CORE_H_INCLUDED) || defined(JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED) || defined(JUCE_API)
    FAIL("a core header pulled in JUCE: core/ must be plain C++17 with no JUCE include");
#endif
    const std::string build = spec::binaryDir();  // this checkout's build tree (spec_paths.h)
    std::ifstream cache(build + "/CMakeCache.txt");
    REQUIRE_MESSAGE(cache.good(), "no CMakeCache.txt in " << build);
    std::string line;
    bool pluginOff = false, pluginOn = false;
    while (std::getline(cache, line)) {
        if (line.rfind("UNDERTOW_BUILD_PLUGIN:", 0) == 0) {
            pluginOff = line.find("=OFF") != std::string::npos;
            pluginOn = line.find("=ON") != std::string::npos;
        }
    }
    REQUIRE_MESSAGE((pluginOff || pluginOn), "the CMake cache has no UNDERTOW_BUILD_PLUGIN option: the top-level "
                                             "CMakeLists.txt must define option(UNDERTOW_BUILD_PLUGIN ...), OFF in "
                                             "the core preset and ON in the plugin preset");
    if (!pluginOff)
        return; // the plugin preset: JUCE is expected there
    // The files the latest configure read (the Makefile generator lists them in CMAKE_MAKEFILE_DEPENDS). Unlike the
    // cache, this list is rewritten by every configure, so it shows what the build system does now.
    std::ifstream depends(build + "/CMakeFiles/Makefile.cmake");
    REQUIRE_MESSAGE(depends.good(), "no CMakeFiles/Makefile.cmake in " << build
                                                                       << ": use the Unix Makefiles generator");
    bool inList = false;
    std::vector<std::string> juce;
    while (std::getline(depends, line)) {
        if (line.find("set(CMAKE_MAKEFILE_DEPENDS") != std::string::npos)
            inList = true;
        else if (inList && line.find(')') != std::string::npos)
            inList = false;
        else if (inList && (line.find("Juce.cmake") != std::string::npos || line.find("JUCE") != std::string::npos))
            juce.push_back(line);
    }
    CHECK_MESSAGE(juce.empty(), "the core preset (UNDERTOW_BUILD_PLUGIN=OFF) read JUCE's CMake files ("
                                    << (juce.empty() ? std::string() : juce.front())
                                    << "): include cmake/Juce.cmake only when UNDERTOW_BUILD_PLUGIN is ON");
}
