#include "undertow/presets.h"

#include <array>
#include <cstddef>
#include <string_view>

namespace undertow {
namespace {

struct PresetDef {
    const char* name;
    const char* slug;
    std::array<float, kNumParams> values;
    Pattern pattern;
};

std::array<float, kNumParams> makeDefaults() {
    std::array<float, kNumParams> values{};
    for (int i = 0; i < kNumParams; ++i) values[static_cast<std::size_t>(i)] = paramSpec(i).def;
    return values;
}

constexpr Pattern makePattern(const std::array<int, kSteps>& semi,
                             const std::array<bool, kSteps>& gate,
                             const std::array<bool, kSteps>& accent,
                             const std::array<bool, kSteps>& slide,
                             const std::array<float, kSteps>& probability) {
    Pattern pattern{};
    for (int i = 0; i < kSteps; ++i) {
        pattern.steps[static_cast<std::size_t>(i)].semitones = semi[static_cast<std::size_t>(i)];
        pattern.steps[static_cast<std::size_t>(i)].gate = gate[static_cast<std::size_t>(i)];
        pattern.steps[static_cast<std::size_t>(i)].accent = accent[static_cast<std::size_t>(i)];
        pattern.steps[static_cast<std::size_t>(i)].slide = slide[static_cast<std::size_t>(i)];
        pattern.steps[static_cast<std::size_t>(i)].probability = probability[static_cast<std::size_t>(i)];
    }
    return pattern;
}

#define ALL_TRUE16 std::array<bool, kSteps>{true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true}
#define ALL_FALSE16 std::array<bool, kSteps>{false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}
#define ALL_ONE16 std::array<float, kSteps>{1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f}

static const PresetDef kPresets[] = {
    {"SUB Pure Foundation", "sub-pure-foundation", [] {
        auto v = makeDefaults();
        v[param::OscWave] = 0; v[param::OscLevel] = 1.00f; v[param::OscOctave] = -2; v[param::SubLevel] = 1.00f;
        v[param::FilterCutoff] = 5200.0f; v[param::FilterRes] = 0.00f; v[param::FilterDrive] = 0.0f; v[param::FilterEnv] = 0.0f;
        v[param::AmpAttack] = 1.0f; v[param::AmpDecay] = 60.0f; v[param::AmpSustain] = 1.0f; v[param::AmpRelease] = 50.0f;
        v[param::FiltAttack] = 1.0f; v[param::FiltDecay] = 60.0f; v[param::FiltSustain] = 0.0f; v[param::FiltRelease] = 50.0f;
        v[param::GlideMs] = 0.0f; v[param::GlideMode] = 0; v[param::Accent] = 0.00f; v[param::Width] = 0.0f; v[param::MonoFreq] = 250.0f;
        v[param::MasterGain] = -14.5f; v[param::SeqOn] = 0; v[param::RumbleLevel] = 0.0f; v[param::RumbleSend] = 0.0f; v[param::RumbleDrive] = 0.0f; v[param::RumbleDuck] = 0.0f;
        return v;
    }(), makePattern(
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        ALL_TRUE16,
        ALL_FALSE16,
        ALL_FALSE16,
        ALL_ONE16
    )},
    {"SUB Low Pressure", "sub-low-pressure", [] {
        auto v = makeDefaults();
        v[param::OscWave] = 0; v[param::OscLevel] = 0.90f; v[param::OscOctave] = -2; v[param::SubLevel] = 1.00f;
        v[param::FilterCutoff] = 5200.0f; v[param::FilterRes] = 0.01f; v[param::FilterDrive] = 0.0f; v[param::FilterEnv] = 0.0f;
        v[param::AmpAttack] = 3.0f; v[param::AmpDecay] = 150.0f; v[param::AmpSustain] = 1.0f; v[param::AmpRelease] = 70.0f;
        v[param::FiltAttack] = 3.0f; v[param::FiltDecay] = 90.0f; v[param::FiltSustain] = 0.0f; v[param::FiltRelease] = 70.0f;
        v[param::GlideMs] = 14.0f; v[param::GlideMode] = 1; v[param::Accent] = 0.00f; v[param::Width] = 0.0f; v[param::MonoFreq] = 220.0f;
        v[param::MasterGain] = -17.0f; v[param::SeqOn] = 0; v[param::SatDrive] = 0.00f;
        return v;
    }(), makePattern(
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        ALL_TRUE16,
        ALL_FALSE16,
        ALL_FALSE16,
        ALL_ONE16
    )},
    {"SUB Kick Companion", "sub-kick-companion", [] {
        auto v = makeDefaults();
        v[param::OscWave] = 0; v[param::OscLevel] = 0.90f; v[param::OscOctave] = -2; v[param::SubLevel] = 1.00f;
        v[param::FilterCutoff] = 2600.0f; v[param::FilterRes] = 0.00f; v[param::FilterDrive] = 0.0f; v[param::FilterEnv] = 0.0f;
        v[param::AmpAttack] = 1.0f; v[param::AmpDecay] = 80.0f; v[param::AmpSustain] = 0.90f; v[param::AmpRelease] = 50.0f;
        v[param::FiltAttack] = 1.0f; v[param::FiltDecay] = 80.0f; v[param::FiltSustain] = 0.0f; v[param::FiltRelease] = 50.0f;
        v[param::Accent] = 0.00f; v[param::MasterGain] = -13.0f;
        return v;
    }(), makePattern(
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        ALL_TRUE16,
        ALL_FALSE16,
        ALL_FALSE16,
        ALL_ONE16
    )},
    {"BASS Hypnotic Roller", "bass-hypnotic-roller", [] {
        auto v = makeDefaults();
        v[param::OscWave] = 0; v[param::OscLevel] = 0.62f; v[param::OscOctave] = 0; v[param::SubLevel] = 0.40f;
        v[param::FilterCutoff] = 1400.0f; v[param::FilterRes] = 0.46f; v[param::FilterDrive] = 0.18f; v[param::FilterEnv] = 1.25f;
        v[param::AmpAttack] = 1.0f; v[param::AmpDecay] = 180.0f; v[param::AmpSustain] = 0.72f; v[param::AmpRelease] = 120.0f;
        v[param::FiltAttack] = 1.0f; v[param::FiltDecay] = 220.0f; v[param::FiltSustain] = 0.0f; v[param::FiltRelease] = 120.0f;
        v[param::GlideMs] = 18.0f; v[param::GlideMode] = 1; v[param::Accent] = 0.38f; v[param::Lfo1Rate] = 5; v[param::Lfo1Shape] = 0; v[param::Lfo1Cutoff] = 0.35f;
        v[param::RumbleLevel] = 0.18f; v[param::RumbleSend] = 0.12f; v[param::MasterGain] = -10.5f; v[param::SeqOn] = 1; v[param::SeqScale] = 1; v[param::SeqEvolve] = 0;
        return v;
    }(), defaultPattern()},
    {"BASS Acid Undertow", "bass-acid-undertow", [] {
        auto v = makeDefaults();
        v[param::OscWave] = 1; v[param::OscLevel] = 0.68f; v[param::OscOctave] = 0; v[param::SubLevel] = 0.32f;
        v[param::FilterCutoff] = 950.0f; v[param::FilterRes] = 0.82f; v[param::FilterDrive] = 0.38f; v[param::FilterEnv] = 2.2f;
        v[param::AmpAttack] = 1.0f; v[param::AmpDecay] = 170.0f; v[param::AmpSustain] = 0.68f; v[param::AmpRelease] = 130.0f;
        v[param::FiltAttack] = 1.0f; v[param::FiltDecay] = 260.0f; v[param::FiltSustain] = 0.0f; v[param::FiltRelease] = 120.0f;
        v[param::GlideMs] = 45.0f; v[param::GlideMode] = 2; v[param::Accent] = 0.55f; v[param::Lfo2Rate] = 11; v[param::Lfo2Shape] = 0; v[param::Lfo2Dest] = 0; v[param::Lfo2Depth] = 0.42f;
        v[param::MasterGain] = -12.5f; v[param::SeqOn] = 1; v[param::SeqScale] = 1; v[param::SeqLength] = 16; v[param::SeqGate] = 0.55f;
        return v;
    }(), [] {
        Pattern p = defaultPattern();
        const std::array<int, kSteps> semi{-12,0,3,0,-12,0,3,7,-12,0,3,0,-12,0,7,10};
        const std::array<bool, kSteps> gate{true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true};
        const std::array<bool, kSteps> accent{true,false,true,false,true,false,true,false,true,false,true,false,true,false,true,true};
        const std::array<bool, kSteps> slide{false,true,false,false,false,true,false,false,false,true,false,false,false,true,false,false};
        const std::array<float, kSteps> prob{1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f};
        return makePattern(semi, gate, accent, slide, prob);
    }()},
    {"BASS Dub Pulse", "bass-dub-pulse", [] {
        auto v = makeDefaults();
        v[param::OscWave] = 1; v[param::OscLevel] = 1.00f; v[param::OscOctave] = 1; v[param::SubLevel] = 0.00f;
        v[param::FilterCutoff] = 6000.0f; v[param::FilterRes] = 0.10f; v[param::FilterDrive] = 0.95f; v[param::FilterEnv] = 0.0f;
        v[param::AmpAttack] = 1.0f; v[param::AmpDecay] = 140.0f; v[param::AmpSustain] = 0.72f; v[param::AmpRelease] = 120.0f;
        v[param::FiltAttack] = 1.0f; v[param::FiltDecay] = 140.0f; v[param::FiltSustain] = 0.0f; v[param::FiltRelease] = 120.0f;
        v[param::GlideMs] = 12.0f; v[param::GlideMode] = 1; v[param::Accent] = 0.34f; v[param::Lfo2Rate] = 11; v[param::Lfo2Shape] = 0; v[param::Lfo2Dest] = 0; v[param::Lfo2Depth] = 0.60f;
        v[param::MasterGain] = -6.0f; v[param::SeqOn] = 1; v[param::SeqScale] = 1; v[param::SeqLength] = 16; v[param::SeqGate] = 0.62f;
        return v;
    }(), [] {
        Pattern p = defaultPattern();
        const std::array<int, kSteps> semi{0,0,-12,0,0,0,-12,0,0,0,-12,0,0,0,-12,0};
        const std::array<bool, kSteps> gate{true,false,true,false,true,false,true,false,true,false,true,false,true,false,true,false};
        const std::array<bool, kSteps> accent{true,false,false,false,true,false,false,false,true,false,false,false,true,false,false,false};
        const std::array<bool, kSteps> slide{false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
        const std::array<float, kSteps> prob{1.f,0.7f,1.f,0.7f,1.f,0.7f,1.f,0.7f,1.f,0.7f,1.f,0.7f,1.f,0.7f,1.f,0.7f};
        return makePattern(semi, gate, accent, slide, prob);
    }()},
{"BASS Warehouse Drone", "bass-warehouse-drone", [] {
        auto v = makeDefaults();
        v[param::OscWave] = 0; v[param::OscLevel] = 0.54f; v[param::OscOctave] = 0; v[param::SubLevel] = 0.34f;
        v[param::FilterCutoff] = 700.0f; v[param::FilterRes] = 0.56f; v[param::FilterDrive] = 0.14f; v[param::FilterEnv] = 1.0f;
        v[param::AmpAttack] = 15.0f; v[param::AmpDecay] = 340.0f; v[param::AmpSustain] = 0.72f; v[param::AmpRelease] = 260.0f;
        v[param::FiltAttack] = 12.0f; v[param::FiltDecay] = 420.0f; v[param::FiltSustain] = 0.0f; v[param::FiltRelease] = 220.0f;
        v[param::GlideMs] = 80.0f; v[param::GlideMode] = 1; v[param::Accent] = 0.30f; v[param::Lfo1Rate] = 16; v[param::Lfo1Shape] = 0; v[param::Lfo1Cutoff] = 0.22f;
        v[param::MasterGain] = -9.5f; v[param::SeqOn] = 1; v[param::SeqScale] = 1; v[param::SeqEvolve] = 3; v[param::SeqMutate] = 0.28f;
        return v;
    }(), [] {
        Pattern p = defaultPattern();
        const std::array<int, kSteps> semi{0,-5,0,3,0,-2,0,5,0,-5,0,3,0,-2,0,7};
        const std::array<bool, kSteps> gate{true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true};
        const std::array<bool, kSteps> accent{true,false,false,true,false,false,true,false,true,false,false,true,false,false,true,false};
        const std::array<bool, kSteps> slide{false,true,false,false,false,true,false,false,false,true,false,false,false,true,false,false};
        const std::array<float, kSteps> prob{1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f};
        return makePattern(semi, gate, accent, slide, prob);
    }()},
    {"RUMBLE Dark Room", "rumble-dark-room", [] {
        auto v = makeDefaults();
        v[param::OscWave] = 0; v[param::OscLevel] = 0.82f; v[param::OscOctave] = -1; v[param::SubLevel] = 0.40f;
        v[param::RumbleLevel] = 0.98f; v[param::RumbleSend] = 0.16f; v[param::RumbleKick] = 0.0f; v[param::RumbleDecay] = 4.0f; v[param::RumbleTone] = 82.0f; v[param::RumbleDrive] = 0.28f; v[param::RumbleDuck] = 0.10f;
        v[param::MasterGain] = -6.5f; v[param::SeqOn] = 0; v[param::SatDrive] = 0.08f; v[param::Width] = 0.0f;
        return v;
    }(), makePattern(
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        ALL_TRUE16,
        ALL_FALSE16,
        ALL_FALSE16,
        ALL_ONE16
    )},
    {"RUMBLE Tunnel Floor", "rumble-tunnel-floor", [] {
        auto v = makeDefaults();
        v[param::OscWave] = 0; v[param::OscLevel] = 0.86f; v[param::OscOctave] = -1; v[param::SubLevel] = 0.38f;
        v[param::RumbleLevel] = 0.96f; v[param::RumbleSend] = 0.24f; v[param::RumbleKick] = 0.72f; v[param::RumbleRate] = 3; v[param::RumbleDecay] = 3.2f; v[param::RumbleTone] = 100.0f; v[param::RumbleDrive] = 0.44f; v[param::RumbleDuck] = 0.64f;
        v[param::MasterGain] = -10.0f; v[param::SeqOn] = 1; v[param::SeqScale] = 1; v[param::SeqRate] = 1;
        return v;
    }(), [] {
        Pattern p = defaultPattern();
        const std::array<int, kSteps> semi{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
        const std::array<bool, kSteps> gate{true,false,true,false,true,false,true,false,true,false,true,false,true,false,true,false};
        const std::array<bool, kSteps> accent{false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
        const std::array<bool, kSteps> slide{false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
        const std::array<float, kSteps> prob{1.f,0.2f,1.f,0.2f,1.f,0.2f,1.f,0.2f,1.f,0.2f,1.f,0.2f,1.f,0.2f,1.f,0.2f};
        return makePattern(semi, gate, accent, slide, prob);
    }()},
    {"RUMBLE Sidechain Fog", "rumble-sidechain-fog", [] {
        auto v = makeDefaults();
        v[param::OscWave] = 0; v[param::OscLevel] = 0.72f; v[param::OscOctave] = -1; v[param::SubLevel] = 0.30f;
        v[param::RumbleLevel] = 0.96f; v[param::RumbleSend] = 0.34f; v[param::RumbleKick] = 0.8f; v[param::RumbleRate] = 1; v[param::RumbleDecay] = 5.0f; v[param::RumbleTone] = 88.0f; v[param::RumbleDrive] = 0.50f; v[param::RumbleDuck] = 0.92f;
        v[param::MasterGain] = -11.0f; v[param::SeqOn] = 1; v[param::SeqScale] = 1; v[param::SeqRate] = 1; v[param::SeqEvolve] = 2; v[param::SeqMutate] = 0.22f;
        return v;
    }(), [] {
        Pattern p = defaultPattern();
        const std::array<int, kSteps> semi{-12,0,-12,0,-12,0,-12,0,-12,0,-12,0,-12,0,-12,0};
        const std::array<bool, kSteps> gate{true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true};
        const std::array<bool, kSteps> accent{true,false,false,false,true,false,false,false,true,false,false,false,true,false,false,false};
        const std::array<bool, kSteps> slide{false,true,false,true,false,true,false,true,false,true,false,true,false,true,false,true};
        const std::array<float, kSteps> prob{1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f,1.f};
        return makePattern(semi, gate, accent, slide, prob);
    }()},
    {"SEQ Evolving Minor", "seq-evolving-minor", [] {
        auto v = makeDefaults();
        v[param::OscWave] = 0; v[param::OscLevel] = 0.66f; v[param::OscOctave] = 0; v[param::SubLevel] = 0.28f;
        v[param::FilterCutoff] = 1300.0f; v[param::FilterRes] = 0.40f; v[param::FilterDrive] = 0.12f; v[param::FilterEnv] = 0.7f;
        v[param::Lfo1Rate] = 2; v[param::Lfo1Shape] = 0; v[param::Lfo1Cutoff] = 0.18f; v[param::Lfo2Rate] = 5; v[param::Lfo2Shape] = 1; v[param::Lfo2Dest] = 0; v[param::Lfo2Depth] = 0.22f;
        v[param::SeqOn] = 1; v[param::SeqScale] = 1; v[param::SeqMutate] = 0.30f; v[param::SeqEvolve] = 2; v[param::SeqRate] = 1; v[param::MasterGain] = -9.0f;
        return v;
    }(), [] {
        Pattern p = defaultPattern();
        const std::array<int, kSteps> semi{0,2,3,5,7,5,3,2,0,2,3,5,7,5,3,2};
        const std::array<bool, kSteps> gate{true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true};
        const std::array<bool, kSteps> accent{true,false,false,true,false,false,true,false,true,false,false,true,false,false,true,false};
        const std::array<bool, kSteps> slide{false,true,false,false,false,true,false,false,false,true,false,false,false,true,false,false};
        const std::array<float, kSteps> prob{1.f,1.f,1.f,0.8f,1.f,1.f,1.f,0.8f,1.f,1.f,1.f,0.8f,1.f,1.f,1.f,0.8f};
        return makePattern(semi, gate, accent, slide, prob);
    }()},
    {"SEQ Phrygian Pulse", "seq-phrygian-pulse", [] {
        auto v = makeDefaults();
        v[param::OscWave] = 0; v[param::OscLevel] = 0.74f; v[param::OscOctave] = 0; v[param::SubLevel] = 0.30f;
        v[param::FilterCutoff] = 1200.0f; v[param::FilterRes] = 0.30f; v[param::FilterDrive] = 0.10f; v[param::FilterEnv] = 0.5f;
        v[param::SeqOn] = 1; v[param::SeqScale] = 2; v[param::SeqSwing] = 0.22f; v[param::SeqGate] = 0.45f; v[param::SeqMutate] = 0.18f; v[param::SeqEvolve] = 0; v[param::MasterGain] = -8.0f;
        return v;
    }(), [] {
        Pattern p = defaultPattern();
        const std::array<int, kSteps> semi{0,1,3,5,7,8,10,8,7,5,3,1,0,1,3,5};
        const std::array<bool, kSteps> gate{true,true,false,true,true,false,true,true,true,false,true,true,true,false,true,true};
        const std::array<bool, kSteps> accent{true,false,false,true,false,false,true,false,true,false,false,true,false,false,true,false};
        const std::array<bool, kSteps> slide{false,false,false,true,false,false,false,false,false,false,true,false,false,false,true,false};
        const std::array<float, kSteps> prob{1.f,0.75f,0.5f,1.f,1.f,0.5f,1.f,0.75f,1.f,0.5f,1.f,0.75f,1.f,0.5f,1.f,0.75f};
        return makePattern(semi, gate, accent, slide, prob);
    }()},
};

#undef ALL_TRUE16
#undef ALL_FALSE16
#undef ALL_ONE16

}  // namespace

int numFactoryPresets() { return static_cast<int>(std::size(kPresets)); }
const char* factoryPresetName(int index) { return (index >= 0 && index < numFactoryPresets()) ? kPresets[index].name : ""; }
const char* factoryPresetSlug(int index) { return (index >= 0 && index < numFactoryPresets()) ? kPresets[index].slug : ""; }
bool factoryPreset(int index, float* values, Pattern* pattern) {
    if (index < 0 || index >= numFactoryPresets() || values == nullptr || pattern == nullptr) return false;
    for (int i = 0; i < kNumParams; ++i) values[i] = kPresets[index].values[static_cast<std::size_t>(i)];
    *pattern = kPresets[index].pattern;
    return true;
}
int findFactoryPreset(std::string_view nameOrSlug) {
    for (int i = 0; i < numFactoryPresets(); ++i) if (nameOrSlug == kPresets[i].name || nameOrSlug == kPresets[i].slug) return i;
    return -1;
}

}  // namespace undertow
