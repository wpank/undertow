// Phase 5 spec tests: the offline renderer (docs/specs/phase-5-ship.md, P5.05).
// spec-needs: core/include/undertow/presets.h
// spec-target: undertow_render

#include "doctest.h"

#include "spec_analysis.h"
#include "spec_paths.h"
#include "spec_wav.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <sys/wait.h>

namespace {

struct Run {
    int status = -1;
    std::string output;
};

// Run the renderer with `args`, from the repository root, capturing what it prints.
Run render(const std::string& args) {
    Run r;
    // This checkout's renderer, run from its root (spec_paths.h).
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

} // namespace

TEST_CASE("P5.05 undertow_render writes a 24-bit stereo WAV of the preset, bar-exact, in level and repeatable") {
    REQUIRE_MESSAGE(std::string(SPEC_TARGET_UNDERTOW_RENDER).size() > 0,
                    "there is no undertow_render target: add tools/render (before tests/spec) to the top-level CMakeLists.txt");
    std::system(("mkdir -p '" + spec::sourceDir() + "/build/spec-render'").c_str());
    const auto first = render("--preset bass-hypnotic-roller --bars 2 --bpm 128 --out build/spec-render/a.wav");
    REQUIRE_MESSAGE(first.status == 0, "undertow_render exited " << first.status << ": " << first.output);
    CHECK_MESSAGE(first.output.find("wrote build/spec-render/a.wav: 180000 frames") != std::string::npos,
                  "the renderer prints `wrote <path>: <frames> frames, peak <x> dBFS, rms <x> dBFS`; it printed: " << first.output);
    const auto wav = spec::readWav(spec::sourceDir() + "/build/spec-render/a.wav");
    REQUIRE_MESSAGE(wav.ok, "the file is not a WAV I can read: " << wav.error);
    CHECK_MESSAGE(wav.format == 1, "the WAV must be integer PCM");
    CHECK_MESSAGE(wav.bitsPerSample == 24, "the WAV must be 24-bit; it is " << wav.bitsPerSample);
    CHECK_MESSAGE(wav.channels == 2, "the WAV must be stereo; it has " << wav.channels << " channels");
    CHECK_MESSAGE(wav.sampleRate == 48000, "the WAV must be 48 kHz; it is " << wav.sampleRate);
    CHECK_MESSAGE(wav.frames == 180000u, "2 bars at 128 BPM at 48 kHz are exactly 180000 frames; the file has " << wav.frames);
    CHECK_MESSAGE(std::max(spec::peakAbs(wav.left), spec::peakAbs(wav.right)) <= 0.9661, "the render passes -0.3 dBFS");
    CHECK_MESSAGE(spec::toDb(spec::rms(wav.left)) >= -30.0, "the render is too quiet: " << spec::toDb(spec::rms(wav.left)) << " dBFS RMS");

    const auto second = render("--preset bass-hypnotic-roller --bars 2 --bpm 128 --out build/spec-render/b.wav");
    REQUIRE(second.status == 0);
    const auto again = spec::readWav(spec::sourceDir() + "/build/spec-render/b.wav");
    CHECK_MESSAGE(again.bytes == wav.bytes, "the same arguments must give the same file, byte for byte");

    const auto byName = render("--preset 'BASS Hypnotic Roller' --bars 2 --bpm 128 --out build/spec-render/c.wav");
    REQUIRE_MESSAGE(byName.status == 0, "--preset must accept a preset's name: " << byName.output);
    CHECK_MESSAGE(spec::readWav(spec::sourceDir() + "/build/spec-render/c.wav").bytes == wav.bytes,
                  "--preset by name and by slug must render the same");
    const auto init = render("--preset init --bars 1 --bpm 120 --sample-rate 44100 --out build/spec-render/d.wav");
    REQUIRE_MESSAGE(init.status == 0, "--preset init (the default patch, holding --note, A2 by default) must render: " << init.output);
    const auto d = spec::readWav(spec::sourceDir() + "/build/spec-render/d.wav");
    CHECK_MESSAGE(d.sampleRate == 44100, "--sample-rate 44100 must write a 44.1 kHz file");
    CHECK_MESSAGE(d.frames == 88200u, "1 bar at 120 BPM at 44.1 kHz is 88200 frames; the file has " << d.frames);
    CHECK_MESSAGE(spec::toDb(spec::rms(d.left)) >= -40.0, "the init patch holding A2 must be heard");
    const auto bad = render("--preset no-such-preset --out build/spec-render/e.wav");
    CHECK_MESSAGE(bad.status != 0, "an unknown preset must fail (non-zero exit) with a message");
    const auto usage = render("--bars");
    CHECK_MESSAGE(usage.status != 0, "bad arguments must fail with a usage message");
}
