#include "undertow/presets.h"

#include <array>
#include <cstddef>
#include <initializer_list>
#include <utility>

namespace undertow {
namespace {

struct PresetDef {
    const char* name;
    const char* slug;
    std::array<float, kNumParams> values;
};

std::array<float, kNumParams> makeDefaults() {
    std::array<float, kNumParams> values{};
    for (int i = 0; i < kNumParams; ++i)
        values[static_cast<std::size_t>(i)] = paramSpec(i).def;
    return values;
}

std::array<float, kNumParams> makePreset(std::initializer_list<std::pair<int, float>> overrides) {
    auto values = makeDefaults();
    for (const auto& [index, value] : overrides)
        values[static_cast<std::size_t>(index)] = value;
    return values;
}

const std::array<PresetDef, 8> kPresets{{
    {"Init", "init", makePreset({})},
    {"Roll", "roll",
     makePreset({{param::VcfCutoff, 130}, {param::VcfEnv, 2.4f}, {param::PitchEnv, 6}, {param::PitchDecay, 60},
                 {param::VcaDecay, 330}, {param::SeqDrift, 0.6f}, {param::Step1Vel, 0}, {param::Step2Vel, 0.35f},
                 {param::Step3Vel, 0.7f}, {param::Step4Vel, 0.5f}, {param::Step5Vel, 0}, {param::Step6Vel, 0.9f},
                 {param::Step7Vel, 0.4f}, {param::Step8Vel, 1.0f}})},
    {"Hold", "hold",
     makePreset({{param::VcfCutoff, 130}, {param::VcfEnv, 2.4f}, {param::PitchEnv, 6}, {param::PitchDecay, 60},
                 {param::VcfDecay, 1400}, {param::VcaAttack, 1}, {param::VcaDecay, 2800}, {param::SeqDrift, 0.6f},
                 {param::MasterGain, -18.5f}, {param::Step1Vel, 0}, {param::Step2Vel, 0.35f}, {param::Step3Vel, 0.7f},
                 {param::Step4Vel, 0.5f}, {param::Step5Vel, 0}, {param::Step6Vel, 0.9f}, {param::Step7Vel, 0.4f},
                 {param::Step8Vel, 1.0f}})},
    {"Drift", "drift",
     makePreset({{param::Vco1Wave, 1}, {param::Vco1Level, 0.4f}, {param::Vco2Wave, 0}, {param::Vco2Level, 0.35f},
                 {param::VcoFm, 0.3f}, {param::NoiseLevel, 0.08f}, {param::SubLevel, 0.85f}, {param::PitchEnv, 9},
                 {param::PitchDecay, 32}, {param::VcfCutoff, 110}, {param::VcfRes, 0.44f}, {param::VcfDrive, 0.49f},
                 {param::VcfEnv, 2.6f}, {param::VcfDecay, 184}, {param::VcaDecay, 276}, {param::SeqDrift, 0.9f},
                 {param::MasterGain, -9.5f}, {param::Step1Vel, 0}, {param::Step2Vel, 0.55f}, {param::Step3Vel, 1.0f},
                 {param::Step4Vel, 0.6f}, {param::Step5Vel, 0}, {param::Step6Vel, 0.7f}, {param::Step7Vel, 0.9f},
                 {param::Step8Vel, 0.5f}})},
    {"Rumble", "rumble",
     makePreset({{param::Vco1Level, 0.15f}, {param::Vco2Level, 0.1f}, {param::NoiseLevel, 0}, {param::SubLevel, 0.6f},
                 {param::VcfCutoff, 90}, {param::VcfEnv, 1.2f}, {param::VcaDecay, 600}, {param::SeqRate, 3},
                 {param::SeqDrift, 0.3f}, {param::RumbleLevel, 1}, {param::RumbleSend, 0.4f}, {param::RumbleKick, 0.8f},
                 {param::RumbleDecay, 6}, {param::RumbleTone, 90}, {param::RumbleDuck, 0.9f}, {param::MasterGain, -13},
                 {param::Step1Vel, 0}, {param::Step2Vel, 0.5f}, {param::Step3Vel, 0}, {param::Step4Vel, 0.35f},
                 {param::Step5Vel, 0}, {param::Step6Vel, 0.6f}, {param::Step7Vel, 0}, {param::Step8Vel, 0.3f}})},
    {"Pressure", "pressure",
     makePreset({{param::Vco1Wave, 1}, {param::Vco1Level, 0.4f}, {param::Vco2Wave, 0}, {param::Vco2Level, 0.35f},
                 {param::VcoFm, 0.3f}, {param::NoiseLevel, 0.08f}, {param::SubLevel, 0.85f}, {param::PitchEnv, 9},
                 {param::PitchDecay, 32}, {param::VcfCutoff, 115}, {param::VcfRes, 0.48f}, {param::VcfDrive, 0.875f},
                 {param::VcfEnv, 2.6f}, {param::VcfDecay, 230}, {param::VcaDecay, 390}, {param::SeqDrift, 0.5f},
                 {param::MasterGain, -7}, {param::Step1Vel, 0}, {param::Step2Vel, 0.55f}, {param::Step3Vel, 1.0f},
                 {param::Step4Vel, 0.6f}, {param::Step5Vel, 0}, {param::Step6Vel, 0.7f}, {param::Step7Vel, 0.9f},
                 {param::Step8Vel, 0.5f}})},
    {"Offbeat", "offbeat",
     makePreset({{param::PitchEnv, 5}, {param::PitchDecay, 50}, {param::VcfCutoff, 120}, {param::VcaDecay, 250},
                 {param::SeqRate, 3}, {param::SeqDrift, 0.4f}, {param::MasterGain, -8}, {param::Step1Vel, 0},
                 {param::Step2Vel, 1.0f}, {param::Step3Vel, 0}, {param::Step4Vel, 0.8f}, {param::Step5Vel, 0},
                 {param::Step6Vel, 0.9f}, {param::Step7Vel, 0}, {param::Step8Vel, 0.7f}})},
    {"Deep", "deep",
     makePreset({{param::Vco2Level, 0.2f}, {param::VcfCutoff, 100}, {param::VcfEnv, 1.5f}, {param::VcaDecay, 375},
                 {param::SeqDrift, 0.5f}, {param::SeqRoot, 40}, {param::MasterGain, -10}, {param::Step1Vel, 0},
                 {param::Step2Vel, 0.4f}, {param::Step3Vel, 0.7f}, {param::Step4Vel, 1.0f}, {param::Step5Vel, 0},
                 {param::Step6Vel, 0.5f}, {param::Step7Vel, 0.8f}, {param::Step8Vel, 0.3f}})},
}};

}  // namespace

int numFactoryPresets() {
    return static_cast<int>(kPresets.size());
}

const char* factoryPresetName(int index) {
    return index >= 0 && index < numFactoryPresets() ? kPresets[static_cast<std::size_t>(index)].name : "";
}

const char* factoryPresetSlug(int index) {
    return index >= 0 && index < numFactoryPresets() ? kPresets[static_cast<std::size_t>(index)].slug : "";
}

bool factoryPreset(int index, float* values) {
    if (values == nullptr || index < 0 || index >= numFactoryPresets())
        return false;
    const auto& preset = kPresets[static_cast<std::size_t>(index)].values;
    for (int i = 0; i < kNumParams; ++i)
        values[i] = preset[static_cast<std::size_t>(i)];
    return true;
}

int findFactoryPreset(std::string_view nameOrSlug) {
    for (int i = 0; i < numFactoryPresets(); ++i) {
        const auto& preset = kPresets[static_cast<std::size_t>(i)];
        if (nameOrSlug == preset.name || nameOrSlug == preset.slug)
            return i;
    }
    return -1;
}

}  // namespace undertow
