// Phase 1 spec tests: the mono voice (docs/specs/phase-1-voice.md, P1.11 to P1.15).
// spec-needs: core/include/undertow/voice.h

#include "doctest.h"

#include "spec_analysis.h"
#include "spec_engine.h"
#include "undertow/voice.h"

#include <cmath>
#include <utility>
#include <vector>

namespace {

constexpr double kRate = 48000.0;

bool near(double value, double want, double tolerance) { return std::fabs(value - want) <= tolerance; }
double semitones(double hz, double reference) { return 12.0 * std::log2(hz / reference); }

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
    spec::Signal run(std::size_t samples) {
        spec::Signal out(samples);
        for (auto& v : out)
            v = voice.process();
        return out;
    }
};

// (time in seconds, frequency) from the periods between rising zero crossings of a sine-like signal.
std::vector<std::pair<double, double>> pitchTrack(const spec::Signal& x, std::size_t offset = 0) {
    const auto crossings = spec::risingZeroCrossings(x);
    std::vector<std::pair<double, double>> track;
    for (std::size_t i = 1; i < crossings.size(); ++i)
        track.emplace_back((offset + 0.5 * (crossings[i] + crossings[i - 1])) / kRate, kRate / (crossings[i] - crossings[i - 1]));
    return track;
}

double pitchAt(const std::vector<std::pair<double, double>>& track, double seconds) {
    double best = 0, distance = 1e9;
    for (const auto& [t, hz] : track)
        if (std::fabs(t - seconds) < distance) {
            distance = std::fabs(t - seconds);
            best = hz;
        }
    return best;
}

} // namespace

TEST_CASE("P1.11 notes play at their pitch, and the sub is a clean sine one or two octaves down, after the filter") {
    Voice v(spec::subPatch());
    v.voice.noteOn(45, 0.7f);
    const auto out = v.run(72000);
    CHECK_MESSAGE(v.voice.currentNote() == 45, "currentNote() is " << v.voice.currentNote() << " while note 45 plays");
    CHECK_MESSAGE(near(v.voice.currentHz(), 110.0, 0.01), "note 45 is 110 Hz; currentHz() is " << v.voice.currentHz());
    const double sub = spec::zeroCrossingHz(out, 24000, 72000, kRate);
    CHECK_MESSAGE(near(sub, 55.0, 0.165), "sub_octave '-1 oct' on note 45 (110 Hz) must sound 55 Hz; it sounds " << sub
                                                                               << " Hz. The sub is the note's frequency / 2");

    Voice two(spec::subPatch());
    two.set("sub_octave", 1);
    two.voice.noteOn(45, 0.7f);
    const auto low = two.run(96000);
    const double sub2 = spec::zeroCrossingHz(low, 24000, 96000, kRate);
    CHECK_MESSAGE(near(sub2, 27.5, 0.0825), "sub_octave '-2 oct' on note 45 must sound 27.5 Hz; it sounds " << sub2 << " Hz");

    // The sub is mixed after the filter: cutoff, resonance and drive never touch it.
    Voice dark(spec::subPatch());
    dark.set("filter_cutoff", 20);
    dark.set("filter_res", 1);
    dark.set("filter_drive", 1);
    dark.voice.noteOn(45, 0.7f);
    const auto clean = dark.run(1u << 17);
    const double level = spec::rms(clean, 48000);
    CHECK_MESSAGE(near(level, 0.5 / std::sqrt(2.0), 0.01), "a sub at level 0.5 has an RMS of 0.354; with the filter at 20 Hz, "
                                                           "full resonance and full drive it is "
                                                               << level << ": mix the sub after the filter");
    const double distortion = spec::thd(spec::spectrum(clean, 1u << 16, 1u << 16, kRate), 55.0, 10);
    CHECK_MESSAGE(spec::toDb(distortion) <= -60.0, "the sub must stay a clean sine (THD " << spec::toDb(distortion)
                                                                                         << " dB, want -60 or less)");

    Voice saw(spec::cleanPatch());
    saw.set("osc_octave", -1);
    saw.set("osc_fine", 50);
    saw.voice.noteOn(45, 0.7f);
    const auto tone = saw.run(1u << 17);
    const double hz = spec::peakHz(spec::spectrum(tone, 1u << 16, 1u << 16, kRate), 30.0, 100.0);
    const double want = 55.0 * std::exp2(50.0 / 1200.0);
    CHECK_MESSAGE(near(hz, want, want * 0.002), "osc_octave -1 and osc_fine +50 ct on note 45 must play " << want << " Hz; got "
                                                                                                         << hz);
}

TEST_CASE("P1.12 legato notes glide in glide_ms without retriggering, and the last held note wins") {
    auto patch = spec::subPatch();
    spec::set(patch, "glide_ms", 100);
    spec::set(patch, "glide_mode", 1);
    spec::set(patch, "amp_attack", 50);
    spec::set(patch, "amp_decay", 100);
    spec::set(patch, "amp_sustain", 0.5f);
    spec::set(patch, "amp_release", 50);
    Voice v(patch);
    v.voice.noteOn(45, 0.7f);
    const auto first = v.run(24000);
    v.voice.noteOn(57, 0.7f); // played over the held 45: legato
    const auto second = v.run(24000);
    const auto track = pitchTrack(second);
    const double halfway = semitones(2.0 * pitchAt(track, 0.050), 110.0);
    CHECK_MESSAGE(near(halfway, 6.0, 0.6), "50 ms into a 100 ms legato glide from note 45 to 57 the pitch is " << halfway
                                                                                                         << " semitones above 45; "
                                                                                                            "want 6 (a glide "
                                                                                                            "linear in pitch "
                                                                                                            "that takes "
                                                                                                            "glide_ms)");
    const double arrived = 2.0 * pitchAt(track, 0.200);
    CHECK_MESSAGE(near(arrived, 220.0, 1.0), "after the glide the pitch is " << arrived << " Hz; want 220 Hz (note 57)");
    const double before = spec::rms(first, 24000 - 2400);
    const double after = spec::rms(second, 0, 4800);
    CHECK_MESSAGE(after <= before * 1.25, "a legato note must not restart the envelopes: the level went from " << before << " to "
                                                                                                           << after);

    v.voice.noteOff(57); // back to the held 45, gliding, still legato
    const auto back = v.run(24000);
    CHECK_MESSAGE(v.voice.currentNote() == 45, "releasing the newest note returns to the held note 45; currentNote() is "
                                                   << v.voice.currentNote());
    CHECK_MESSAGE(near(2.0 * pitchAt(pitchTrack(back), 0.300), 110.0, 0.6), "the voice must return to note 45's 110 Hz");
    v.voice.noteOff(45);
    v.run(24000);
    CHECK_MESSAGE(!v.voice.isActive(), "releasing the last held note releases the voice (50 ms release, 0.5 s later it is idle)");

    // glide_mode Off: an overlapping note jumps.
    auto off = patch;
    spec::set(off, "glide_mode", 0);
    Voice o(off);
    o.voice.noteOn(45, 0.7f);
    o.run(24000);
    o.voice.noteOn(57, 0.7f);
    o.run(48);
    CHECK_MESSAGE(near(o.voice.currentHz(), 220.0, 0.5), "with glide_mode Off a new note jumps; 1 ms after it the pitch is "
                                                            << o.voice.currentHz() << " Hz");

    // A note that does not overlap retriggers the envelopes, and only glide_mode Always glides it.
    Voice r(patch);
    r.voice.noteOn(45, 0.7f);
    const auto held = r.run(24000);
    r.voice.noteOff(45);
    r.run(480);
    r.voice.noteOn(57, 0.7f);
    r.run(48);
    CHECK_MESSAGE(near(r.voice.currentHz(), 220.0, 0.5), "in Legato mode a detached note does not glide; 1 ms in, the pitch is "
                                                            << r.voice.currentHz());
    const auto attack = r.run(4800);
    CHECK_MESSAGE(spec::peakAbs(attack, 2400) >= 1.3 * spec::peakAbs(held, 24000 - 4800),
                  "a detached note must retrigger the attack (towards level 1) from the release");
    auto always = patch;
    spec::set(always, "glide_mode", 2);
    Voice a(always);
    a.voice.noteOn(45, 0.7f);
    a.run(24000);
    a.voice.noteOff(45);
    a.run(480);
    a.voice.noteOn(57, 0.7f);
    a.run(2400);
    CHECK_MESSAGE(near(semitones(a.voice.currentHz(), 110.0), 6.0, 0.6),
                  "in Always mode a detached note glides too; 50 ms in it is " << semitones(a.voice.currentHz(), 110.0)
                                                                               << " semitones above 45");
}

TEST_CASE("P1.13 accent makes a note louder and brighter, and pitch bend moves it by bend_range") {
    auto loud = [](float accent, float velocity) {
        auto p = spec::subPatch();
        spec::set(p, "accent", accent);
        Voice v(p);
        v.voice.noteOn(45, velocity);
        return spec::peakAbs(v.run(24000), 4800);
    };
    const double ratio = loud(1.0f, 1.0f) / loud(1.0f, 90.0f / 127.0f);
    CHECK_MESSAGE(ratio >= 1.8, "with accent 1 a velocity-127 note is only " << ratio << "x the level of a velocity-90 note; "
                                                                                       "accent multiplies the level by (1 + accent)");
    CHECK_MESSAGE(ratio <= 2.2, "with accent 1 an accented note is " << ratio << "x louder; want 2x");
    const double flat = loud(0.0f, 1.0f) / loud(0.0f, 90.0f / 127.0f);
    CHECK_MESSAGE(near(flat, 1.0, 0.02), "with accent 0 velocity must not change the level; ratio " << flat);

    auto brightness = [](float velocity) {
        auto p = spec::cleanPatch();
        spec::set(p, "accent", 1);
        spec::set(p, "filter_cutoff", 300);
        spec::set(p, "filter_env", 2);
        spec::set(p, "filt_decay", 400);
        spec::set(p, "filt_sustain", 0);
        Voice v(p);
        v.voice.noteOn(45, velocity);
        const auto out = v.run(1u << 14);
        const auto s = spec::spectrum(out, 0, 1u << 14, kRate);
        return spec::bandPower(s, 1500.0, 20000.0) / spec::bandPower(s, 20.0, 20000.0);
    };
    const double brighter = brightness(1.0f) / brightness(90.0f / 127.0f);
    CHECK_MESSAGE(brighter >= 1.5, "an accented note must be brighter: its share of energy above 1.5 kHz is only " << brighter
                                                                                                                << "x a plain "
                                                                                                                   "note's (accent "
                                                                                                                   "adds 2 x accent "
                                                                                                                   "octaves of "
                                                                                                                   "filter envelope)");

    Voice b(spec::subPatch());
    b.voice.noteOn(45, 0.7f);
    b.voice.setPitchBend(1.0f);
    b.run(4800);
    CHECK_MESSAGE(near(b.voice.currentHz(), 110.0 * std::exp2(2.0 / 12.0), 0.1),
                  "full up bend with bend_range 2 must raise note 45 two semitones (123.47 Hz); currentHz() is "
                      << b.voice.currentHz());
    const auto bent = b.run(48000);
    CHECK_MESSAGE(near(spec::zeroCrossingHz(bent, 0, bent.size(), kRate), 55.0 * std::exp2(2.0 / 12.0), 0.25),
                  "the sub follows the bend");
    b.voice.setPitchBend(-1.0f);
    b.run(480);
    CHECK_MESSAGE(near(b.voice.currentHz(), 110.0 * std::exp2(-2.0 / 12.0), 0.1),
                  "full down bend must lower note 45 two semitones (98.0 Hz); currentHz() is " << b.voice.currentHz());
}

TEST_CASE("P1.14 level changes are smoothed: no click, and settled within 50 ms") {
    auto p = spec::subPatch();
    spec::set(p, "sub_level", 0);
    Voice v(p);
    v.voice.noteOn(45, 0.7f);
    // The sub restarts at phase 0 on a note from idle, so sample 8945 is a peak of its 55 Hz sine (cycle 10.25).
    const auto before = v.run(8945);
    v.set("sub_level", 1.0f);
    const auto out = v.run(4800);
    double jump = std::fabs(out[0] - before.back()); // the step across the change itself
    for (std::size_t n = 1; n < 2400; ++n)
        jump = std::max(jump, static_cast<double>(std::fabs(out[n] - out[n - 1])));
    CHECK_MESSAGE(jump <= 0.02, "sub_level stepping from 0 to 1 at a peak of the sine jumped the output by " << jump
                                                                                                             << " in one sample: "
                                                                                                                "smooth level "
                                                                                                                "changes over a "
                                                                                                                "few ms");
    const double settled = spec::rms(out, 2400, 4800);
    CHECK_MESSAGE(settled >= 0.98 / std::sqrt(2.0), "50 ms after sub_level moved to 1 the RMS is " << settled
                                                                                                  << "; the smoothing must "
                                                                                                     "settle within 50 ms "
                                                                                                     "(want at least 0.693)");
}

TEST_CASE("P1.15 every note from silence starts the same way, and notes join without clicks") {
    auto p = spec::subPatch();
    spec::set(p, "osc_level", 0.3f);
    spec::set(p, "amp_release", 20);
    Voice v(p);
    v.voice.noteOn(45, 0.7f);
    const auto a = v.run(4800);
    v.voice.noteOff(45);
    v.run(48000);
    REQUIRE_MESSAGE(!v.voice.isActive(), "the voice must be idle 1 s after a 20 ms release");
    v.voice.noteOn(45, 0.7f);
    const auto b = v.run(4800);
    double difference = 0;
    for (std::size_t n = 0; n < 480; ++n)
        difference = std::max(difference, static_cast<double>(std::fabs(a[n] - b[n])));
    CHECK_MESSAGE(difference <= 1e-4, "two notes from silence differ by " << difference
                                                                          << " in their first 10 ms: reset the oscillators "
                                                                             "(and the sub) to phase 0 on a note from idle");
    CHECK_MESSAGE(std::fabs(a[0]) <= 0.05, "a note from silence starts with a click (first sample " << a[0] << ")");

    // A legato change of note keeps the phases running: no jump bigger than the waveform itself makes.
    auto s = spec::subPatch();
    Voice w(s);
    w.voice.noteOn(45, 0.7f);
    const auto steady = w.run(24000);
    w.voice.noteOn(52, 0.7f);
    const auto joined = w.run(4800);
    double normal = 0, join = 0;
    for (std::size_t n = 12000; n < 24000; ++n)
        normal = std::max(normal, static_cast<double>(std::fabs(steady[n] - steady[n - 1])));
    join = std::fabs(joined[0] - steady.back());
    for (std::size_t n = 1; n < 480; ++n)
        join = std::max(join, static_cast<double>(std::fabs(joined[n] - joined[n - 1])));
    CHECK_MESSAGE(join <= 2.5 * normal, "a legato note change jumped the sub by " << join << " (a steady 55 Hz sine moves at most "
                                                                                     << normal << " per sample): keep the phase "
                                                                                                  "running on legato notes");
}
