// Phase 5.3 spec tests: the four showcase renders play at one level, init's, so the reel doesn't dip at BASS Hypnotic
// Roller (docs/specs/phase-5.3-presets.md, P5.3.05). Run after tools/render-demos.sh.
// spec-needs: core/include/undertow/presets.h

#include "doctest.h"

#include "spec_analysis.h"
#include "spec_paths.h"
#include "spec_wav.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {

// A render's level: the RMS of both channels together, in dBFS, as P5.09 measures it.
double levelDb(const spec::Wav& wav) {
    const double left = spec::rms(wav.left), right = spec::rms(wav.right);
    return 10.0 * std::log10(std::max(0.5 * (left * left + right * right), 1e-30));
}

} // namespace

TEST_CASE("P5.3.05 the showcase renders play within 1.5 dB of init's level") {
    const std::string root = spec::sourceDir();
    const auto init = spec::readWav(root + "/artifacts/audio/init.wav");
    REQUIRE_MESSAGE(init.ok, "artifacts/audio/init.wav is missing or unreadable (" << init.error << "): tools/render-demos.sh writes it");
    const double anchor = levelDb(init);
    for (const char* slug : {"sub-pure-foundation", "bass-hypnotic-roller", "rumble-dark-room"}) {
        const std::string name = slug;
        const auto wav = spec::readWav(root + "/artifacts/audio/" + name + ".wav");
        REQUIRE_MESSAGE(wav.ok, "artifacts/audio/" << name << ".wav is missing or unreadable (" << wav.error << ")");
        const double level = levelDb(wav);
        CHECK_MESSAGE(std::abs(level - anchor) <= 1.5, name << ".wav plays at " << level << " dBFS RMS, " << level - anchor
                                                            << " dB from init's " << anchor
                                                            << ": the showcase reel plays its four renders at one level, within 1.5 dB "
                                                               "of init's. Set the preset's master_gain, then run tools/render-demos.sh");
    }
}
