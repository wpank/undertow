// SPDX-License-Identifier: AGPL-3.0-only
// Phase 1 spec tests: the engine skeleton and the beat clock (docs/specs/phase-1-foundation.md, checks P1.03 to
// P1.07).
// spec-needs: core/include/undertow/engine.h

#include "doctest.h"

#include "spec_alloc.h"
#include "spec_engine.h"
#include "spec_paths.h"
#include "undertow/engine.h"
#include "undertow/params.h"

#include <cmath>
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

TEST_CASE("P1.03 parameters start at their defaults; setParam clamps, rounds and ignores bad indices") {
    undertow::Engine engine;
    engine.prepare(kRate, kMaxBlock);
    const auto idle = engine.meters();
    CHECK_MESSAGE(idle.note == -1, "a new engine plays no note: meters().note must be -1; got " << idle.note);
    CHECK_MESSAGE(idle.noteHz == 0.0f, "a new engine plays no note: meters().noteHz must be 0; got " << idle.noteHz);
    CHECK_MESSAGE(idle.step == -1, "a new engine plays no step: meters().step must be -1; got " << idle.step);
    CHECK_MESSAGE(idle.peak == 0.0f, "a new engine has made no sound: meters().peak must be 0; got " << idle.peak);
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
        if (spec.kind == undertow::ParamKind::Float) {
            const float middle = 0.5f * (spec.min + spec.max) + 0.123f;
            engine.setParam(i, middle);
            CHECK_MESSAGE(engine.getParam(i) == doctest::Approx(middle).epsilon(1e-6),
                          "a Float parameter keeps the value it is given inside its range");
        }
        engine.setParam(i, spec.def);
    }
    engine.setParam(-1, 1.0f);
    engine.setParam(undertow::kNumParams, 1.0f);
    CHECK_MESSAGE(engine.getParam(-1) == 0.0f, "getParam of an index out of range returns 0");
    CHECK_MESSAGE(engine.getParam(undertow::kNumParams) == 0.0f, "getParam of an index out of range returns 0");
}

TEST_CASE("P1.04 process writes finite samples, the same on both channels, for any block size, and an empty block "
          "changes nothing") {
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
            events.push_back(midi(0, 0x90, 33, 100));
            events.push_back(midi(size / 2, 0xB0, 1, 64));
            events.push_back(midi(size - 1, 0x80, 33, 0));
        }
        const undertow::Transport transport{true, true, 130.0, 4.0};
        int nonFinite = 0, different = 0;
        for (int pass = 0; pass < 3; ++pass) {
            engine.process(left.data(), right.data(), size, events.data(), static_cast<int>(events.size()), transport);
            for (int n = 0; n < size; ++n) {
                const float l = left[static_cast<std::size_t>(n)], r = right[static_cast<std::size_t>(n)];
                nonFinite += (std::isfinite(l) ? 0 : 1) + (std::isfinite(r) ? 0 : 1);
                different += l == r ? 0 : 1;
            }
        }
        CHECK_MESSAGE(nonFinite == 0, nonFinite << " output samples were left non-finite (the buffers start as NaN): "
                                                   "process must write every sample of both channels");
        CHECK_MESSAGE(different == 0, different << " samples differ between the channels: the instrument is mono, so "
                                                   "process writes the same value to left and right");
        CHECK_MESSAGE(left[static_cast<std::size_t>(size)] == 7.0f, "process wrote past numSamples");
        CHECK_MESSAGE(right[static_cast<std::size_t>(size)] == 7.0f, "process wrote past numSamples");
    }

    // Two engines play the same 2 s of the rumble; one of them also gets empty blocks between its real ones.
    undertow::Engine plain, interrupted;
    for (auto* e : {&plain, &interrupted}) {
        e->prepare(kRate, kMaxBlock);
        spec::apply(*e, spec::rumblePatch());
        e->reset();
    }
    std::vector<float> l1(256), r1(256), l2(256), r2(256);
    undertow::Transport transport{true, true, 130.0, 0.0};
    int mismatched = 0;
    for (int block = 0; block < 375; ++block) {
        plain.process(l1.data(), r1.data(), 256, nullptr, 0, transport);
        interrupted.process(l2.data(), r2.data(), 256, nullptr, 0, transport);
        const auto before = interrupted.meters();
        interrupted.process(l2.data(), r2.data(), 0, nullptr, 0, transport);
        const auto after = interrupted.meters();
        if (block == 200) {
            CHECK_MESSAGE(after.peak == before.peak, "an empty block changed meters().peak from " << before.peak << " to "
                                                                                                  << after.peak);
            CHECK_MESSAGE(after.rms == before.rms, "an empty block changed meters().rms from " << before.rms << " to "
                                                                                               << after.rms);
            CHECK_MESSAGE(after.ppq == before.ppq, "an empty block moved the clock from " << before.ppq << " to "
                                                                                          << after.ppq);
        }
        for (int n = 0; n < 256; ++n)
            mismatched += l1[static_cast<std::size_t>(n)] == l2[static_cast<std::size_t>(n)] ? 0 : 1;
        transport.ppq += 256.0 / kRate * 130.0 / 60.0;
    }
    CHECK_MESSAGE(mismatched == 0, mismatched << " samples changed when empty blocks were processed in between: a "
                                                 "block of 0 samples must change nothing");
}

TEST_CASE("P1.05 process, setParam, getParam, meters and reset never allocate") {
    undertow::Engine engine;
    engine.prepare(kRate, kMaxBlock);
    std::vector<float> left(kMaxBlock), right(kMaxBlock), scope(undertow::kScopeSamples);
    std::vector<undertow::MidiEvent> events = {
        midi(0, 0x90, 40, 90),   midi(10, 0x90, 47, 120), midi(100, 0xB0, 1, 80),
        midi(200, 0x80, 47, 0),  midi(255, 0xB0, 123, 0),
    };
    double ppq = 0;
    spec::AllocationCounter counter;
    for (int block = 0; block < 1000; ++block) {
        for (int i = 0; i < undertow::kNumParams; ++i) {
            const auto& spec = undertow::paramSpec(i);
            engine.setParam(i, spec.min + (spec.max - spec.min) * static_cast<float>((block * 7 + i) % 11) / 10.0f);
            (void)engine.getParam(i);
        }
        const undertow::Transport transport{block % 3 != 0, block % 5 != 0, 130.0, ppq};
        engine.process(left.data(), right.data(), 256, events.data(), static_cast<int>(events.size()), transport);
        ppq += 256.0 / kRate * 130.0 / 60.0;
        (void)engine.meters();
        (void)engine.copyScope(scope.data(), static_cast<int>(scope.size()));
        if (block % 100 == 99)
            engine.reset();
    }
    const std::size_t allocations = counter.count(); // read before CHECK: doctest's first assertion allocates
    CHECK_MESSAGE(allocations == 0, allocations << " heap allocations happened inside process/setParam/getParam/"
                                                   "meters/copyScope/reset: allocate everything in prepare() and use "
                                                   "fixed-size storage");
}

TEST_CASE("P1.06 the beat clock follows the host transport, and runs on its own at seq_bpm without one") {
    undertow::Engine engine;
    engine.prepare(kRate, kMaxBlock);
    std::vector<float> left(kMaxBlock), right(kMaxBlock);

    engine.process(left.data(), right.data(), 480, nullptr, 0, undertow::Transport{true, true, 120.0, 8.0});
    auto m = engine.meters();
    CHECK_MESSAGE(m.ppq == doctest::Approx(8.02).epsilon(1e-9),
                  "with a playing host transport (120 BPM, ppq 8 at the block start), meters().ppq after a "
                  "480-sample block at 48 kHz must be 8.02 (the position at the block's end); got "
                      << m.ppq);
    CHECK_MESSAGE(m.hostClock, "with a valid host transport, meters().hostClock must be true");
    CHECK_MESSAGE(m.bpm == doctest::Approx(120.0), "meters().bpm must be the host's tempo, 120; got " << m.bpm);

    engine.process(left.data(), right.data(), 480, nullptr, 0, undertow::Transport{true, false, 120.0, 3.5});
    m = engine.meters();
    CHECK_MESSAGE(m.ppq == doctest::Approx(3.5).epsilon(1e-9),
                  "with a stopped host transport, meters().ppq must stay at the host's position (3.5); got " << m.ppq);

    // Free mode: with no host the own clock always runs in Free; in Key and Notes it runs only while a key is held
    // (phase 7.1).
    engine.setParam(undertow::param::SeqMode, 1.0f);
    engine.setParam(undertow::param::SeqBpm, 120.0f);
    engine.reset();
    CHECK_MESSAGE(engine.meters().ppq == doctest::Approx(0.0), "reset() must put the engine's own clock at 0");
    for (int block = 0; block < 100; ++block)
        engine.process(left.data(), right.data(), 480, nullptr, 0, undertow::Transport{});
    m = engine.meters();
    CHECK_MESSAGE(m.ppq == doctest::Approx(2.0).epsilon(1e-6),
                  "in Free mode without a host transport (Transport::valid false, as in the Standalone app) the "
                  "clock runs by itself at seq_bpm from 0 after reset(): 1 s at 120 BPM is 2 beats; got "
                      << m.ppq);
    CHECK_MESSAGE(!m.hostClock, "without a host transport, meters().hostClock must be false");
    CHECK_MESSAGE(m.bpm == doctest::Approx(120.0), "without a host, meters().bpm must be seq_bpm, 120; got " << m.bpm);

    engine.setParam(undertow::param::SeqBpm, 130.0f);
    engine.reset();
    for (int block = 0; block < 100; ++block)
        engine.process(left.data(), right.data(), 480, nullptr, 0, undertow::Transport{});
    CHECK_MESSAGE(engine.meters().ppq == doctest::Approx(130.0 / 60.0).epsilon(1e-6),
                  "1 s at seq_bpm 130 is " << 130.0 / 60.0 << " beats; got " << engine.meters().ppq);
}

TEST_CASE("P1.07 the core builds without JUCE, and the retired oscillator, filter, envelope and LFO files are gone") {
#if defined(JUCE_CORE_H_INCLUDED) || defined(JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED) || defined(JUCE_API)
    FAIL("a core header pulled in JUCE: core/ must be plain C++17 with no JUCE include");
#endif
    const std::string source = spec::sourceDir();  // this checkout (spec_paths.h)
    for (const char* retired : {"core/include/undertow/oscillator.h", "core/src/oscillator.cpp",
                                "core/include/undertow/filter.h", "core/src/filter.cpp",
                                "core/include/undertow/envelope.h", "core/src/envelope.cpp",
                                "core/include/undertow/lfo.h", "core/src/lfo.cpp"}) {
        std::ifstream file(source + "/" + retired);
        CHECK_MESSAGE(!file.good(), std::string(retired) << " is still there: phase 1 retires it (delete the file; "
                                                            "phase 2 builds vco.h and ladder.h, phase 3 decay.h)");
    }

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
