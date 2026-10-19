// Phase 5 spec tests: the demo renders (docs/specs/phase-5-ship.md, P5.06). Run after tools/render-demos.sh.
// spec-needs: core/include/undertow/presets.h

#include "doctest.h"

#include "spec_analysis.h"
#include "spec_wav.h"
#include "undertow/presets.h"

#include <fstream>
#include <string>

TEST_CASE("P5.06 render-demos.sh wrote the four committed showcase WAVs and a WAV of every preset") {
    const std::string root = UNDERTOW_SOURCE_DIR;
    for (const char* slug : {"init", "sub-pure-foundation", "bass-hypnotic-roller", "rumble-dark-room"}) {
        const std::string path = root + "/artifacts/audio/" + slug + ".wav";
        INFO(path);
        const auto wav = spec::readWav(path);
        REQUIRE_MESSAGE(wav.ok, "the showcase render is missing or unreadable (" << wav.error << "): tools/render-demos.sh writes it");
        CHECK_MESSAGE(wav.format == 1, "integer PCM");
        CHECK_MESSAGE(wav.bitsPerSample == 24, "24-bit");
        CHECK_MESSAGE(wav.channels == 2, "stereo");
        CHECK_MESSAGE(wav.sampleRate == 48000, "48 kHz");
        CHECK_MESSAGE(wav.frames == 720000u, "8 bars at 128 BPM: 720000 frames; this has " << wav.frames);
        CHECK_MESSAGE(wav.bytes.size() <= 5u * 1024u * 1024u, "a committed render stays under 5 MB");
        CHECK_MESSAGE(std::max(spec::peakAbs(wav.left), spec::peakAbs(wav.right)) <= 0.9661, "passes -0.3 dBFS");
        CHECK_MESSAGE(spec::toDb(spec::rms(wav.left)) >= -30.0, "too quiet: " << spec::toDb(spec::rms(wav.left)) << " dBFS RMS");
    }
    for (int i = 0; i < undertow::numFactoryPresets(); ++i) {
        const std::string path = root + "/build/demos/" + undertow::factoryPresetSlug(i) + ".wav";
        CHECK_MESSAGE(spec::readWav(path).ok, path << " is missing: render-demos.sh renders every preset into build/demos/");
    }
}
