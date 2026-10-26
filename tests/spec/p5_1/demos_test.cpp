// Phase 5.1 spec tests: the demo renders are exactly what undertow_render writes, with nothing done to them
// afterwards (docs/specs/phase-5.1-renders.md, P5.1.04 and P5.1.05). Run after tools/render-demos.sh.
// spec-needs: core/include/undertow/presets.h
// spec-target: undertow_render

#include "doctest.h"

#include "spec_paths.h"
#include "spec_wav.h"
#include "undertow/presets.h"

#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <regex>
#include <set>
#include <string>
#include <vector>

#include <sys/wait.h>

namespace {

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

// The renderer's file for a slug, with tools/render-demos.sh's arguments: 8 bars at 128 BPM, the rest the defaults.
std::vector<char> renderedBytes(const std::string& slug) {
    std::system(("mkdir -p '" + spec::sourceDir() + "/build/spec-render'").c_str());
    const std::string out = "build/spec-render/p5_1-demo-" + slug + ".wav";
    const auto run = render("--preset " + slug + " --bars 8 --bpm 128 --out " + out);
    REQUIRE_MESSAGE(run.status == 0, "undertow_render exited " << run.status << ": " << run.output);
    const auto wav = spec::readWav(spec::sourceDir() + "/" + out);
    REQUIRE_MESSAGE(wav.ok, "the renderer's file is not a WAV I can read: " << wav.error);
    return wav.bytes;
}

std::set<std::string> wavsIn(const std::string& directory) {
    std::set<std::string> names;
    if (DIR* dir = opendir(directory.c_str())) {
        while (const dirent* entry = readdir(dir)) {
            const std::string name = entry->d_name;
            if (name.size() > 4 && name.compare(name.size() - 4, 4, ".wav") == 0)
                names.insert(name);
        }
        closedir(dir);
    }
    return names;
}

} // namespace

TEST_CASE("P5.1.04 every demo render is exactly undertow_render's file: the four committed showcase WAVs and build/demos") {
    const std::string root = spec::sourceDir();
    const std::set<std::string> showcase{"init.wav", "sub-pure-foundation.wav", "bass-hypnotic-roller.wav", "rumble-dark-room.wav"};
    CHECK_MESSAGE(wavsIn(root + "/artifacts/audio") == showcase,
                  "artifacts/audio holds exactly the four showcase renders: init, sub-pure-foundation, bass-hypnotic-roller and "
                  "rumble-dark-room");
    for (const auto& name : showcase) {
        const std::string slug = name.substr(0, name.size() - 4);
        const auto committed = spec::readWav(root + "/artifacts/audio/" + name);
        REQUIRE_MESSAGE(committed.ok, "artifacts/audio/" << name << " is missing or unreadable: tools/render-demos.sh writes it");
        CHECK_MESSAGE(committed.bytes == renderedBytes(slug),
                      "artifacts/audio/" << name << " differs from undertow_render --preset " << slug
                                         << " --bars 8 --bpm 128: a demo is the renderer's file as written. Never rescale, pad or "
                                            "make up samples; a render that is too quiet means the renderer or the preset is wrong");
    }
    for (int i = 0; i < undertow::numFactoryPresets(); ++i) {
        const std::string slug = undertow::factoryPresetSlug(i);
        const auto demo = spec::readWav(root + "/build/demos/" + slug + ".wav");
        REQUIRE_MESSAGE(demo.ok, "build/demos/" << slug << ".wav is missing: tools/render-demos.sh renders every preset there");
        CHECK_MESSAGE(demo.bytes == renderedBytes(slug), "build/demos/" << slug << ".wav differs from undertow_render --preset " << slug
                                                                       << " --bars 8 --bpm 128: the script writes the renderer's files as they "
                                                                          "come");
    }
}

TEST_CASE("P5.1.05 tools/render-demos.sh hands on the renderer's files untouched: it runs no interpreter or audio tool") {
    std::ifstream script(spec::sourceDir() + "/tools/render-demos.sh");
    REQUIRE_MESSAGE(script.good(), "tools/render-demos.sh is missing");
    // A program that could rewrite a WAV after the renderer wrote it.
    const std::regex rewriter(R"(\b(python3?|perl|ruby|node|sox|ffmpeg|afconvert|dd|xxd|truncate)\b)");
    std::string line;
    for (int number = 1; std::getline(script, line); ++number) {
        const auto first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == '#')
            continue;
        std::smatch found;
        CHECK_MESSAGE(!std::regex_search(line, found, rewriter),
                      "tools/render-demos.sh:" << number << " runs " << found.str() << ": `" << line
                                               << "`. The script builds undertow_render and runs it, and nothing else touches a "
                                                  "render; never rescale, pad or make up samples");
    }
}
