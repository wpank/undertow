// Phase 2.1 spec tests: glides that keep their time and their pitch (docs/specs/phase-2.1-voice-fixes.md, P2.1.02 and
// P2.1.03).
// spec-needs: core/include/undertow/voice.h

#include "doctest.h"

#include "spec_engine.h"
#include "undertow/voice.h"

#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr double kRate = 48000.0;

struct Voice {
    undertow::MonoVoice voice;
    spec::Params params;
    explicit Voice(spec::Params p) : params(p) {
        voice.prepare(kRate);
        voice.setParams(params.data());
    }
    void set(const char* name, float value) {
        spec::set(params, name, value);
        voice.setParams(params.data());
    }
    void run(std::size_t samples) {
        for (std::size_t n = 0; n < samples; ++n)
            voice.process();
    }
};

// The played pitch in semitones over note 45 (110 Hz), from currentHz().
double over45(const undertow::MonoVoice& voice) { return 12.0 * std::log2(voice.currentHz() / 110.0); }

spec::Params glidePatch() {
    auto p = spec::subPatch();
    spec::set(p, "glide_ms", 200);
    spec::set(p, "glide_mode", 1); // Legato
    return p;
}

} // namespace

TEST_CASE("P2.1.02 releasing a held note that is not sounding leaves the glide alone: it lands on time, in a straight line") {
    // A MIDI slide as a piano roll plays it: note 57 starts while 45 is held (a 200 ms legato glide from 45 to 57), and
    // 45 is released 50 ms later. 57 sounds throughout, so the glide must go on as it started.
    Voice v(glidePatch());
    v.voice.noteOn(45, 0.7f);
    v.run(24000);
    v.voice.noteOn(57, 0.7f);
    std::vector<double> pitch; // one value per sample from the note-on
    for (int n = 0; n < 19200; ++n) {
        if (n == 2400)
            v.voice.noteOff(45);
        v.voice.process();
        pitch.push_back(over45(v.voice));
    }
    std::size_t landed = pitch.size();
    for (std::size_t n = 0; n < pitch.size(); ++n)
        if (pitch[n] >= 12.0 - 0.01) {
            landed = n;
            break;
        }
    const double landedMs = static_cast<double>(landed) / kRate * 1000.0;
    CHECK_MESSAGE(std::fabs(landedMs - 200.0) <= 3.0, "the glide reached note 57 " << landedMs << " ms after its note-on; with "
                                                                                     "glide_ms 200 it must land at 200 ms (+- 3), "
                                                                                     "however the notes overlap. Releasing 45, "
                                                                                     "which is not sounding, must not restart it");
    for (const double fraction : {0.25, 0.5, 0.75}) {
        const auto at = static_cast<std::size_t>(fraction * 9600.0);
        CHECK_MESSAGE(std::fabs(pitch[at] - 12.0 * fraction) <= 0.1,
                      static_cast<int>(fraction * 200) << " ms into the glide the pitch is " << pitch[at]
                                                       << " semitones over 45; a straight line from 45 to 57 gives " << 12.0 * fraction);
    }
    CHECK_MESSAGE(std::fabs(pitch.back() - 12.0) <= 0.01, "after the glide the pitch is " << pitch.back() << " semitones over 45; want 12");
}

TEST_CASE("P2.1.03 a glide turned off part way leaves no note detuned: the next note plays its own pitch") {
    // Halfway through a 200 ms legato glide from 45 to 57, glide_mode goes Off (or glide_ms goes to 0). The next
    // legato note, and a return to a held note, must then play exactly their own pitch, at once and for good.
    const std::vector<std::pair<const char*, float>> offs = {{"glide_mode", 0.0f}, {"glide_ms", 0.0f}};
    for (const auto& off : offs) {
        const char* name = off.first;
        const float value = off.second;
        INFO(std::string(name) << " set to " << value << " halfway through the glide");
        Voice v(glidePatch());
        v.voice.noteOn(45, 0.7f);
        v.run(24000);
        v.voice.noteOn(57, 0.7f);
        v.run(4800);
        v.set(name, value);
        v.voice.noteOn(64, 0.7f); // legato over 45 and 57: no glide now
        v.run(48);
        const double cents = 100.0 * (over45(v.voice) - 19.0);
        CHECK_MESSAGE(std::fabs(cents) <= 1.0, "1 ms after note 64 the pitch is " << cents << " cents from note 64");
        v.run(24000);
        const double later = 100.0 * (over45(v.voice) - 19.0);
        CHECK_MESSAGE(std::fabs(later) <= 1.0, "0.5 s after note 64 the pitch is " << later << " cents from note 64");

        Voice w(glidePatch());
        w.voice.noteOn(45, 0.7f);
        w.run(24000);
        w.voice.noteOn(57, 0.7f);
        w.run(4800);
        w.set(name, value);
        w.voice.noteOff(57); // back to the held 45, with no glide
        w.run(48);
        const double back = 100.0 * over45(w.voice);
        CHECK_MESSAGE(std::fabs(back) <= 1.0, "1 ms after releasing 57 the pitch is " << back << " cents from the held note 45");
        w.run(24000);
        const double backLater = 100.0 * over45(w.voice);
        CHECK_MESSAGE(std::fabs(backLater) <= 1.0, "0.5 s after releasing 57 the pitch is " << backLater << " cents from note 45");
    }
}
