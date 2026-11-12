// SPDX-License-Identifier: AGPL-3.0-only
// Phase 5 spec tests: the factory presets (docs/specs/phase-5-sound.md, checks P5.15 to P5.22).
// spec-needs: core/include/undertow/presets.h
// spec-target: undertow_render

#include "doctest.h"

#include "spec_engine.h"
#include "spec_paths.h"
#include "spec_sound.h"
#include "undertow/engine.h"
#include "undertow/pattern.h"
#include "undertow/presets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr double kRate = 48000.0;
constexpr double kBpm = 130.0;
using namespace undertow::param;

// The spec's table: each preset's values that differ from the defaults.
struct Expected {
    const char* name;
    const char* slug;
    std::vector<std::pair<int, float>> settings;
};

std::vector<std::pair<int, float>> steps(std::vector<std::pair<int, float>> s, const std::vector<float>& velocities) {
    for (int i = 0; i < 8; ++i)
        s.emplace_back(Step1Vel + i, velocities[static_cast<std::size_t>(i)]);
    return s;
}

const std::vector<Expected>& table() {
    static const std::vector<float> roll{0, 0.35f, 0.7f, 0.5f, 0, 0.9f, 0.4f, 1.0f};
    static const std::vector<float> press{0, 0.55f, 1.0f, 0.6f, 0, 0.7f, 0.9f, 0.5f};
    static const std::vector<Expected> t{
        {"Init", "init", {}},
        {"Roll", "roll",
         steps({{VcfCutoff, 130}, {VcfEnv, 2.4f}, {PitchEnv, 6}, {PitchDecay, 60}, {VcaDecay, 330}, {SeqDrift, 0.6f}},
               roll)},
        {"Hold", "hold",
         steps({{VcfCutoff, 130}, {VcfEnv, 2.4f}, {PitchEnv, 6}, {PitchDecay, 60}, {VcfDecay, 1400}, {VcaAttack, 1},
                {VcaDecay, 2800}, {SeqDrift, 0.6f}, {MasterGain, -18.5f}},
               roll)},
        {"Drift", "drift",
         steps({{Vco1Wave, 1}, {Vco1Level, 0.4f}, {Vco2Wave, 0}, {Vco2Level, 0.35f}, {VcoFm, 0.3f}, {NoiseLevel, 0.08f},
                {SubLevel, 0.85f}, {PitchEnv, 9}, {PitchDecay, 32}, {VcfCutoff, 110}, {VcfRes, 0.44f}, {VcfDrive, 0.49f},
                {VcfEnv, 2.6f}, {VcfDecay, 184}, {VcaDecay, 276}, {SeqDrift, 0.9f}, {MasterGain, -9.5f}},
               press)},
        {"Rumble", "rumble",
         steps({{Vco1Level, 0.15f}, {Vco2Level, 0.1f}, {NoiseLevel, 0}, {SubLevel, 0.6f}, {VcfCutoff, 90}, {VcfEnv, 1.2f},
                {VcaDecay, 600}, {SeqRate, 3}, {SeqDrift, 0.3f}, {RumbleLevel, 1}, {RumbleSend, 0.4f}, {RumbleKick, 0.8f},
                {RumbleDecay, 6}, {RumbleTone, 90}, {RumbleDuck, 0.9f}, {MasterGain, -13}},
               {0, 0.5f, 0, 0.35f, 0, 0.6f, 0, 0.3f})},
        {"Pressure", "pressure",
         steps({{Vco1Wave, 1}, {Vco1Level, 0.4f}, {Vco2Wave, 0}, {Vco2Level, 0.35f}, {VcoFm, 0.3f}, {NoiseLevel, 0.08f},
                {SubLevel, 0.85f}, {PitchEnv, 9}, {PitchDecay, 32}, {VcfCutoff, 115}, {VcfRes, 0.48f},
                {VcfDrive, 0.875f}, {VcfEnv, 2.6f}, {VcfDecay, 230}, {VcaDecay, 390}, {SeqDrift, 0.5f},
                {MasterGain, -7}},
               press)},
        {"Offbeat", "offbeat",
         steps({{PitchEnv, 5}, {PitchDecay, 50}, {VcfCutoff, 120}, {VcaDecay, 250}, {SeqRate, 3}, {SeqDrift, 0.4f},
                {MasterGain, -8}},
               {0, 1.0f, 0, 0.8f, 0, 0.9f, 0, 0.7f})},
        {"Deep", "deep",
         steps({{Vco2Level, 0.2f}, {VcfCutoff, 100}, {VcfEnv, 1.5f}, {VcaDecay, 375}, {SeqDrift, 0.5f}, {SeqRoot, 40},
                {MasterGain, -10}},
               {0, 0.4f, 0.7f, 1.0f, 0, 0.5f, 0.8f, 0.3f})},
    };
    return t;
}

spec::Params preset(int index) {
    spec::Params p{};
    REQUIRE(undertow::factoryPreset(index, p.data()));
    return p;
}

constexpr double kStepBeats[8] = {4, 2, 1, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0};
constexpr int kPerBeat[8] = {1, 1, 1, 2, 4, 8, 3, 6};

bool rolling(const spec::Params& p) { return p[VcaDecay] <= 500.0f; }
bool held(const spec::Params& p) { return p[VcaDecay] >= 2000.0f; }

struct Played {
    spec::Signal out;
    std::vector<std::pair<long long, undertow::Hit>> hits;
};

// `bars` bars at 130 BPM on the preset's own root (seq_root held from the first sample), in blocks of 512, with the
// rumble as the preset has it or off.
Played play(spec::Params p, int bars, bool rumble = true) {
    if (!rumble) {
        spec::set(p, RumbleLevel, 0);
        spec::set(p, RumbleKick, 0);
        spec::set(p, RumbleSend, 0);
    }
    undertow::Engine e;
    e.prepare(kRate, 512);
    spec::apply(e, p);
    e.reset();
    Played r;
    const auto total = static_cast<std::size_t>(bars * 4 * std::llround(60.0 / kBpm * kRate));
    r.out.resize(total);
    std::vector<float> right(512);
    undertow::Hit block[undertow::kMaxHits];
    undertow::Transport t{true, true, kBpm, 0.0};
    const undertow::MidiEvent key = spec::noteOn(0, static_cast<int>(p[SeqRoot]));
    for (std::size_t s = 0; s < total; s += 512) {
        const int n = static_cast<int>(std::min<std::size_t>(512, total - s));
        t.ppq = static_cast<double>(s) * kBpm / (60.0 * kRate);
        e.process(r.out.data() + s, right.data(), n, &key, s == 0 ? 1 : 0, t);
        const int count = e.hits(block, undertow::kMaxHits);
        for (int k = 0; k < count; ++k)
            r.hits.emplace_back(static_cast<long long>(s) + block[k].sampleOffset, block[k]);
    }
    return r;
}

double db10(double share) { return 10.0 * std::log10(std::max(share, 1e-30)); }

} // namespace

TEST_CASE("P5.15 the factory presets: Init, then Roll, Hold, Drift, Rumble, Pressure, Offbeat and Deep, at the "
          "spec's values") {
    const auto& t = table();
    REQUIRE_MESSAGE(undertow::numFactoryPresets() == static_cast<int>(t.size()),
                    "there must be " << t.size() << " factory presets; there are " << undertow::numFactoryPresets());
    for (std::size_t i = 0; i < t.size(); ++i) {
        const auto p = preset(static_cast<int>(i));
        INFO("preset " << i << ", " << t[i].name);
        CHECK_MESSAGE(std::string(undertow::factoryPresetName(static_cast<int>(i))) == t[i].name, "its name");
        CHECK_MESSAGE(std::string(undertow::factoryPresetSlug(static_cast<int>(i))) == t[i].slug, "its slug");
        CHECK_MESSAGE(undertow::findFactoryPreset(t[i].slug) == static_cast<int>(i), "findFactoryPreset finds it");
        auto want = spec::defaults();
        for (const auto& [index, value] : t[i].settings)
            spec::set(want, index, value);
        for (int k = 0; k < undertow::kNumParams; ++k)
            CHECK_MESSAGE(p[static_cast<std::size_t>(k)] == doctest::Approx(want[static_cast<std::size_t>(k)]).epsilon(1e-6),
                          "`" << undertow::paramSpec(k).id << "` is " << p[static_cast<std::size_t>(k)] << "; the table says "
                              << want[static_cast<std::size_t>(k)]);
    }
    // Roll and Hold compare short and long decays: the same pattern and pitches, only the decays, the attack and the
    // master level that keeps them equally loud differ.
    const auto roll = preset(1), hold = preset(2);
    for (int k = 0; k < undertow::kNumParams; ++k) {
        if (k == PitchDecay || k == VcfDecay || k == VcaDecay || k == VcaAttack || k == MasterGain)
            continue;
        CHECK_MESSAGE(roll[static_cast<std::size_t>(k)] == hold[static_cast<std::size_t>(k)],
                      "Roll and Hold differ in `" << undertow::paramSpec(k).id << "`: they share everything but the "
                                                    "decays, the attack and the master level");
    }
}

TEST_CASE("P5.16 never acid: resonance at most 0.6, the thud at most +9 on fast lines, and no rolling preset hits on "
          "the kick's beats at any drift") {
    for (int i = 0; i < undertow::numFactoryPresets(); ++i) {
        const auto p = preset(i);
        INFO("preset " << undertow::factoryPresetName(i));
        CHECK_MESSAGE(p[VcfRes] <= 0.6f, "resonance " << p[VcfRes] << ": no preset raises the peak past +5 dB (0.6)");
        const int rate = static_cast<int>(p[SeqRate]);
        if (kStepBeats[rate] <= 0.25)
            CHECK_MESSAGE(p[PitchEnv] <= 9.0f, "a thud of " << p[PitchEnv]
                                                           << " st on a 1/16 or faster line: at most +9, or its "
                                                              "sweeps push the line past 150 Hz");
        if (!rolling(p))
            continue;
        auto wild = p;
        spec::set(wild, SeqDrift, 1.0f);
        const auto r = play(wild, 32, false);
        const double stepSamples = kStepBeats[rate] * 60.0 / kBpm * kRate;
        int onBeat = 0;
        for (const auto& [at, hit] : r.hits) {
            const auto count = std::llround(static_cast<double>(at) / stepSamples);
            onBeat += count % kPerBeat[rate] == 0 ? 1 : 0;
        }
        CHECK_MESSAGE(onBeat == 0, onBeat << " hits started on a whole beat in 32 bars at drift 1.0: a rolling preset "
                                             "leaves the kick's beats empty");
    }
}

TEST_CASE("P5.17 every preset keeps its energy between 30 and 150 Hz and stays dark (the PRD's range, at the "
          "output)") {
    for (int i = 0; i < undertow::numFactoryPresets(); ++i) {
        const auto p = preset(i);
        const auto voice = play(p, 8, false).out;
        const auto l = spec::ltas(voice, kRate);
        INFO("preset " << undertow::factoryPresetName(i) << ", the voice alone, 8 bars on its own root");
        CHECK_MESSAGE(spec::share(l, 0, 150) >= 0.90, 100 * spec::share(l, 0, 150) << "% below 150 Hz; want 90%");
        CHECK_MESSAGE(spec::share(l, 0, 300) >= 0.985, 100 * spec::share(l, 0, 300) << "% below 300 Hz; want 98.5%");
        CHECK_MESSAGE(spec::share(l, 30, 150) >= 0.88, 100 * spec::share(l, 30, 150) << "% from 30 to 150 Hz; want 88%");
        CHECK_MESSAGE(spec::share(l, 0, 20) <= 0.0025, 100 * spec::share(l, 0, 20) << "% below 20 Hz; want 0.25% at "
                                                                                      "most");
        CHECK_MESSAGE(spec::share(l, 0, 30) <= 0.02, 100 * spec::share(l, 0, 30) << "% below 30 Hz; want 2% at most");
        CHECK_MESSAGE(spec::centroidHz(l) <= 90.0, "the centroid is " << spec::centroidHz(l) << " Hz; want 90 Hz");
        CHECK_MESSAGE(db10(spec::share(l, 500, 24001)) <= -25.0, "above 500 Hz " << db10(spec::share(l, 500, 24001))
                                                                                  << " dB; want -25 dB");
        CHECK_MESSAGE(db10(spec::share(l, 1000, 24001)) <= -40.0, "above 1 kHz " << db10(spec::share(l, 1000, 24001))
                                                                                  << " dB; want -40 dB");
        CHECK_MESSAGE(db10(spec::share(l, 2000, 24001)) <= -55.0, "above 2 kHz " << db10(spec::share(l, 2000, 24001))
                                                                                  << " dB; want -55 dB");
        const double body = db10(spec::share(l, 150, 500));
        CHECK_MESSAGE(body <= -11.0, "150 to 500 Hz holds " << body << " dB; want 8% (-11 dB) at most");
        if (p[RumbleLevel] < 0.8f) // a bed led by the rumble keeps its voice dark on purpose
            CHECK_MESSAGE(body >= -20.0, "150 to 500 Hz holds " << body << " dB; want 1% (-20 dB) or more");
        CHECK_MESSAGE(spec::brightnessP95(voice, kRate) <= 150.0, "the brightest frames reach "
                                                                      << spec::brightnessP95(voice, kRate)
                                                                      << " Hz; want 150 Hz");
    }
}

TEST_CASE("P5.18 rolling presets roll and held ones hold") {
    for (int i = 0; i < undertow::numFactoryPresets(); ++i) {
        const auto p = preset(i);
        INFO("preset " << undertow::factoryPresetName(i));
        if (rolling(p)) {
            const auto r = play(p, 8, false);
            std::vector<std::size_t> onsets;
            for (const auto& h : r.hits)
                onsets.push_back(static_cast<std::size_t>(h.first));
            const double drop = spec::medianDropBeforeNextDb(r.out, onsets, kRate);
            CHECK_MESSAGE(drop >= 8.0, "a rolling preset falls only " << drop
                                                                     << " dB (median) before the next hit; want 8 dB");
        } else if (held(p)) {
            auto q = p;
            spec::set(q, SeqMode, 2);
            spec::set(q, RumbleLevel, 0);
            spec::set(q, RumbleKick, 0);
            spec::set(q, RumbleSend, 0);
            undertow::Engine e;
            e.prepare(kRate, 512);
            spec::apply(e, q);
            e.reset();
            const auto one = spec::render(e, 48000, {spec::noteOn(0, 43, 127)}, {true, true, kBpm, 0.0}).left;
            const auto env = spec::envelopeDb(one, kRate, 0.020);
            const auto peak = std::max_element(env.begin(), env.begin() + 24000);
            const double fall = *peak - *(peak + static_cast<long>(0.115 * kRate));
            CHECK_MESSAGE(fall <= 3.0, "a held preset's note falls " << fall << " dB 115 ms after its peak; want 3 dB "
                                                                                "at most");
        }
    }
}

TEST_CASE("P5.19 every preset plays within 2 LU of Init, its true peaks at most -1 dBTP") {
    const double init = spec::integratedLufs(play(preset(0), 8).out);
    for (int i = 0; i < undertow::numFactoryPresets(); ++i) {
        const auto out = play(preset(i), 8).out;
        const double lufs = spec::integratedLufs(out), peak = spec::truePeakDbtp(out);
        INFO("preset " << undertow::factoryPresetName(i));
        CHECK_MESSAGE(std::fabs(lufs - init) <= 2.0, "it plays at " << lufs << " LUFS, " << lufs - init
                                                                    << " LU from Init; want within 2 LU: set its "
                                                                       "master_gain");
        CHECK_MESSAGE(peak <= -1.0, "its true peak is " << peak << " dBTP; want -1 dBTP at most");
    }
}

TEST_CASE("P5.20 under the kick: rolling presets repeat exactly, and held ones keep the kick's tail from digging holes") {
    for (int i = 0; i < undertow::numFactoryPresets(); ++i) {
        auto p = preset(i);
        INFO("preset " << undertow::factoryPresetName(i));
        if (rolling(p)) {
            spec::set(p, SeqDrift, 0);
            const auto voice = play(p, 8, false).out;
            const auto pass = static_cast<std::size_t>(
                std::lround(8 * kStepBeats[static_cast<int>(p[SeqRate])] * 60.0 / kBpm * kRate));
            const auto bar = static_cast<std::size_t>(4 * std::llround(60.0 / kBpm * kRate));
            const int cycles = static_cast<int>((voice.size() - bar) / pass);
            const double spread = spec::repeatSpreadDb(voice, bar, pass, cycles, kRate);
            CHECK_MESSAGE(spread <= 0.5, "at drift 0 the pattern's repeats differ by " << spread
                                                                                       << " dB; want 0.5 dB at most, "
                                                                                          "so it meets the kick the "
                                                                                          "same way every time");
        } else if (held(p)) {
            const auto voice = play(p, 8, false).out;
            std::vector<std::size_t> kicks;
            const auto mix = spec::underTheKick(voice, kBpm, kRate, -8.0, &kicks);
            const auto withKick = spec::postKickValleysDb(mix, kicks, kRate);
            const auto alone = spec::postKickValleysDb(voice, kicks, kRate);
            CHECK_MESSAGE(withKick.first - alone.first <= 6.0, "the kick deepens the median valley by "
                                                                   << withKick.first - alone.first
                                                                   << " dB; want 6 dB at most");
            CHECK_MESSAGE(withKick.second <= 18.0, "the worst valley under the kick is " << withKick.second
                                                                                         << " dB; want 18 dB at most");
        }
    }
}

TEST_CASE("P5.21 the Rumble preset is a bed led by its rumble, and dark") {
    const int index = undertow::findFactoryPreset("rumble");
    REQUIRE(index > 0);
    auto p = preset(index);
    const auto whole = play(p, 9).out;
    const spec::Signal out(whole.begin() + static_cast<long>(4 * std::llround(60.0 / kBpm * kRate)), whole.end());
    auto voiceOnly = p;
    spec::set(voiceOnly, RumbleLevel, 0);
    spec::set(voiceOnly, RumbleKick, 0);
    spec::set(voiceOnly, RumbleSend, 0);
    const double voice = spec::rms(play(voiceOnly, 8).out), all = spec::rms(out);
    CHECK_MESSAGE(spec::toDb(all / voice) >= 3.0, "with its rumble the Rumble preset is only "
                                                      << spec::toDb(all / voice)
                                                      << " dB louder than its voice alone; the rumble leads (3 dB)");
    const auto l = spec::ltas(out, kRate);
    CHECK_MESSAGE(spec::share(l, 0, 150) >= 0.97, 100 * spec::share(l, 0, 150) << "% below 150 Hz; want 97%");
    CHECK_MESSAGE(spec::centroidHz(l) <= 85.0, "its centroid is " << spec::centroidHz(l) << " Hz; want 85 Hz");
    CHECK_MESSAGE(db10(spec::share(l, 500, 24001)) <= -40.0, "above 500 Hz " << db10(spec::share(l, 500, 24001))
                                                                              << " dB; want -40 dB");
}

TEST_CASE("P5.22 the renderer holds each preset's own root: Deep renders on E2") {
    REQUIRE_MESSAGE(std::string(SPEC_TARGET_UNDERTOW_RENDER).size() > 0, "there is no undertow_render target");
    std::system(("mkdir -p '" + spec::sourceDir() + "/build/spec-render'").c_str());
    const std::string program = spec::built(SPEC_TARGET_UNDERTOW_RENDER_IN_BUILD, SPEC_TARGET_UNDERTOW_RENDER);
    const std::string command = "cd '" + spec::sourceDir() + "' && '" + program +
                                "' --preset deep --bars 1 --out build/spec-render/deep.wav --hits "
                                "build/spec-render/deep.json > /dev/null 2>&1";
    REQUIRE(std::system(command.c_str()) == 0);
    std::ifstream in(spec::sourceDir() + "/build/spec-render/deep.json");
    std::stringstream s;
    s << in.rdbuf();
    const std::string json = s.str();
    const auto at = json.find("\"root\"");
    REQUIRE(at != std::string::npos);
    CHECK_MESSAGE(std::strtod(json.c_str() + json.find(':', at) + 1, nullptr) == 40.0,
                  "without --note the renderer holds Deep's seq_root, 40 (E2)");
}
