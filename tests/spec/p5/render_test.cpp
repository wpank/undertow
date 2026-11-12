// SPDX-License-Identifier: AGPL-3.0-only
// Phase 5 spec tests: the hits the engine plays, and the renderer's hits file and preset list
// (docs/specs/phase-5-sound.md, checks P5.11 to P5.14).
// spec-needs: core/include/undertow/presets.h
// spec-target: undertow_render

#include "doctest.h"

#include "spec_alloc.h"
#include "spec_engine.h"
#include "spec_paths.h"
#include "spec_wav.h"
#include "undertow/engine.h"
#include "undertow/pattern.h"
#include "undertow/presets.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <sys/wait.h>

namespace {

constexpr double kRate = 48000.0;
using namespace undertow::param;

struct Run {
    int status = -1;
    std::string output;
};

// Run the renderer with `args`, from the repository root, capturing what it prints.
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

std::string slurp(const std::string& path) {
    std::ifstream in(path);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

// The number after `"key":` at or after `from`; NAN when it isn't there.
double field(const std::string& json, const std::string& key, std::size_t from = 0, std::size_t to = std::string::npos) {
    const auto at = json.find("\"" + key + "\"", from);
    if (at == std::string::npos || at >= to)
        return NAN;
    const auto colon = json.find(':', at);
    if (colon == std::string::npos)
        return NAN;
    return std::strtod(json.c_str() + colon + 1, nullptr);
}

struct FileHit {
    double sample, step, note, pitch, velocity;
};

// The objects of the "hits" array, each read by its keys.
std::vector<FileHit> hitsOf(const std::string& json) {
    std::vector<FileHit> out;
    auto at = json.find("\"hits\"");
    if (at == std::string::npos)
        return out;
    at = json.find('[', at);
    while (at != std::string::npos) {
        const auto open = json.find('{', at), close = json.find('}', open == std::string::npos ? at : open);
        const auto end = json.find(']', at);
        if (open == std::string::npos || close == std::string::npos || (end != std::string::npos && end < open))
            break;
        out.push_back({field(json, "sample", open, close), field(json, "step", open, close), field(json, "note", open, close),
                       field(json, "pitch", open, close), field(json, "velocity", open, close)});
        at = close + 1;
    }
    return out;
}

struct Expected {
    long long sample;
    int step, note;
    float velocity;
};

// What the default pattern plays on `root` from beat 0 at `bpm`: every sounding count's start sample, step, note and
// velocity, the drift included, before `samples`.
std::vector<Expected> expected(const spec::Params& p, int root, double bpm, long long samples) {
    undertow::Pattern pattern;
    pattern.setParams(p.data());
    const double stepSamples = 0.25 * 60.0 / bpm * kRate;
    std::vector<Expected> out;
    for (long long k = 0; static_cast<double>(k) * stepSamples < static_cast<double>(samples); ++k) {
        const auto s = pattern.at(k, root);
        if (s.velocity > 0.0f)
            out.push_back({static_cast<long long>(std::ceil(static_cast<double>(k) * stepSamples - 1e-6)),
                           static_cast<int>(k % pattern.length()), root + s.pitch, s.velocity});
    }
    return out;
}

} // namespace

TEST_CASE("P5.11 the engine reports every hit it plays, at its sample, with its step, note and velocity") {
    // The default patch under a held key, 4 bars at 130 BPM in blocks of 512.
    undertow::Engine e;
    const auto p = spec::defaults();
    e.prepare(kRate, 512);
    spec::apply(e, p);
    e.reset();
    const auto want = expected(p, 40, 130.0, static_cast<long long>(16 * 60.0 / 130.0 * kRate));
    std::vector<float> left(512), right(512);
    std::vector<undertow::Hit> got;
    std::vector<long long> at;
    undertow::Hit block[undertow::kMaxHits];
    undertow::Transport t{true, true, 130.0, 0.0};
    const auto total = static_cast<long long>(16 * 60.0 / 130.0 * kRate);
    std::size_t allocations = 0;
    for (long long start = 0; start < total; start += 512) {
        const int n = static_cast<int>(std::min<long long>(512, total - start));
        const undertow::MidiEvent key = spec::noteOn(0, 40);
        t.ppq = static_cast<double>(start) * 130.0 / (60.0 * kRate);
        spec::AllocationCounter counter;
        e.process(left.data(), right.data(), n, &key, start == 0 ? 1 : 0, t);
        const int count = e.hits(block, undertow::kMaxHits);
        allocations += counter.count();
        for (int k = 0; k < count; ++k) {
            got.push_back(block[k]);
            at.push_back(start + block[k].sampleOffset);
        }
    }
    CHECK_MESSAGE(allocations == 0, allocations << " heap allocations in process() or hits()");
    REQUIRE_MESSAGE(got.size() == want.size(), "4 bars of the default pattern play " << want.size()
                                                                                     << " hits (rests are not hits); "
                                                                                        "hits() reported "
                                                                                     << got.size());
    int wrong = 0;
    for (std::size_t i = 0; i < got.size(); ++i) {
        const bool ok = std::llabs(at[i] - want[i].sample) <= 1 && got[i].step == want[i].step &&
                        got[i].note == want[i].note && got[i].velocity == want[i].velocity;
        if (!ok && wrong++ < 4)
            FAIL_CHECK("hit " << i << ": sample " << at[i] << ", step " << got[i].step << ", note " << got[i].note
                              << ", velocity " << got[i].velocity << "; want sample " << want[i].sample << ", step "
                              << want[i].step << ", note " << want[i].note << ", velocity " << want[i].velocity);
    }
    CHECK_MESSAGE(wrong == 0, wrong << " hits differ from what the pattern plays, drift included");

    // Notes mode: each MIDI note-on is a hit with step -1; hits() copies at most what it is asked for, and the next
    // process() call starts a new list.
    undertow::Engine m;
    auto q = spec::defaults();
    spec::set(q, SeqMode, 2);
    m.prepare(kRate, 512);
    spec::apply(m, q);
    m.reset();
    const undertow::MidiEvent notes[3] = {spec::noteOn(10, 45, 127), spec::noteOn(200, 47, 64), spec::noteOn(300, 48, 0)};
    m.process(left.data(), right.data(), 512, notes, 3, t);
    undertow::Hit two[2];
    REQUIRE_MESSAGE(m.hits(block, undertow::kMaxHits) == 2, "two note-ons with velocity are two hits; a note-on at "
                                                            "velocity 0 is none");
    CHECK_MESSAGE((block[0].sampleOffset == 10 && block[0].step == -1 && block[0].note == 45 &&
                   block[0].velocity == doctest::Approx(1.0f)),
                  "a Notes mode hit carries its sample, step -1, its note and velocity / 127");
    CHECK_MESSAGE((block[1].sampleOffset == 200 && block[1].note == 47 && block[1].velocity == doctest::Approx(64.0f / 127)),
                  "the second hit at sample 200, note 47, velocity 64 / 127");
    CHECK_MESSAGE(m.hits(two, 1) == 1, "hits(destination, 1) copies one hit");
    m.process(left.data(), right.data(), 512, nullptr, 0, t);
    CHECK_MESSAGE(m.hits(block, undertow::kMaxHits) == 0, "the next process() call with no hits must report none");
}

TEST_CASE("P5.12 the renderer writes the hits it played beside the render, drift included") {
    REQUIRE_MESSAGE(std::string(SPEC_TARGET_UNDERTOW_RENDER).size() > 0, "there is no undertow_render target");
    std::system(("mkdir -p '" + spec::sourceDir() + "/build/spec-render'").c_str());
    const std::string path = spec::sourceDir() + "/build/spec-render/hits.json";
    std::remove(path.c_str());
    const auto run = render("--preset init --bars 8 --bpm 130 --note 43 --out build/spec-render/hits.wav --hits "
                            "build/spec-render/hits.json");
    REQUIRE_MESSAGE(run.status == 0, "undertow_render --hits exited " << run.status << ": " << run.output);
    const std::string json = slurp(path);
    REQUIRE_MESSAGE(!json.empty(), "--hits build/spec-render/hits.json wrote nothing");
    CHECK_MESSAGE(field(json, "rate") == 48000, "the file must say \"rate\": 48000");
    CHECK_MESSAGE(field(json, "bpm") == 130, "the file must say \"bpm\": 130");
    CHECK_MESSAGE(field(json, "bars") == 8, "the file must say \"bars\": 8");
    CHECK_MESSAGE(field(json, "root") == 43, "the file must say \"root\": 43, the key held");
    const auto hits = hitsOf(json);
    const auto want = expected(spec::defaults(), 43, 130.0, static_cast<long long>(32 * 60.0 / 130.0 * kRate));
    REQUIRE_MESSAGE(hits.size() == want.size(), "8 bars of Init play " << want.size() << " hits; the file lists "
                                                                       << hits.size());
    int wrong = 0, drifted = 0;
    for (std::size_t i = 0; i < hits.size(); ++i) {
        const auto& h = hits[i];
        const bool ok = std::fabs(h.sample - static_cast<double>(want[i].sample)) <= 1 && h.step == want[i].step + 1 &&
                        h.note == want[i].note && h.pitch == want[i].note - 43 &&
                        std::fabs(h.velocity - static_cast<double>(want[i].velocity)) <= 1e-5;
        drifted += want[i].note != 43 ? 1 : 0;
        if (!ok && wrong++ < 4)
            FAIL_CHECK("hit " << i << " reads sample " << h.sample << ", step " << h.step << ", note " << h.note
                              << ", pitch " << h.pitch << ", velocity " << h.velocity << "; want sample "
                              << want[i].sample << ", step " << want[i].step + 1 << " (counting from 1), note "
                              << want[i].note << ", pitch " << want[i].note - 43 << ", velocity " << want[i].velocity);
    }
    CHECK_MESSAGE(wrong == 0, wrong << " hits in the file differ from what the engine played");
    CHECK(drifted > 0); // the default drift moves some of these 8 bars' notes
    // The render itself is unchanged by --hits.
    const auto plain = render("--preset init --bars 8 --bpm 130 --note 43 --out build/spec-render/plain.wav");
    REQUIRE(plain.status == 0);
    CHECK_MESSAGE(spec::readWav(spec::sourceDir() + "/build/spec-render/plain.wav").bytes ==
                      spec::readWav(spec::sourceDir() + "/build/spec-render/hits.wav").bytes,
                  "writing the hits must not change the render");
}

TEST_CASE("P5.13 the renderer holds the preset's own root unless --note says otherwise") {
    const auto own = render("--preset init --bars 1 --bpm 130 --out build/spec-render/own.wav --hits "
                            "build/spec-render/own.json");
    REQUIRE_MESSAGE(own.status == 0, own.output);
    CHECK_MESSAGE(field(slurp(spec::sourceDir() + "/build/spec-render/own.json"), "root") == 43,
                  "without --note the renderer holds the preset's seq_root: Init's is 43");
    const auto other = render("--preset init --bars 1 --bpm 130 --note 36 --out build/spec-render/other.wav --hits "
                              "build/spec-render/other.json");
    REQUIRE_MESSAGE(other.status == 0, other.output);
    const std::string json = slurp(spec::sourceDir() + "/build/spec-render/other.json");
    CHECK_MESSAGE(field(json, "root") == 36, "--note 36 holds note 36");
    const auto hits = hitsOf(json);
    REQUIRE(!hits.empty());
    CHECK_MESSAGE(hits[0].note == 36 + hits[0].pitch, "each hit's note is the root plus its pitch");
    const auto bad = render("--preset init --note 200 --out build/spec-render/bad.wav");
    CHECK_MESSAGE(bad.status != 0, "--note 200 is not a MIDI note: the renderer must refuse it");
}

TEST_CASE("P5.14 --list prints every factory preset's slug, and render-demos.sh renders each with its hits") {
    const auto list = render("--list");
    REQUIRE_MESSAGE(list.status == 0, "undertow_render --list exited " << list.status << ": " << list.output);
    std::vector<std::string> slugs;
    std::istringstream lines(list.output);
    for (std::string line; std::getline(lines, line);)
        if (!line.empty())
            slugs.push_back(line);
    REQUIRE_MESSAGE(static_cast<int>(slugs.size()) == undertow::numFactoryPresets(),
                    "--list printed " << slugs.size() << " lines; there are " << undertow::numFactoryPresets()
                                      << " factory presets, one slug a line");
    for (int i = 0; i < undertow::numFactoryPresets(); ++i)
        CHECK_MESSAGE(slugs[static_cast<std::size_t>(i)] == undertow::factoryPresetSlug(i),
                      "line " << i + 1 << " must be preset " << i << "'s slug, " << undertow::factoryPresetSlug(i));
    const std::string demos = spec::sourceDir() + "/build/demos";
    std::system(("mkdir -p '" + demos + "' && touch '" + demos + "/stale.wav'").c_str());
    const int status = std::system(("cd '" + spec::sourceDir() + "' && tools/render-demos.sh > /dev/null 2>&1").c_str());
    const bool ran = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    REQUIRE_MESSAGE(ran, "tools/render-demos.sh failed");
    CHECK_MESSAGE(!std::ifstream(demos + "/stale.wav").good(), "render-demos.sh must start from an empty build/demos");
    for (const auto& slug : slugs) {
        const auto wav = spec::readWav(demos + "/" + slug + ".wav");
        INFO("preset " << slug);
        CHECK_MESSAGE(wav.ok, "build/demos/" << slug << ".wav is missing or unreadable: " << wav.error);
        CHECK_MESSAGE(wav.frames == static_cast<std::size_t>(8 * std::llround(4 * 60.0 / 130.0 * kRate)),
                      "each demo is 8 bars at 130 BPM, each bar rounded to whole samples: " << wav.frames << " frames");
        const std::string json = slurp(demos + "/" + slug + "-hits.json");
        CHECK_MESSAGE(json.find("\"hits\"") != std::string::npos, "build/demos/" << slug << "-hits.json must list the "
                                                                                       "demo's hits");
    }
}
