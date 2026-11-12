// SPDX-License-Identifier: AGPL-3.0-only
// Phase 1 spec tests: the parameter table (docs/specs/phase-1-foundation.md, checks P1.01 and P1.02).
// spec-needs: core/include/undertow/params.h

#include "doctest.h"

#include "undertow/params.h"

#include <cmath>
#include <string>
#include <vector>

namespace {

using undertow::ParamKind;

struct Expected {
    const char* id;
    const char* name;
    ParamKind kind;
    float min, max, def, centre;
    const char* unit;
    std::vector<std::string> choices;
};

const std::vector<std::string> kWaves = {"Triangle", "Square"};
const std::vector<std::string> kRates = {"1 bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T"};

// The table in docs/PRD.md, "Parameters", in host order. Index = position = undertow::param enumerator.
const std::vector<Expected> kTable = {
    {"vco1_wave", "VCO 1 Wave", ParamKind::Choice, 0, 1, 0, 0, "", kWaves},
    {"vco1_level", "VCO 1 Level", ParamKind::Float, 0, 1, 0.45f, 0, "", {}},
    {"vco2_wave", "VCO 2 Wave", ParamKind::Choice, 0, 1, 1, 0, "", kWaves},
    {"vco2_tune", "VCO 2 Tune", ParamKind::Float, -24, 24, -12, 0, "st", {}},
    {"vco2_level", "VCO 2 Level", ParamKind::Float, 0, 1, 0.28f, 0, "", {}},
    {"vco_fm", "1 to 2 FM", ParamKind::Float, 0, 1, 0.15f, 0, "", {}},
    {"noise_level", "Noise", ParamKind::Float, 0, 1, 0.05f, 0, "", {}},
    {"sub_level", "Sub", ParamKind::Float, 0, 1, 1, 0, "", {}},
    {"sub_octave", "Sub Octave", ParamKind::Choice, 0, 1, 1, 0, "", {"0 oct", "-1 oct"}},
    {"pitch_env", "Pitch Env", ParamKind::Float, 0, 24, 7, 0, "st", {}},
    {"pitch_decay", "Pitch Decay", ParamKind::Float, 5, 1000, 75, 60, "ms", {}},
    {"vcf_cutoff", "Cutoff", ParamKind::Float, 20, 8000, 140, 300, "Hz", {}},
    {"vcf_res", "Resonance", ParamKind::Float, 0, 1, 0.32f, 0, "", {}},
    {"vcf_drive", "Drive", ParamKind::Float, 0, 1, 0.4f, 0, "", {}},
    {"vcf_env", "VCF Env", ParamKind::Float, 0, 4, 2, 0, "oct", {}},
    {"vcf_decay", "VCF Decay", ParamKind::Float, 10, 10000, 275, 300, "ms", {}},
    {"vca_attack", "Attack", ParamKind::Choice, 0, 1, 0, 0, "", {"Fast", "Slow"}},
    {"vca_decay", "VCA Decay", ParamKind::Float, 10, 20000, 390, 300, "ms", {}},
    {"seq_mode", "Mode", ParamKind::Choice, 0, 2, 0, 0, "", {"Key", "Free", "Notes"}},
    {"seq_rate", "Rate", ParamKind::Choice, 0, 7, 4, 0, "", kRates},
    {"seq_length", "Length", ParamKind::Int, 1, 8, 8, 0, "", {}},
    {"seq_drift", "Drift", ParamKind::Float, 0, 1, 0.7f, 0, "", {}},
    {"seq_root", "Root", ParamKind::Int, 24, 60, 43, 0, "", {}},
    {"seq_bpm", "Tempo", ParamKind::Float, 60, 200, 130, 0, "bpm", {}},
    {"step1_pitch", "Step 1 Pitch", ParamKind::Int, -12, 12, 0, 0, "st", {}},
    {"step2_pitch", "Step 2 Pitch", ParamKind::Int, -12, 12, 0, 0, "st", {}},
    {"step3_pitch", "Step 3 Pitch", ParamKind::Int, -12, 12, 0, 0, "st", {}},
    {"step4_pitch", "Step 4 Pitch", ParamKind::Int, -12, 12, 0, 0, "st", {}},
    {"step5_pitch", "Step 5 Pitch", ParamKind::Int, -12, 12, 0, 0, "st", {}},
    {"step6_pitch", "Step 6 Pitch", ParamKind::Int, -12, 12, 0, 0, "st", {}},
    {"step7_pitch", "Step 7 Pitch", ParamKind::Int, -12, 12, 0, 0, "st", {}},
    {"step8_pitch", "Step 8 Pitch", ParamKind::Int, -12, 12, 0, 0, "st", {}},
    {"step1_vel", "Step 1 Velocity", ParamKind::Float, 0, 1, 0, 0, "", {}},
    {"step2_vel", "Step 2 Velocity", ParamKind::Float, 0, 1, 0.22f, 0, "", {}},
    {"step3_vel", "Step 3 Velocity", ParamKind::Float, 0, 1, 1, 0, "", {}},
    {"step4_vel", "Step 4 Velocity", ParamKind::Float, 0, 1, 0.55f, 0, "", {}},
    {"step5_vel", "Step 5 Velocity", ParamKind::Float, 0, 1, 0, 0, "", {}},
    {"step6_vel", "Step 6 Velocity", ParamKind::Float, 0, 1, 0.25f, 0, "", {}},
    {"step7_vel", "Step 7 Velocity", ParamKind::Float, 0, 1, 0.9f, 0, "", {}},
    {"step8_vel", "Step 8 Velocity", ParamKind::Float, 0, 1, 0.6f, 0, "", {}},
    {"rumble_level", "Rumble", ParamKind::Float, 0, 1, 0.3f, 0, "", {}},
    {"rumble_send", "Rumble Send", ParamKind::Float, 0, 1, 0.2f, 0, "", {}},
    {"rumble_kick", "Ghost Kick", ParamKind::Float, 0, 1, 0.6f, 0, "", {}},
    {"rumble_rate", "Kick Rate", ParamKind::Choice, 0, 3, 0, 0, "", {"1/4", "1/2", "1 bar", "1/8"}},
    {"rumble_decay", "Rumble Decay", ParamKind::Float, 0.2f, 8, 4.5f, 2, "s", {}},
    {"rumble_tone", "Rumble Tone", ParamKind::Float, 40, 250, 100, 100, "Hz", {}},
    {"rumble_drive", "Rumble Drive", ParamKind::Float, 0, 1, 0.5f, 0, "", {}},
    {"rumble_duck", "Duck", ParamKind::Float, 0, 1, 0.88f, 0, "", {}},
    {"master_gain", "Master", ParamKind::Float, -24, 6, -12, 0, "dB", {}},
};

std::string kindName(ParamKind kind) {
    switch (kind) {
    case ParamKind::Float: return "Float";
    case ParamKind::Int: return "Int";
    case ParamKind::Choice: return "Choice";
    case ParamKind::Bool: return "Bool";
    }
    return "?";
}

bool same(float a, float b) { return std::fabs(a - b) <= 1e-6f * std::max(1.0f, std::fabs(b)); }

std::string str(const char* s) { return s == nullptr ? std::string("<null>") : std::string(s); }

} // namespace

TEST_CASE("P1.01 the parameter table matches docs/PRD.md row for row") {
    REQUIRE_MESSAGE(undertow::kNumParams == static_cast<int>(kTable.size()),
                    "undertow::kNumParams must be " << kTable.size() << " (docs/PRD.md, Parameters); it is "
                                                    << undertow::kNumParams);
    CHECK_MESSAGE(undertow::kSteps == 8, "undertow::kSteps, the sequencer's steps, must be 8");
    for (int i = 0; i < undertow::kNumParams; ++i) {
        const auto& want = kTable[static_cast<std::size_t>(i)];
        const undertow::ParamSpec& got = undertow::paramSpec(i);
        INFO("parameter " << i << " must be `" << std::string(want.id) << "` (docs/PRD.md, Parameters, row " << i << ")");
        CHECK_MESSAGE(str(got.id) == want.id, "id is `" << str(got.id) << "`");
        CHECK_MESSAGE(str(got.name) == want.name, "name is \"" << str(got.name) << "\", want \"" << std::string(want.name)
                                                             << "\"");
        CHECK_MESSAGE(got.kind == want.kind, "kind is " << kindName(got.kind) << ", want " << kindName(want.kind));
        CHECK_MESSAGE(same(got.min, want.min), "min is " << got.min << ", want " << want.min);
        CHECK_MESSAGE(same(got.max, want.max), "max is " << got.max << ", want " << want.max);
        CHECK_MESSAGE(same(got.def, want.def), "default is " << got.def << ", want " << want.def);
        CHECK_MESSAGE(same(got.centre, want.centre), "centre is " << got.centre << ", want " << want.centre);
        CHECK_MESSAGE(str(got.unit) == want.unit, "unit is \"" << str(got.unit) << "\", want \"" << std::string(want.unit)
                                                             << "\"");
        CHECK_MESSAGE(got.numChoices == static_cast<int>(want.choices.size()),
                      "numChoices is " << got.numChoices << ", want " << want.choices.size());
        if (got.numChoices == static_cast<int>(want.choices.size()) && got.numChoices > 0) {
            REQUIRE_MESSAGE(got.choices != nullptr, "choices is null but numChoices is " << got.numChoices);
            for (int c = 0; c < got.numChoices; ++c)
                CHECK_MESSAGE(str(got.choices[c]) == want.choices[static_cast<std::size_t>(c)],
                              "choice " << c << " is \"" << str(got.choices[c]) << "\", want \""
                                        << want.choices[static_cast<std::size_t>(c)] << "\"");
        }
        if (want.choices.empty())
            CHECK_MESSAGE(got.choices == nullptr, "only a Choice parameter has choice labels");
    }
}

TEST_CASE("P1.02 parameters are found by id, and the undertow::param enumerators are their indices") {
    for (int i = 0; i < static_cast<int>(kTable.size()); ++i)
        CHECK_MESSAGE(undertow::findParam(kTable[static_cast<std::size_t>(i)].id) == i,
                      "findParam(\"" << std::string(kTable[static_cast<std::size_t>(i)].id) << "\") must return " << i);
    for (const char* unknown : {"no_such_param", "", "vcf", "vcf_cutoff ", "VCF_CUTOFF", "step9_pitch", "step0_vel"})
        CHECK_MESSAGE(undertow::findParam(unknown) == -1,
                      "findParam(\"" << std::string(unknown) << "\") must return -1: ids match exactly");
    // The first version's ids are gone.
    for (const char* retired : {"osc_wave", "filter_cutoff", "glide_ms", "accent", "lfo1_rate", "seq_on", "seq_gate",
                                "sat_drive", "width", "mono_freq"})
        CHECK_MESSAGE(undertow::findParam(retired) == -1,
                      "findParam(\"" << std::string(retired) << "\") must return -1: that parameter is retired");

    using namespace undertow::param;
    CHECK(Vco1Wave == 0);
    CHECK(Vco2Tune == 3);
    CHECK(SubOctave == 8);
    CHECK(PitchDecay == 10);
    CHECK(VcfCutoff == 11);
    CHECK(VcfDecay == 15);
    CHECK(VcaAttack == 16);
    CHECK(VcaDecay == 17);
    CHECK(SeqMode == 18);
    CHECK(SeqBpm == 23);
    CHECK(Step1Pitch == 24);
    CHECK(Step8Pitch == 31);
    CHECK(Step1Vel == 32);
    CHECK(Step8Vel == 39);
    CHECK(RumbleLevel == 40);
    CHECK(RumbleDuck == 47);
    CHECK(MasterGain == 48);
    CHECK(Step1Pitch + undertow::kSteps == Step1Vel);
}
