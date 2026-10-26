// Phase 5.1 spec tests: the offline renderer plays each preset as the engine plays it, the note held when the
// sequencer is off and the whole pattern when it is on (docs/specs/phase-5.1-renders.md, P5.1.02 and P5.1.03).
// spec-needs: core/include/undertow/presets.h
// spec-target: undertow_render

#include "doctest.h"

#include "spec_analysis.h"
#include "spec_engine.h"
#include "spec_paths.h"
#include "spec_wav.h"
#include "undertow/presets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include <sys/wait.h>

namespace {

constexpr double kRate = 48000.0;
constexpr std::size_t kBar = 90000;  // one bar at 128 BPM and 48 kHz
constexpr int kBars = 8;             // as tools/render-demos.sh renders

struct Run {
    int status = -1;
    std::string output;
};

// Run the renderer with `args` from the repository root, as P5.05 does.
Run render(const std::string& args) {
    Run r;
    const std::string program = spec::built(SPEC_TARGET_UNDERTOW_RENDER_IN_BUILD, SPEC_TARGET_UNDERTOW_RENDER);
    const std::string command = "cd '" + spec::sourceDir() + "' && '" + program + "' " + args + " 2>&1";
    FILE* pipe = popen(command.c_str(), "r");
    REQUIRE_MESSAGE(pipe != nullptr, "cannot run the renderer");
    char line[512];
    while (fgets(line, sizeof line, pipe) != nullptr)
        r.output += line;
    const int raw = pclose(pipe);
    r.status = WIFEXITED(raw) ? WEXITSTATUS(raw) : -1;
    return r;
}

// The preset's slug, or init (index -1).
std::string slugOf(int index) { return index < 0 ? std::string("init") : std::string(undertow::factoryPresetSlug(index)); }

// The preset's values and pattern; init is the defaults and the default pattern.
void presetOf(int index, spec::Params& values, undertow::Pattern& pattern) {
    if (index < 0) {
        for (int i = 0; i < undertow::kNumParams; ++i)
            values[static_cast<std::size_t>(i)] = undertow::paramSpec(i).def;
        pattern = undertow::defaultPattern();
        return;
    }
    REQUIRE(undertow::factoryPreset(index, values.data(), &pattern));
}

bool sequenced(int index) {
    spec::Params values{};
    undertow::Pattern pattern;
    presetOf(index, values, pattern);
    return values[static_cast<std::size_t>(spec::id("seq_on"))] >= 0.5f;
}

// The engine playing the preset as docs/specs/phase-5-ship.md says the renderer does, and as P5.09 renders it: its
// values and pattern, a reset, a host transport from beat 0 at 128 BPM in blocks of 512, and A2 (45) held at
// velocity 100 from the first frame when its sequencer is off.
spec::Stereo enginePlays(int index) {
    spec::Params values{};
    undertow::Pattern pattern;
    presetOf(index, values, pattern);
    undertow::Engine engine;
    engine.prepare(kRate, 512);
    spec::apply(engine, values);
    engine.setPattern(pattern);
    engine.reset();
    std::vector<undertow::MidiEvent> events;
    if (values[static_cast<std::size_t>(spec::id("seq_on"))] < 0.5f)
        events.push_back(spec::noteOn(0, 45, 100));
    return spec::render(engine, kBar * kBars, events, undertow::Transport{true, true, 128.0, 0.0}, 512);
}

// RMS of both channels together over [from, to), in dBFS.
double levelDb(const std::vector<float>& left, const std::vector<float>& right, std::size_t from, std::size_t to) {
    double sum = 0.0;
    for (std::size_t i = from; i < to; ++i)
        sum += 0.5 * (static_cast<double>(left[i]) * left[i] + static_cast<double>(right[i]) * right[i]);
    return 10.0 * std::log10(std::max(sum / static_cast<double>(std::max<std::size_t>(1, to - from)), 1e-30));
}

// The renderer's 8 bars of the preset at 128 BPM, made once and shared by the test cases.
const spec::Wav& rendered(int index) {
    static std::map<int, spec::Wav> renders;
    auto found = renders.find(index);
    if (found == renders.end()) {
        std::system(("mkdir -p '" + spec::sourceDir() + "/build/spec-render'").c_str());
        const std::string out = "build/spec-render/p5_1-" + slugOf(index) + ".wav";
        const auto run = render("--preset " + slugOf(index) + " --bars 8 --bpm 128 --out " + out);
        REQUIRE_MESSAGE(run.status == 0, "undertow_render exited " << run.status << ": " << run.output);
        auto wav = spec::readWav(spec::sourceDir() + "/" + out);
        REQUIRE_MESSAGE(wav.ok, "the renderer's file is not a WAV I can read: " << wav.error);
        REQUIRE_MESSAGE(wav.frames == kBar * kBars, "8 bars at 128 BPM are " << kBar * kBars << " frames; the file has " << wav.frames);
        found = renders.emplace(index, std::move(wav)).first;
    }
    return found->second;
}

} // namespace

TEST_CASE("P5.1.02 undertow_render plays each preset as the engine does, the note held and the whole pattern, bar by bar "
          "within 0.5 dB") {
    for (int index = -1; index < undertow::numFactoryPresets(); ++index) {
        const auto& wav = rendered(index);
        const auto engine = enginePlays(index);
        const std::string how = sequenced(index) ? "its sequencer playing the whole pattern"
                                                 : "A2 held at velocity 100 from the first frame";
        for (int bar = 0; bar < kBars; ++bar) {
            const auto from = static_cast<std::size_t>(bar) * kBar, to = from + kBar;
            const double got = levelDb(wav.left, wav.right, from, to), want = levelDb(engine.left, engine.right, from, to);
            CHECK_MESSAGE(std::abs(got - want) <= 0.5,
                          slugOf(index) << ", bar " << bar + 1 << ": the render is " << got << " dBFS RMS; the engine playing the preset ("
                                        << how << ", a host transport from beat 0, blocks of 512) gives " << want
                                        << ". Hold --note when the sequencer is off, and process in blocks no longer than the "
                                           "prepared maximum: the engine hands back at most 64 sequencer events a block");
        }
    }
}

TEST_CASE("P5.1.03 a preset with its sequencer off renders in level: undertow_render's A2 lands within 3 dB of -18 dBFS RMS, "
          "as P5.09 holds") {
    int checked = 0;
    for (int index = -1; index < undertow::numFactoryPresets(); ++index) {
        if (sequenced(index))
            continue;
        const auto& wav = rendered(index);
        const double level = levelDb(wav.left, wav.right, 0, wav.frames);
        CHECK_MESSAGE(std::abs(level + 18.0) <= 3.0, slugOf(index) << " renders at " << level
                                                                   << " dBFS RMS; holding A2 at velocity 100 it plays within 3 dB of -18 "
                                                                      "(P5.09): the renderer must hold --note while the sequencer is off");
        ++checked;
    }
    CHECK_MESSAGE(checked >= 2, "init and at least one factory preset play with the sequencer off; found " << checked);
}
