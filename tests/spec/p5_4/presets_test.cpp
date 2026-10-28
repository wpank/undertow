// Phase 5.4 spec tests: SUB Kick Companion as named, "a short, round sub note that sits after a kick", and apart from
// the two SUB presets that hold their note (docs/specs/phase-5.4-presets.md, P5.4.01 to P5.4.06). Each reads a preset's
// voice alone: the preset as P5.09 renders it, holding A2 at velocity 100, with its rumble layer muted, as the mid of
// its two channels.
// spec-needs: core/include/undertow/presets.h

#include "doctest.h"

#include "spec_analysis.h"
#include "spec_engine.h"
#include "undertow/engine.h"
#include "undertow/presets.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr double kRate = 48000.0;
constexpr double kPi = 3.14159265358979323846;

// A preset's voice: 2 bars at 128 BPM from beat 0, A2 (45) held at velocity 100 when its sequencer is off,
// rumble_level 0; the mid of the two channels. Each voice is rendered once and shared by the test cases.
const spec::Signal& voiceOf(const char* name) {
    static std::map<std::string, spec::Signal> voices;
    auto found = voices.find(name);
    if (found != voices.end())
        return found->second;
    const int index = undertow::findFactoryPreset(name);
    REQUIRE_MESSAGE(index >= 0, "there must be a preset named " << std::string(name));
    spec::Params values{};
    undertow::Pattern pattern;
    REQUIRE(undertow::factoryPreset(index, values.data(), &pattern));
    values[static_cast<std::size_t>(spec::id("rumble_level"))] = 0.0f;
    undertow::Engine engine;
    engine.prepare(kRate, 512);
    spec::apply(engine, values);
    engine.setPattern(pattern);
    engine.reset();
    std::vector<undertow::MidiEvent> events;
    if (values[static_cast<std::size_t>(spec::id("seq_on"))] < 0.5f)
        events.push_back(spec::noteOn(0, 45, 100));
    const auto out = spec::render(engine, 180000, events, undertow::Transport{true, true, 128.0, 0.0}, 512);
    spec::Signal mid(out.left.size());
    for (std::size_t i = 0; i < mid.size(); ++i)
        mid[i] = 0.5f * (out.left[i] + out.right[i]);
    return voices.emplace(name, std::move(mid)).first->second;
}

const spec::Signal& kickVoice() { return voiceOf("SUB Kick Companion"); }

// A voice's envelope: its RMS over one cycle of 55 Hz (873 samples), every 1 ms (48 samples), and its loudest window
// in the first second.
struct Envelope {
    std::vector<double> rms;
    std::size_t peakIndex = 0;
    double peak() const { return rms[peakIndex]; }
    static double ms(std::size_t i) { return (48.0 * static_cast<double>(i) + 436.5) / 48.0; } // window i's centre
};

Envelope envelopeOf(const spec::Signal& voice) {
    Envelope e;
    e.rms = spec::rmsEnvelope(voice, 873, 48);
    for (std::size_t i = 0; i < e.rms.size() && Envelope::ms(i) < 1000.0; ++i)
        if (e.rms[i] > e.rms[e.peakIndex])
            e.peakIndex = i;
    return e;
}

// P5.3.03's reading: the voice's first 40 ms re its level at 280 to 320 ms, in dB.
double onsetDb(const spec::Signal& voice) {
    return spec::toDb(spec::rms(voice, 0, 1920) / std::max(spec::rms(voice, 13440, 15360), 1e-30));
}

} // namespace

TEST_CASE("P5.4.01 SUB Kick Companion is short: holding A2, its voice falls 20 dB under its peak 120 to 300 ms after it starts") {
    const auto env = envelopeOf(kickVoice());
    REQUIRE_MESSAGE(env.peak() > 0.0, "SUB Kick Companion holding A2 must sound");
    double fall = -1.0;
    for (std::size_t i = env.peakIndex; i < env.rms.size(); ++i)
        if (env.rms[i] <= 0.1 * env.peak()) {
            fall = Envelope::ms(i);
            break;
        }
    REQUIRE_MESSAGE(fall >= 0.0, "holding A2, SUB Kick Companion's voice never falls 20 dB under its peak: it holds its note. \"A "
                                 "short sub note\" decays and is gone: amp_sustain 0, with an amp decay of about 250 to 500 ms");
    CHECK_MESSAGE(fall >= 120.0, "holding A2, SUB Kick Companion's voice falls 20 dB under its peak " << fall
                                                                                                       << " ms after it starts: a blip, not a "
                                                                                                          "note. A short note lasts at least "
                                                                                                          "120 ms (an amp decay of about 200 ms "
                                                                                                          "or more; 400 ms falls at 223 ms)");
    CHECK_MESSAGE(fall <= 300.0, "holding A2, SUB Kick Companion's voice falls 20 dB under its peak only " << fall
                                                                                                          << " ms after it starts; a short note "
                                                                                                             "is gone within 300 ms (amp_sustain 0 "
                                                                                                             "and an amp decay of about 500 ms or "
                                                                                                             "less; 600 ms falls at 326 ms)");
}

TEST_CASE("P5.4.02 SUB Kick Companion's note ends: from 1 s on, its voice stays at least 40 dB under its peak") {
    const auto env = envelopeOf(kickVoice());
    REQUIRE_MESSAGE(env.peak() > 0.0, "SUB Kick Companion holding A2 must sound");
    double tail = 0.0;
    for (std::size_t i = 0; i < env.rms.size(); ++i)
        if (Envelope::ms(i) >= 1000.0)
            tail = std::max(tail, env.rms[i]);
    const double tailDb = spec::toDb(tail / env.peak());
    CHECK_MESSAGE(tailDb <= -40.0, "holding A2, SUB Kick Companion's voice reaches " << tailDb
                                                                                    << " dB re its peak from 1 s on; a short note ends rather "
                                                                                       "than holding quietly: amp_sustain 0. A sustain of 0.02 "
                                                                                       "holds it at -32 dB, 0.9 at -1 dB");
}

TEST_CASE("P5.4.03 SUB Kick Companion starts round: no click in its first 5 ms") {
    const auto& voice = kickVoice();
    const double peak = envelopeOf(voice).peak();
    REQUIRE_MESSAGE(peak > 0.0, "SUB Kick Companion holding A2 must sound");
    const auto high = spec::highpass(voice, 2000.0, kRate, 4);
    const double bright = spec::toDb(spec::rms(high, 0, 240) / peak);
    CHECK_MESSAGE(bright <= -45.0, "in its first 5 ms, the voice's energy above 2 kHz is " << bright
                                                                                           << " dB re its peak; a round note starts without a "
                                                                                              "click, at least 45 dB under (the sine sub starts "
                                                                                              "at about -60 to -72 dB). A blip of the main "
                                                                                              "oscillator or the filter envelope at the start "
                                                                                              "is a click");
    double step = 0.0;
    for (std::size_t n = 1; n <= 240; ++n)
        step = std::max(step, std::abs(static_cast<double>(voice[n]) - static_cast<double>(voice[n - 1])));
    // A 55 Hz sine at the peak's RMS steps at most 2 pi 55 / 48000 times its amplitude between samples.
    const double sineStep = 2.0 * kPi * 55.0 / kRate * std::sqrt(2.0) * peak;
    CHECK_MESSAGE(step <= 2.0 * sineStep, "in its first 5 ms, the voice's steepest step from one sample to the next is "
                                              << step / sineStep
                                              << " times a 55 Hz sine's at its peak level; a round start is at most 2 times (the sine sub "
                                                 "starts at about 0.6 to 1.1)");
}

TEST_CASE("P5.4.04 SUB Kick Companion is round in tone: its harmonics at least 40 dB under its 55 Hz sub") {
    const auto& voice = kickVoice();
    // The loud part of the note: 8192 samples from 20 ms, to 191 ms.
    const auto loud = spec::spectrum(voice, 960, 1u << 13, kRate);
    const double f0 = spec::peakHz(loud, 20.0, 80.0);
    REQUIRE_MESSAGE(std::abs(f0 - 55.0) <= 3.0, "holding A2, SUB Kick Companion's voice centres at "
                                                    << f0 << " Hz; it is A2's sub at -1 oct, 55 Hz, as it is today (sub_octave -1 oct)");
    const double thd = spec::toDb(spec::thd(loud, f0, 10));
    CHECK_MESSAGE(thd <= -40.0, "over 20 to 190 ms, harmonics 2 to 10 of the voice sit "
                                    << thd
                                    << " dB re its 55 Hz sub; a round note keeps them at least 40 dB under (sat_drive about 0.3 or less: "
                                       "0.1 gives -56 dB, 0.4 gives -36 dB). A main oscillator mixed in reads as harmonics too");
}

TEST_CASE("P5.4.05 SUB Kick Companion is apart from SUB Pure Foundation and SUB Low Pressure: its onset at least 20 dB "
          "further over its level at 300 ms than theirs") {
    const double kick = onsetDb(kickVoice());
    for (const char* other : {"SUB Pure Foundation", "SUB Low Pressure"}) {
        const std::string name = other;
        const double theirs = onsetDb(voiceOf(other));
        CHECK_MESSAGE(kick >= theirs + 20.0, "SUB Kick Companion's first 40 ms sit "
                                                 << kick << " dB re its level at 280 to 320 ms, and " << name << "'s " << theirs
                                                 << " dB; a short note after a kick is gone by 300 ms where the held SUB presets "
                                                    "still sound, at least 20 dB further apart");
    }
}

TEST_CASE("P5.4.06 SUB Pure Foundation and SUB Low Pressure still hold their note: the last 300 ms of 2 bars within 3 dB of "
          "their peak") {
    // P5.09 levels a SUB preset by its loudest 300 ms (phase 5.4), which no longer reads a fade: this holds the two SUB
    // presets named for a held note to it.
    for (const char* held : {"SUB Pure Foundation", "SUB Low Pressure"}) {
        const std::string name = held;
        const auto& voice = voiceOf(held);
        const auto env = envelopeOf(voice);
        REQUIRE_MESSAGE(env.peak() > 0.0, name << " holding A2 must sound");
        const double end = spec::toDb(spec::rms(voice, 165600, 180000) / env.peak());
        CHECK_MESSAGE(end >= -3.0, "holding A2 for 2 bars, " << name << "'s voice ends " << end
                                                             << " dB re its peak; it holds its note, within 3 dB (amp_sustain 0.7 or "
                                                                "more). Only SUB Kick Companion is a short note");
    }
}
