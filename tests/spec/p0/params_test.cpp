// Phase 0 spec tests: the parameter table (docs/specs/phase-0-foundation.md, checks P0.01 and P0.02).
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

const std::vector<std::string> kLfoRates = {"4 bars", "2 bars", "1 bar", "1/2",  "1/4",  "1/8",
                                            "1/16",   "1/32",   "1/4T",  "1/8T", "1/16T", "3/16",
                                            "5/16",   "7/16",   "3/4",   "5/4",  "7/4"};
const std::vector<std::string> kLfoShapes = {"Sine", "Triangle", "Saw Down", "Saw Up", "Square", "Sample & Hold"};

// The table in docs/PRD.md, "Parameters", in host order. Index = position = undertow::param enumerator.
const std::vector<Expected> kTable = {
    {"osc_wave", "Wave", ParamKind::Choice, 0, 1, 0, 0, "", {"Saw", "Square"}},
    {"osc_pw", "Pulse Width", ParamKind::Float, 0.05f, 0.95f, 0.5f, 0, "", {}},
    {"osc_level", "Osc Level", ParamKind::Float, 0, 1, 0.6f, 0, "", {}},
    {"osc_octave", "Octave", ParamKind::Int, -2, 1, 0, 0, "oct", {}},
    {"osc_fine", "Fine", ParamKind::Float, -50, 50, 0, 0, "ct", {}},
    {"sub_level", "Sub Level", ParamKind::Float, 0, 1, 0.7f, 0, "", {}},
    {"sub_octave", "Sub Octave", ParamKind::Choice, 0, 1, 0, 0, "", {"-1 oct", "-2 oct"}},
    {"filter_cutoff", "Cutoff", ParamKind::Float, 20, 18000, 600, 600, "Hz", {}},
    {"filter_res", "Resonance", ParamKind::Float, 0, 1, 0.35f, 0, "", {}},
    {"filter_drive", "Drive", ParamKind::Float, 0, 1, 0.2f, 0, "", {}},
    {"filter_env", "Env Amount", ParamKind::Float, -6, 6, 3, 0, "oct", {}},
    {"filter_track", "Key Track", ParamKind::Float, 0, 1, 0.5f, 0, "", {}},
    {"amp_attack", "Amp Attack", ParamKind::Float, 0.5f, 2000, 1, 30, "ms", {}},
    {"amp_decay", "Amp Decay", ParamKind::Float, 1, 4000, 300, 200, "ms", {}},
    {"amp_sustain", "Amp Sustain", ParamKind::Float, 0, 1, 0.8f, 0, "", {}},
    {"amp_release", "Amp Release", ParamKind::Float, 1, 4000, 60, 200, "ms", {}},
    {"filt_attack", "Filter Attack", ParamKind::Float, 0.5f, 2000, 1, 30, "ms", {}},
    {"filt_decay", "Filter Decay", ParamKind::Float, 1, 4000, 220, 200, "ms", {}},
    {"filt_sustain", "Filter Sustain", ParamKind::Float, 0, 1, 0, 0, "", {}},
    {"filt_release", "Filter Release", ParamKind::Float, 1, 4000, 60, 200, "ms", {}},
    {"glide_ms", "Glide", ParamKind::Float, 0, 1000, 50, 100, "ms", {}},
    {"glide_mode", "Glide Mode", ParamKind::Choice, 0, 2, 1, 0, "", {"Off", "Legato", "Always"}},
    {"accent", "Accent", ParamKind::Float, 0, 1, 0.5f, 0, "", {}},
    {"bend_range", "Bend Range", ParamKind::Int, 0, 12, 2, 0, "st", {}},
    {"lfo1_rate", "LFO 1 Rate", ParamKind::Choice, 0, 16, 2, 0, "", kLfoRates},
    {"lfo1_shape", "LFO 1 Shape", ParamKind::Choice, 0, 5, 0, 0, "", kLfoShapes},
    {"lfo1_cutoff", "LFO 1 to Cutoff", ParamKind::Float, -4, 4, 0.5f, 0, "oct", {}},
    {"lfo1_phase", "LFO 1 Phase", ParamKind::Float, 0, 1, 0, 0, "", {}},
    {"lfo2_rate", "LFO 2 Rate", ParamKind::Choice, 0, 16, 11, 0, "", kLfoRates},
    {"lfo2_shape", "LFO 2 Shape", ParamKind::Choice, 0, 5, 1, 0, "", kLfoShapes},
    {"lfo2_dest", "LFO 2 Dest", ParamKind::Choice, 0, 5, 0, 0, "",
     {"Cutoff", "Resonance", "Pulse Width", "Rumble Send", "Pitch", "Amp"}},
    {"lfo2_depth", "LFO 2 Depth", ParamKind::Float, 0, 1, 0, 0, "", {}},
    {"lfo2_phase", "LFO 2 Phase", ParamKind::Float, 0, 1, 0, 0, "", {}},
    {"rumble_level", "Rumble", ParamKind::Float, 0, 1, 0.3f, 0, "", {}},
    {"rumble_send", "Rumble Send", ParamKind::Float, 0, 1, 0.25f, 0, "", {}},
    {"rumble_kick", "Ghost Kick", ParamKind::Float, 0, 1, 0.6f, 0, "", {}},
    {"rumble_rate", "Kick Rate", ParamKind::Choice, 0, 3, 0, 0, "", {"1/4", "1/2", "1 bar", "1/8"}},
    {"rumble_decay", "Rumble Decay", ParamKind::Float, 0.2f, 8, 2.5f, 2, "s", {}},
    {"rumble_tone", "Rumble Tone", ParamKind::Float, 40, 250, 110, 100, "Hz", {}},
    {"rumble_drive", "Rumble Drive", ParamKind::Float, 0, 1, 0.4f, 0, "", {}},
    {"rumble_duck", "Duck", ParamKind::Float, 0, 1, 0.8f, 0, "", {}},
    {"sat_drive", "Saturation", ParamKind::Float, 0, 1, 0.2f, 0, "", {}},
    {"width", "Width", ParamKind::Float, 0, 1, 0.25f, 0, "", {}},
    {"mono_freq", "Mono Below", ParamKind::Float, 60, 250, 120, 0, "Hz", {}},
    {"master_gain", "Master", ParamKind::Float, -24, 6, -3, 0, "dB", {}},
    {"seq_on", "Sequencer", ParamKind::Bool, 0, 1, 0, 0, "", {}},
    {"seq_sync", "Seq Sync", ParamKind::Choice, 0, 1, 0, 0, "", {"Host", "Internal"}},
    {"seq_bpm", "Seq BPM", ParamKind::Float, 60, 200, 128, 0, "bpm", {}},
    {"seq_rate", "Seq Rate", ParamKind::Choice, 0, 3, 1, 0, "", {"1/8", "1/16", "1/32", "1/16T"}},
    {"seq_length", "Seq Length", ParamKind::Int, 1, 16, 16, 0, "", {}},
    {"seq_gate", "Seq Gate", ParamKind::Float, 0.05f, 1, 0.5f, 0, "", {}},
    {"seq_swing", "Swing", ParamKind::Float, 0, 1, 0, 0, "", {}},
    {"seq_root", "Root", ParamKind::Int, 24, 60, 33, 0, "", {}},
    {"seq_scale", "Scale", ParamKind::Choice, 0, 4, 1, 0, "",
     {"Chromatic", "Minor", "Phrygian", "Dorian", "Minor Pent"}},
    {"seq_mutate", "Mutate", ParamKind::Float, 0, 1, 0.25f, 0, "", {}},
    {"seq_evolve", "Evolve", ParamKind::Choice, 0, 5, 0, 0, "",
     {"Off", "1 bar", "2 bars", "4 bars", "8 bars", "16 bars"}},
    {"seq_seed", "Seed", ParamKind::Int, 0, 9999, 1, 0, "", {}},
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

TEST_CASE("P0.01 the parameter table matches docs/PRD.md row for row") {
    REQUIRE_MESSAGE(undertow::kNumParams == static_cast<int>(kTable.size()),
                    "undertow::kNumParams must be " << kTable.size() << " (docs/PRD.md, Parameters)");
    for (int i = 0; i < undertow::kNumParams; ++i) {
        const auto& want = kTable[static_cast<std::size_t>(i)];
        const undertow::ParamSpec& got = undertow::paramSpec(i);
        INFO("parameter " << i << " must be `" << std::string(want.id) << "` (docs/PRD.md, Parameters, row " << i << ")");
        CHECK_MESSAGE(str(got.id) == want.id, "id is `" << str(got.id) << "`");
        CHECK_MESSAGE(str(got.name) == want.name, "name is \"" << str(got.name) << "\", want \"" << std::string(want.name) << "\"");
        CHECK_MESSAGE(got.kind == want.kind, "kind is " << kindName(got.kind) << ", want " << kindName(want.kind));
        CHECK_MESSAGE(same(got.min, want.min), "min is " << got.min << ", want " << want.min);
        CHECK_MESSAGE(same(got.max, want.max), "max is " << got.max << ", want " << want.max);
        CHECK_MESSAGE(same(got.def, want.def), "default is " << got.def << ", want " << want.def);
        CHECK_MESSAGE(same(got.centre, want.centre), "centre is " << got.centre << ", want " << want.centre);
        CHECK_MESSAGE(str(got.unit) == want.unit, "unit is \"" << str(got.unit) << "\", want \"" << std::string(want.unit) << "\"");
        CHECK_MESSAGE(got.numChoices == static_cast<int>(want.choices.size()),
                      "numChoices is " << got.numChoices << ", want " << want.choices.size());
        if (got.numChoices == static_cast<int>(want.choices.size()) && got.numChoices > 0) {
            REQUIRE_MESSAGE(got.choices != nullptr, "choices is null but numChoices is " << got.numChoices);
            for (int c = 0; c < got.numChoices; ++c)
                CHECK_MESSAGE(str(got.choices[c]) == want.choices[static_cast<std::size_t>(c)],
                              "choice " << c << " is \"" << str(got.choices[c]) << "\", want \""
                                        << want.choices[static_cast<std::size_t>(c)] << "\"");
        }
        if (want.kind == ParamKind::Choice)
            CHECK_MESSAGE(same(got.max, static_cast<float>(want.choices.size() - 1)),
                          "a Choice parameter's max is its number of choices minus one");
    }
}

TEST_CASE("P0.02 parameters are found by id, and the undertow::param enumerators are their indices") {
    for (int i = 0; i < static_cast<int>(kTable.size()); ++i)
        CHECK_MESSAGE(undertow::findParam(kTable[static_cast<std::size_t>(i)].id) == i,
                      "findParam(\"" << std::string(kTable[static_cast<std::size_t>(i)].id) << "\") must return " << i);
    CHECK_MESSAGE(undertow::findParam("no_such_param") == -1, "findParam of an unknown id must return -1");
    CHECK_MESSAGE(undertow::findParam("") == -1, "findParam(\"\") must return -1");

    using namespace undertow::param;
    CHECK(OscWave == 0);
    CHECK(SubOctave == 6);
    CHECK(FilterCutoff == 7);
    CHECK(FilterTrack == 11);
    CHECK(AmpAttack == 12);
    CHECK(FiltRelease == 19);
    CHECK(GlideMs == 20);
    CHECK(BendRange == 23);
    CHECK(Lfo1Rate == 24);
    CHECK(Lfo2Phase == 32);
    CHECK(RumbleLevel == 33);
    CHECK(RumbleDuck == 40);
    CHECK(SatDrive == 41);
    CHECK(MasterGain == 44);
    CHECK(SeqOn == 45);
    CHECK(SeqSeed == 56);
}
