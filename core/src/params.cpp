#include "undertow/params.h"

namespace undertow {
namespace {

constexpr const char* const kWaveChoices[] = {"Triangle", "Square"};
constexpr const char* const kSubOctaveChoices[] = {"0 oct", "-1 oct"};
constexpr const char* const kAttackChoices[] = {"Fast", "Slow"};
constexpr const char* const kSeqModeChoices[] = {"Key", "Free", "Notes"};
constexpr const char* const kRateChoices[] = {"1 bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T"};
constexpr const char* const kRumbleRateChoices[] = {"1/4", "1/2", "1 bar", "1/8"};

constexpr ParamSpec kParams[kNumParams] = {
    {"vco1_wave", "VCO 1 Wave", ParamKind::Choice, 0.0f, 1.0f, 0.0f, 0.0f, "", kWaveChoices, 2},
    {"vco1_level", "VCO 1 Level", ParamKind::Float, 0.0f, 1.0f, 0.45f, 0.0f, "", nullptr, 0},
    {"vco2_wave", "VCO 2 Wave", ParamKind::Choice, 0.0f, 1.0f, 1.0f, 0.0f, "", kWaveChoices, 2},
    {"vco2_tune", "VCO 2 Tune", ParamKind::Float, -24.0f, 24.0f, -12.0f, 0.0f, "st", nullptr, 0},
    {"vco2_level", "VCO 2 Level", ParamKind::Float, 0.0f, 1.0f, 0.28f, 0.0f, "", nullptr, 0},
    {"vco_fm", "1 to 2 FM", ParamKind::Float, 0.0f, 1.0f, 0.15f, 0.0f, "", nullptr, 0},
    {"noise_level", "Noise", ParamKind::Float, 0.0f, 1.0f, 0.05f, 0.0f, "", nullptr, 0},
    {"sub_level", "Sub", ParamKind::Float, 0.0f, 1.0f, 1.0f, 0.0f, "", nullptr, 0},
    {"sub_octave", "Sub Octave", ParamKind::Choice, 0.0f, 1.0f, 1.0f, 0.0f, "", kSubOctaveChoices, 2},
    {"pitch_env", "Pitch Env", ParamKind::Float, 0.0f, 24.0f, 7.0f, 0.0f, "st", nullptr, 0},
    {"pitch_decay", "Pitch Decay", ParamKind::Float, 5.0f, 1000.0f, 75.0f, 60.0f, "ms", nullptr, 0},
    {"vcf_cutoff", "Cutoff", ParamKind::Float, 20.0f, 8000.0f, 140.0f, 300.0f, "Hz", nullptr, 0},
    {"vcf_res", "Resonance", ParamKind::Float, 0.0f, 1.0f, 0.32f, 0.0f, "", nullptr, 0},
    {"vcf_drive", "Drive", ParamKind::Float, 0.0f, 1.0f, 0.4f, 0.0f, "", nullptr, 0},
    {"vcf_env", "VCF Env", ParamKind::Float, 0.0f, 4.0f, 2.0f, 0.0f, "oct", nullptr, 0},
    {"vcf_decay", "VCF Decay", ParamKind::Float, 10.0f, 10000.0f, 275.0f, 300.0f, "ms", nullptr, 0},
    {"vca_attack", "Attack", ParamKind::Choice, 0.0f, 1.0f, 0.0f, 0.0f, "", kAttackChoices, 2},
    {"vca_decay", "VCA Decay", ParamKind::Float, 10.0f, 20000.0f, 390.0f, 300.0f, "ms", nullptr, 0},
    {"seq_mode", "Mode", ParamKind::Choice, 0.0f, 2.0f, 0.0f, 0.0f, "", kSeqModeChoices, 3},
    {"seq_rate", "Rate", ParamKind::Choice, 0.0f, 7.0f, 4.0f, 0.0f, "", kRateChoices, 8},
    {"seq_length", "Length", ParamKind::Int, 1.0f, 8.0f, 8.0f, 0.0f, "", nullptr, 0},
    {"seq_drift", "Drift", ParamKind::Float, 0.0f, 1.0f, 0.7f, 0.0f, "", nullptr, 0},
    {"seq_root", "Root", ParamKind::Int, 24.0f, 60.0f, 43.0f, 0.0f, "", nullptr, 0},
    {"seq_bpm", "Tempo", ParamKind::Float, 60.0f, 200.0f, 130.0f, 0.0f, "bpm", nullptr, 0},
    {"step1_pitch", "Step 1 Pitch", ParamKind::Int, -12.0f, 12.0f, 0.0f, 0.0f, "st", nullptr, 0},
    {"step2_pitch", "Step 2 Pitch", ParamKind::Int, -12.0f, 12.0f, 0.0f, 0.0f, "st", nullptr, 0},
    {"step3_pitch", "Step 3 Pitch", ParamKind::Int, -12.0f, 12.0f, 0.0f, 0.0f, "st", nullptr, 0},
    {"step4_pitch", "Step 4 Pitch", ParamKind::Int, -12.0f, 12.0f, 0.0f, 0.0f, "st", nullptr, 0},
    {"step5_pitch", "Step 5 Pitch", ParamKind::Int, -12.0f, 12.0f, 0.0f, 0.0f, "st", nullptr, 0},
    {"step6_pitch", "Step 6 Pitch", ParamKind::Int, -12.0f, 12.0f, 0.0f, 0.0f, "st", nullptr, 0},
    {"step7_pitch", "Step 7 Pitch", ParamKind::Int, -12.0f, 12.0f, 0.0f, 0.0f, "st", nullptr, 0},
    {"step8_pitch", "Step 8 Pitch", ParamKind::Int, -12.0f, 12.0f, 0.0f, 0.0f, "st", nullptr, 0},
    {"step1_vel", "Step 1 Velocity", ParamKind::Float, 0.0f, 1.0f, 0.0f, 0.0f, "", nullptr, 0},
    {"step2_vel", "Step 2 Velocity", ParamKind::Float, 0.0f, 1.0f, 0.22f, 0.0f, "", nullptr, 0},
    {"step3_vel", "Step 3 Velocity", ParamKind::Float, 0.0f, 1.0f, 1.0f, 0.0f, "", nullptr, 0},
    {"step4_vel", "Step 4 Velocity", ParamKind::Float, 0.0f, 1.0f, 0.55f, 0.0f, "", nullptr, 0},
    {"step5_vel", "Step 5 Velocity", ParamKind::Float, 0.0f, 1.0f, 0.0f, 0.0f, "", nullptr, 0},
    {"step6_vel", "Step 6 Velocity", ParamKind::Float, 0.0f, 1.0f, 0.25f, 0.0f, "", nullptr, 0},
    {"step7_vel", "Step 7 Velocity", ParamKind::Float, 0.0f, 1.0f, 0.9f, 0.0f, "", nullptr, 0},
    {"step8_vel", "Step 8 Velocity", ParamKind::Float, 0.0f, 1.0f, 0.6f, 0.0f, "", nullptr, 0},
    {"rumble_level", "Rumble", ParamKind::Float, 0.0f, 1.0f, 0.3f, 0.0f, "", nullptr, 0},
    {"rumble_send", "Rumble Send", ParamKind::Float, 0.0f, 1.0f, 0.2f, 0.0f, "", nullptr, 0},
    {"rumble_kick", "Ghost Kick", ParamKind::Float, 0.0f, 1.0f, 0.6f, 0.0f, "", nullptr, 0},
    {"rumble_rate", "Kick Rate", ParamKind::Choice, 0.0f, 3.0f, 0.0f, 0.0f, "", kRumbleRateChoices, 4},
    {"rumble_decay", "Rumble Decay", ParamKind::Float, 0.2f, 8.0f, 4.5f, 2.0f, "s", nullptr, 0},
    {"rumble_tone", "Rumble Tone", ParamKind::Float, 40.0f, 250.0f, 100.0f, 100.0f, "Hz", nullptr, 0},
    {"rumble_drive", "Rumble Drive", ParamKind::Float, 0.0f, 1.0f, 0.5f, 0.0f, "", nullptr, 0},
    {"rumble_duck", "Duck", ParamKind::Float, 0.0f, 1.0f, 0.88f, 0.0f, "", nullptr, 0},
    {"master_gain", "Master", ParamKind::Float, -24.0f, 6.0f, -12.0f, 0.0f, "dB", nullptr, 0},
};


}  // namespace

const ParamSpec& paramSpec(int index) {
    return kParams[index];
}

int findParam(std::string_view id) {
    for (int i = 0; i < kNumParams; ++i) {
        if (id == kParams[i].id)
            return i;
    }
    return -1;
}

}  // namespace undertow
