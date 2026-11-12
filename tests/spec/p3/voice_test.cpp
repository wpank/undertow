// SPDX-License-Identifier: AGPL-3.0-only
// Phase 3 spec tests: the voice (docs/specs/phase-3-voice.md, checks P3.06 to P3.14).
// spec-needs: core/include/undertow/voice.h

#include "doctest.h"

#include "spec_analysis.h"
#include "spec_engine.h"
#include "spec_sound.h"
#include "undertow/voice.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr double kRate = 48000.0;
using namespace undertow::param;

// One source at a time through an open, clean ladder: no thud, no filter decay, a level decay long enough to hold.
spec::Params plain(int source) {
    auto p = spec::defaults();
    for (int i : {Vco1Level, Vco2Level, NoiseLevel, SubLevel})
        spec::set(p, i, i == source ? 0.5f : 0.0f);
    spec::set(p, Vco1Wave, 0);
    spec::set(p, Vco2Wave, 0);
    spec::set(p, Vco2Tune, 0);
    spec::set(p, VcoFm, 0);
    spec::set(p, PitchEnv, 0);
    spec::set(p, VcfCutoff, 8000);
    spec::set(p, VcfRes, 0);
    spec::set(p, VcfDrive, 0);
    spec::set(p, VcfEnv, 0);
    spec::set(p, VcaAttack, 0);
    spec::set(p, VcaDecay, 20000);
    return p;
}

undertow::Voice make(const spec::Params& p) {
    undertow::Voice v;
    v.prepare(kRate);
    v.setParams(p.data());
    v.reset();
    v.setParams(p.data());
    return v;
}

spec::Signal run(undertow::Voice& v, std::size_t samples) {
    spec::Signal out(samples);
    for (auto& x : out)
        x = v.process();
    return out;
}

double semitones(double hz, double reference) { return 12.0 * std::log2(hz / reference); }

// The default patch played as its pattern would be: a hit on every 1/16 at 130 BPM whose velocity is not 0, rooted on
// seq_root, for `bars` bars. Returns the voice's output; `onsets` gets the hits' samples.
spec::Signal rollingLine(const spec::Params& p, int bars, std::vector<std::size_t>* onsets = nullptr) {
    auto v = make(p);
    const double step = 0.25 * 60.0 / 130.0 * kRate;
    const int steps = 16 * bars;
    spec::Signal out(static_cast<std::size_t>(std::lround(steps * step)));
    int next = 0;
    for (std::size_t n = 0; n < out.size(); ++n) {
        while (next < steps && static_cast<std::size_t>(std::lround(next * step)) == n) {
            const float velocity = p[static_cast<std::size_t>(Step1Vel + next % undertow::kSteps)];
            if (velocity > 0.0f) {
                v.hit(static_cast<int>(p[SeqRoot]) + static_cast<int>(p[static_cast<std::size_t>(Step1Pitch + next % undertow::kSteps)]),
                      velocity);
                if (onsets != nullptr)
                    onsets->push_back(n);
            }
            ++next;
        }
        out[n] = v.process();
    }
    return out;
}

} // namespace

TEST_CASE("P3.06 a hit plays VCO 1 at its note, VCO 2 vco2_tune semitones from it, and the sub at or an octave under") {
    struct Case {
        int source;
        float tune;
        int subOctave;
        double hz;
        const char* what;
    };
    for (const Case c : {Case{Vco1Level, 0, 0, 220.0, "VCO 1 at note 57"},
                         Case{Vco2Level, -12, 0, 110.0, "VCO 2 at -12 semitones"},
                         Case{Vco2Level, 7, 0, 220.0 * std::pow(2.0, 7.0 / 12.0), "VCO 2 at +7 semitones"},
                         Case{SubLevel, 0, 1, 110.0, "the sub at -1 oct"},
                         Case{SubLevel, 0, 0, 220.0, "the sub at 0 oct"}}) {
        auto p = plain(c.source);
        spec::set(p, Vco2Tune, c.tune);
        spec::set(p, SubOctave, static_cast<float>(c.subOctave));
        auto v = make(p);
        v.hit(57, 1.0f);
        const auto out = run(v, static_cast<std::size_t>(kRate));
        const double got = spec::zeroCrossingHz(out, 4800, out.size(), kRate);
        INFO(std::string(c.what));
        CHECK_MESSAGE(std::fabs(got / c.hz - 1.0) <= 0.002, "it sounds " << got << " Hz; want " << c.hz);
        CHECK_MESSAGE(v.note() == 57, "note() is " << v.note() << " while the hit sounds; want 57");
        CHECK_MESSAGE(v.noteHz() == doctest::Approx(220.0).epsilon(1e-6), "noteHz() is " << v.noteHz()
                                                                                         << "; note 57 is 220 Hz");
    }
    // The noise joins the mixer: alone through the open ladder it is a broad hiss.
    auto noisy = make(plain(NoiseLevel));
    noisy.hit(57, 1.0f);
    const auto hiss = run(noisy, static_cast<std::size_t>(0.5 * kRate));
    CHECK_MESSAGE(spec::rms(hiss, 4800) >= 0.05, "noise_level 0.5 alone gave an RMS of " << spec::rms(hiss, 4800)
                                                                                         << ": the noise joins the "
                                                                                            "mixer");
    // VCO 1's triangle modulates VCO 2's frequency, f2 (1 + vco_fm tri1): with VCO 2 a fifth up, its cycles vary and
    // their mean frequency stays.
    auto fmPatch = plain(Vco2Level);
    spec::set(fmPatch, Vco2Tune, 7.0f);
    spec::set(fmPatch, VcoFm, 0.5f);
    auto fm = make(fmPatch);
    fm.hit(57, 1.0f);
    const auto swept = run(fm, static_cast<std::size_t>(kRate));
    const auto crossings = spec::risingZeroCrossings(swept, 4800, swept.size());
    double shortest = 1e9, longest = 0;
    for (std::size_t i = 1; i < crossings.size(); ++i) {
        shortest = std::min(shortest, crossings[i] - crossings[i - 1]);
        longest = std::max(longest, crossings[i] - crossings[i - 1]);
    }
    CHECK_MESSAGE(longest / shortest >= 1.2, "at vco_fm 0.5 VCO 2's cycles vary only from " << shortest << " to "
                                                                                          << longest
                                                                                          << " samples: VCO 1's "
                                                                                             "triangle modulates "
                                                                                             "VCO 2's frequency");
    const double mean = spec::zeroCrossingHz(swept, 4800, swept.size(), kRate);
    const double fifth = 220.0 * std::pow(2.0, 7.0 / 12.0);
    CHECK_MESSAGE(std::fabs(mean / fifth - 1.0) <= 0.01, "under linear FM VCO 2's mean frequency must stay "
                                                             << fifth << " Hz; it is " << mean);
}

TEST_CASE("P3.07 the thud: each hit starts pitch_env semitones high and falls to 1% of that in pitch_decay") {
    auto p = plain(SubLevel);
    spec::set(p, PitchEnv, 12);
    spec::set(p, PitchDecay, 75);
    auto v = make(p);
    v.hit(57, 1.0f);
    (void)v.process();
    const double start = semitones(v.vco1Hz(), 220.0);
    CHECK_MESSAGE(std::fabs(start - 12.0) <= 0.05, "the first sample of a hit plays " << start
                                                                                      << " semitones above the note; "
                                                                                         "the thud starts pitch_env "
                                                                                         "(12) above it");
    double reached = -1;
    for (std::size_t n = 1; n < static_cast<std::size_t>(0.3 * kRate); ++n) {
        (void)v.process();
        if (reached < 0 && semitones(v.vco1Hz(), 220.0) <= 0.12)
            reached = static_cast<double>(n + 1) / kRate * 1000.0;
    }
    CHECK_MESSAGE(std::fabs(reached / 75.0 - 1.0) <= 0.05, "the thud fell to 1% of its 12 semitones (0.12) after "
                                                               << reached << " ms; pitch_decay is 75 ms (within 5%)");
    CHECK_MESSAGE(std::fabs(semitones(v.vco1Hz(), 220.0)) <= 0.001, "300 ms later VCO 1 must play the note itself");
}

TEST_CASE("P3.08 the filter decay: the cutoff rises vcf_env x velocity octaves above vcf_cutoff and falls back") {
    for (const float velocity : {1.0f, 0.5f}) {
        auto p = plain(Vco1Level);
        spec::set(p, VcfCutoff, 200);
        spec::set(p, VcfEnv, 3);
        spec::set(p, VcfDecay, 300);
        auto v = make(p);
        v.hit(45, velocity);
        std::vector<double> octaves;
        for (std::size_t n = 0; n < static_cast<std::size_t>(0.6 * kRate); ++n) {
            (void)v.process();
            octaves.push_back(std::log2(v.cutoffHz() / 200.0));
        }
        const auto top = std::max_element(octaves.begin(), octaves.end());
        INFO("velocity " << velocity);
        CHECK_MESSAGE(std::fabs(*top - 3.0 * velocity) <= 0.02, "the cutoff rose " << *top
                                                                                  << " octaves; vcf_env x velocity "
                                                                                     "is "
                                                                                  << 3.0 * velocity);
        double reached = -1;
        for (auto it = top; it != octaves.end(); ++it)
            if (*it <= 0.01 * 3.0 * velocity) {
                reached = static_cast<double>(it - top) / kRate * 1000.0;
                break;
            }
        CHECK_MESSAGE(std::fabs(reached / 300.0 - 1.0) <= 0.05, "from its peak the cutoff took "
                                                                    << reached
                                                                    << " ms to fall back to within 1% of its swing; "
                                                                       "vcf_decay is 300 ms (within 5%)");
        CHECK_MESSAGE(octaves.back() == doctest::Approx(0.0).epsilon(1e-3), "the cutoff settles on vcf_cutoff");
    }
}

TEST_CASE("P3.08 the voice's resonance and drive reach the ladder") {
    // Resonance: the noise through a 500 Hz cutoff gains a peak just under it.
    auto ring = [](float resonance) {
        auto p = plain(NoiseLevel);
        spec::set(p, VcfCutoff, 500);
        spec::set(p, VcfRes, resonance);
        auto v = make(p);
        v.hit(57, 1.0f);
        const auto out = run(v, static_cast<std::size_t>(2 * kRate));
        const auto l = spec::ltas(out, kRate);
        return 10.0 * std::log10(spec::share(l, 380, 500) / 120.0 / (spec::share(l, 40, 160) / 120.0));
    };
    CHECK_MESSAGE(ring(1.0f) - ring(0.0f) >= 6.0, "vcf_res 1 raised the noise just under the 500 Hz cutoff by only "
                                                     << ring(1.0f) - ring(0.0f) << " dB: pass resonance to the ladder");
    // Drive: the sub alone at a 220 Hz cutoff gains harmonics.
    auto warm = [](float drive) {
        auto p = plain(SubLevel);
        spec::set(p, SubLevel, 1.0f);
        spec::set(p, VcfCutoff, 220);
        spec::set(p, VcfDrive, drive);
        auto v = make(p);
        v.hit(57, 1.0f);
        const auto out = run(v, static_cast<std::size_t>(kRate));
        const auto sp = spec::spectrum(out, 4800, 1u << 15, kRate);
        return spec::toDb(spec::amplitudeAt(sp, 330.0) / spec::amplitudeAt(sp, 110.0));
    };
    CHECK_MESSAGE(warm(1.0f) - warm(0.0f) >= 10.0, "vcf_drive 1 raised the sub's 3rd harmonic by only "
                                                       << warm(1.0f) - warm(0.0f) << " dB over drive 0: pass the "
                                                                                     "drive to the ladder");
}

TEST_CASE("P3.09 the attack: Fast, the level in 1.2 ms and the filter in 0.6 ms; Slow, 100 ms and 500 ms (10 to 90%)") {
    for (const int slow : {0, 1}) {
        auto p = plain(Vco1Level);
        spec::set(p, VcaAttack, static_cast<float>(slow));
        spec::set(p, VcfCutoff, 200);
        spec::set(p, VcfEnv, 2);
        spec::set(p, VcfDecay, 10000);
        auto v = make(p);
        v.hit(45, 1.0f);
        std::vector<double> level, octaves;
        for (std::size_t n = 0; n < static_cast<std::size_t>(1.5 * kRate); ++n) {
            (void)v.process();
            level.push_back(v.level());
            octaves.push_back(std::log2(v.cutoffHz() / 200.0) / 2.0);
        }
        auto rise = [](const std::vector<double>& x, double top) {
            double t10 = -1, t90 = -1;
            for (std::size_t n = 0; n < x.size(); ++n) {
                if (t10 < 0 && x[n] >= 0.1 * top)
                    t10 = static_cast<double>(n);
                if (t90 < 0 && x[n] >= 0.9 * top)
                    t90 = static_cast<double>(n);
            }
            return (t90 - t10) / kRate * 1000.0;
        };
        const double levelRise = rise(level, *std::max_element(level.begin(), level.end()));
        const double filterRise = rise(octaves, *std::max_element(octaves.begin(), octaves.end()));
        // Phase 7 softened Fast from 1 ms for both (docs/specs/phase-7-ship.md).
        const double wantLevel = slow ? 100.0 : 1.2, wantFilter = slow ? 500.0 : 0.6;
        INFO(std::string(slow ? "Slow" : "Fast"));
        CHECK_MESSAGE(std::fabs(levelRise / wantLevel - 1.0) <= 0.1, "the level rose 10 to 90% in "
                                                                         << levelRise << " ms; want " << wantLevel
                                                                         << " ms");
        CHECK_MESSAGE(std::fabs(filterRise / wantFilter - 1.0) <= 0.1, "the filter rose 10 to 90% in "
                                                                           << filterRise << " ms; want " << wantFilter
                                                                           << " ms");
    }
}

TEST_CASE("P3.10 velocity sets the peak level and scales the filter's swing; 0 is a rest") {
    for (const float velocity : {1.0f, 0.5f, 0.25f}) {
        auto v = make(plain(SubLevel));
        v.hit(45, velocity);
        float peak = 0;
        for (int n = 0; n < 4800; ++n) {
            (void)v.process();
            peak = std::max(peak, v.level());
        }
        CHECK_MESSAGE(std::fabs(spec::toDb(peak / velocity)) <= 0.3, "at velocity " << velocity << " the VCA peaks at "
                                                                                     << peak
                                                                                     << "; the peak is the velocity");
    }
    // The default patch: a hit at velocity 0.5 is darker than one at 1, just after its attack (5 to 25 ms).
    auto bright = [](float velocity) {
        auto v = make(spec::defaults());
        v.hit(43, velocity);
        const auto out = run(v, static_cast<std::size_t>(0.5 * kRate));
        const auto high = spec::zeroPhaseHighpass(out, 200.0, kRate, 4);
        const std::size_t from = spec::ms(5), to = spec::ms(25);
        return 20.0 * std::log10(spec::rms(high, from, to) / spec::rms(out, from, to));
    };
    const double full = bright(1.0f), half = bright(0.5f);
    CHECK_MESSAGE(full - half >= 3.0, "5 to 25 ms into a hit at velocity 0.5 the power above 200 Hz is "
                                          << half << " dB re its total, against " << full
                                          << " dB at velocity 1: velocity scales the filter's swing, so the softer "
                                             "hit must be 3 dB darker or more");
    auto a = make(spec::defaults()), b = make(spec::defaults());
    a.hit(43, 1.0f);
    b.hit(43, 1.0f);
    run(a, 2400);
    run(b, 2400);
    b.hit(43, 0.0f);
    const auto withRest = run(b, 9600), without = run(a, 9600);
    CHECK_MESSAGE(withRest == without, "a hit at velocity 0 changed the sound: velocity 0 is a rest, and the ringing "
                                       "note carries on through it");
}

TEST_CASE("P3.11 a hit that finds the level under -20 dB restarts the oscillators; into a louder note they run on") {
    // A phrase that repeats: three hits, then a gap that takes the level to about -30 dB, under -20 dB. With the
    // restart each phrase starts the same way, so after the first two the output repeats exactly.
    auto p = spec::defaults();
    spec::set(p, NoiseLevel, 0);
    auto v = make(p);
    const std::size_t cycle = static_cast<std::size_t>(0.5 * kRate);
    const std::size_t hits[] = {0, spec::ms(115), spec::ms(230)};
    const float velocities[] = {1.0f, 0.5f, 0.8f};
    std::vector<spec::Signal> phrases;
    for (int k = 0; k < 4; ++k) {
        spec::Signal phrase(cycle);
        for (std::size_t n = 0; n < cycle; ++n) {
            for (int h = 0; h < 3; ++h)
                if (n == hits[h])
                    v.hit(43, velocities[h]);
            phrase[n] = v.process();
        }
        phrases.push_back(phrase);
    }
    double difference = 0;
    for (std::size_t n = 0; n < cycle; ++n)
        difference = std::max(difference, static_cast<double>(std::fabs(phrases[3][n] - phrases[2][n])));
    CHECK_MESSAGE(difference <= 1e-4, "the third and fourth phrases differ by up to "
                                          << difference
                                          << ": a hit that finds the level under -20 dB restarts VCO 1 and VCO 2 at "
                                             "phase 0, so a repeated phrase plays the same every time");

    // Into a ringing note (the level near 1) the oscillators run on: a triangle through an open ladder never jumps.
    // The second hit lands on the triangle's peak (phase 0.25 of 220 Hz, 2455 samples in), where a restart to phase 0
    // would drop it by half its swing.
    auto q = plain(Vco1Level);
    auto w = make(q);
    w.hit(57, 1.0f);
    const float last = run(w, 2455).back();
    w.hit(57, 1.0f);
    const auto after = run(w, 480);
    double jump = std::fabs(after.front() - last); // across the hit itself
    for (std::size_t n = 1; n < after.size(); ++n)
        jump = std::max(jump, static_cast<double>(std::fabs(after[n] - after[n - 1])));
    CHECK_MESSAGE(jump <= 0.02, "a hit into a ringing note made the output jump " << jump
                                                                                  << ": keep the oscillators running "
                                                                                     "when the level is above -20 dB");

    // And under -20 dB a hit plays exactly as one from silence.
    auto r = plain(Vco1Level);
    spec::set(r, VcaDecay, 30);
    auto x = make(r), y = make(r);
    x.hit(57, 1.0f);
    run(x, spec::ms(50) + 7);
    x.hit(57, 1.0f);
    y.hit(57, 1.0f);
    const auto restarted = run(x, 480), fresh = run(y, 480);
    double gap = 0;
    for (std::size_t n = 96; n < restarted.size(); ++n) // after the ladder's own memory of the note before (2 ms)
        gap = std::max(gap, static_cast<double>(std::fabs(restarted[n] - fresh[n])));
    CHECK_MESSAGE(gap <= 1e-3, "a hit that found the level at -66 dB differs from one from silence by up to "
                                   << gap << ": it must restart the oscillators at phase 0");
}

TEST_CASE("P3.12 a knob moved during a note never clicks: the levels and the filter glide over a few milliseconds") {
    auto p = plain(SubLevel);
    spec::set(p, SubLevel, 0.0f);
    auto v = make(p);
    v.hit(45, 1.0f);
    const float last = run(v, spec::ms(200) + 218).back(); // the change lands on the 55 Hz sub's peak
    spec::set(p, SubLevel, 1.0f);
    v.setParams(p.data());
    const auto out = run(v, spec::ms(100));
    double jump = std::fabs(out.front() - last); // across the change itself
    for (std::size_t n = 1; n < out.size(); ++n)
        jump = std::max(jump, static_cast<double>(std::fabs(out[n] - out[n - 1])));
    const double slope = 2.0 * 3.141592653589793 * 55.0 / kRate; // a full-scale 55 Hz sine's steepest move
    CHECK_MESSAGE(jump <= 2.0 * slope, "the sub level went 0 to 1 and the output moved " << jump
                                                                                        << " in one sample, where the "
                                                                                           "sine moves at most "
                                                                                        << slope
                                                                                        << ": smooth each level over "
                                                                                           "about 5 ms");
    CHECK_MESSAGE(spec::peakAbs(out, spec::ms(50)) >= 0.85, "50 ms after the change the sub must be at its new level");

    // The cutoff glides too: right after a jump from 100 Hz to 4 kHz it is still near 100 Hz, and 50 ms later there.
    auto q = plain(Vco1Level);
    spec::set(q, VcfCutoff, 100);
    auto w = make(q);
    w.hit(45, 1.0f);
    run(w, spec::ms(200));
    spec::set(q, VcfCutoff, 4000);
    w.setParams(q.data());
    (void)w.process();
    const double first = w.cutoffHz();
    run(w, spec::ms(50));
    const double settled = w.cutoffHz();
    CHECK_MESSAGE(first <= 110.0, "one sample after vcf_cutoff jumped from 100 Hz to 4 kHz the ladder's cutoff is "
                                      << first << " Hz: glide it over about 5 ms, in octaves");
    CHECK_MESSAGE(settled >= 3900.0, "50 ms after the change the cutoff is " << settled << " Hz; it must have reached "
                                                                                           "4 kHz");
}

TEST_CASE("P3.13 the default patch's rolling line keeps its energy between 30 and 150 Hz, and stays dark") {
    std::vector<std::size_t> onsets;
    const auto line = rollingLine(spec::defaults(), 8, &onsets);
    const auto l = spec::ltas(line, kRate);
    const double below150 = spec::share(l, 0, 150), below300 = spec::share(l, 0, 300),
                 band = spec::share(l, 30, 150), below30 = spec::share(l, 0, 30);
    CHECK_MESSAGE(below150 >= 0.90, (100 * below150) << "% of the power is below 150 Hz; want 90% or more");
    CHECK_MESSAGE(below300 >= 0.985, (100 * below300) << "% of the power is below 300 Hz; want 98.5% or more");
    CHECK_MESSAGE(band >= 0.88, (100 * band) << "% of the power is between 30 and 150 Hz; want 88% or more");
    // The PRD's 2% (section 1.1). Each hit's start leaves a little power below 30 Hz, more with phase 7's softer
    // attack (1.1% of the voice alone); the output stage's 20 Hz high-pass takes most of it (P5.17).
    CHECK_MESSAGE(below30 <= 0.02, (100 * below30) << "% of the power is below 30 Hz; want 2% at most");
    CHECK_MESSAGE(spec::centroidHz(l) <= 90.0, "the spectral centroid is " << spec::centroidHz(l)
                                                                           << " Hz; want 90 Hz at most");
    const double above500 = 10 * std::log10(spec::share(l, 500, 24001)),
                 above1k = 10 * std::log10(spec::share(l, 1000, 24001)),
                 above2k = 10 * std::log10(spec::share(l, 2000, 24001)),
                 body = 10 * std::log10(spec::share(l, 150, 500));
    CHECK_MESSAGE(above500 <= -25.0, "the band above 500 Hz is " << above500 << " dB re the total; want -25 dB or less");
    CHECK_MESSAGE(above1k <= -40.0, "the band above 1 kHz is " << above1k << " dB re the total; want -40 dB or less");
    CHECK_MESSAGE(above2k <= -55.0, "the band above 2 kHz is " << above2k << " dB re the total; want -55 dB or less");
    CHECK_MESSAGE(body >= -20.0, "150 to 500 Hz holds " << body
                                                        << " dB of the power; the drive must leave 1% (-20 dB) or "
                                                           "more, to be heard on small speakers");
    CHECK_MESSAGE(body <= -11.0, "150 to 500 Hz holds " << body << " dB of the power; want 8% (-11 dB) at most");
    const double bright = spec::brightnessP95(line, kRate);
    CHECK_MESSAGE(bright <= 150.0, "the brightest 5% of the line's frames have their centroid at " << bright
                                                                                                  << " Hz; want 150 Hz "
                                                                                                     "at most");
}

TEST_CASE("P3.14 short decays roll and long ones hold: one note, 115 ms after its peak") {
    // Rolling: the default patch as it is. Holding: a 2.8 s level decay, with the thud and the filter's swing off so
    // the decay alone is judged.
    for (const bool held : {false, true}) {
        auto p = spec::defaults();
        if (held) {
            spec::set(p, VcaDecay, 2800);
            spec::set(p, VcfEnv, 0);
            spec::set(p, PitchEnv, 0);
        }
        auto v = make(p);
        v.hit(43, 1.0f);
        const auto out = run(v, static_cast<std::size_t>(0.5 * kRate));
        const auto env = spec::envelopeDb(out, kRate);
        const auto peak = std::max_element(env.begin(), env.begin() + static_cast<long>(spec::ms(60)));
        const double drop = *peak - *(peak + static_cast<long>(spec::ms(115)));
        INFO(std::string(held ? "held (2.8 s)" : "rolling (the default 390 ms)") << ": 115 ms after its peak the note "
                                                                                    "is "
                                                                                 << drop << " dB down");
        if (!held)
            CHECK_MESSAGE(drop >= 10.0, "a rolling note falls at least 10 dB in the 115 ms after its peak, one 1/16 "
                                        "at 130 BPM");
        else
            CHECK_MESSAGE(drop <= 3.0, "a held note falls at most 3 dB in the 115 ms after its peak");
    }
}
