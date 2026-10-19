#include "undertow/params.h"

namespace undertow {

// Choice label arrays (stable, never freed)

static const char* const kOscWaveChoices[] = {"Saw", "Square"};
static const char* const kSubOctaveChoices[] = {"-1 oct", "-2 oct"};
static const char* const kGlideModeChoices[] = {"Off", "Legato", "Always"};
static const char* const kLfoRateChoices[] = {
    "4 bars", "2 bars", "1 bar", "1/2",  "1/4",  "1/8",
    "1/16",   "1/32",   "1/4T",  "1/8T", "1/16T", "3/16",
    "5/16",   "7/16",   "3/4",   "5/4",  "7/4"
};
static const char* const kLfoShapeChoices[] = {
    "Sine", "Triangle", "Saw Down", "Saw Up", "Square", "Sample & Hold"
};
static const char* const kLfo2DestChoices[] = {
    "Cutoff", "Resonance", "Pulse Width", "Rumble Send", "Pitch", "Amp"
};
static const char* const kRumbleRateChoices[] = {"1/4", "1/2", "1 bar", "1/8"};
static const char* const kSeqSyncChoices[] = {"Host", "Internal"};
static const char* const kSeqRateChoices[] = {"1/8", "1/16", "1/32", "1/16T"};
static const char* const kSeqScaleChoices[] = {"Chromatic", "Minor", "Phrygian", "Dorian", "Minor Pent"};
static const char* const kSeqEvolveChoices[] = {"Off", "1 bar", "2 bars", "4 bars", "8 bars", "16 bars"};

// The parameter table in docs/PRD.md section 4 order.

static const ParamSpec kParams[kNumParams] = {
    // 0
    {"osc_wave",       "Wave",          ParamKind::Choice, 0,     1,     0,    0,   "",    kOscWaveChoices,    2},
    // 1
    {"osc_pw",         "Pulse Width",   ParamKind::Float,  0.05f, 0.95f, 0.5f, 0,   "",    nullptr,            0},
    // 2
    {"osc_level",      "Osc Level",     ParamKind::Float,  0,     1,     0.6f, 0,   "",    nullptr,            0},
    // 3
    {"osc_octave",     "Octave",        ParamKind::Int,   -2,     1,     0,    0,   "oct", nullptr,            0},
    // 4
    {"osc_fine",       "Fine",          ParamKind::Float, -50,    50,    0,    0,   "ct",  nullptr,            0},
    // 5
    {"sub_level",      "Sub Level",     ParamKind::Float,  0,     1,     0.7f, 0,   "",    nullptr,            0},
    // 6
    {"sub_octave",     "Sub Octave",    ParamKind::Choice, 0,     1,     0,    0,   "",    kSubOctaveChoices,  2},
    // 7
    {"filter_cutoff",  "Cutoff",        ParamKind::Float,  20,    18000, 600,  600, "Hz",  nullptr,            0},
    // 8
    {"filter_res",     "Resonance",     ParamKind::Float,  0,     1,     0.35f,0,   "",    nullptr,            0},
    // 9
    {"filter_drive",   "Drive",         ParamKind::Float,  0,     1,     0.2f, 0,   "",    nullptr,            0},
    // 10
    {"filter_env",     "Env Amount",    ParamKind::Float, -6,     6,     3,    0,   "oct", nullptr,            0},
    // 11
    {"filter_track",   "Key Track",     ParamKind::Float,  0,     1,     0.5f, 0,   "",    nullptr,            0},
    // 12
    {"amp_attack",     "Amp Attack",    ParamKind::Float,  0.5f,  2000,  1,    30,  "ms",  nullptr,            0},
    // 13
    {"amp_decay",      "Amp Decay",     ParamKind::Float,  1,     4000,  300,  200, "ms",  nullptr,            0},
    // 14
    {"amp_sustain",    "Amp Sustain",   ParamKind::Float,  0,     1,     0.8f, 0,   "",    nullptr,            0},
    // 15
    {"amp_release",    "Amp Release",   ParamKind::Float,  1,     4000,  60,   200, "ms",  nullptr,            0},
    // 16
    {"filt_attack",    "Filter Attack", ParamKind::Float,  0.5f,  2000,  1,    30,  "ms",  nullptr,            0},
    // 17
    {"filt_decay",     "Filter Decay",  ParamKind::Float,  1,     4000,  220,  200, "ms",  nullptr,            0},
    // 18
    {"filt_sustain",   "Filter Sustain",ParamKind::Float,  0,     1,     0,    0,   "",    nullptr,            0},
    // 19
    {"filt_release",   "Filter Release",ParamKind::Float,  1,     4000,  60,   200, "ms",  nullptr,            0},
    // 20
    {"glide_ms",       "Glide",         ParamKind::Float,  0,     1000,  50,   100, "ms",  nullptr,            0},
    // 21
    {"glide_mode",     "Glide Mode",    ParamKind::Choice, 0,     2,     1,    0,   "",    kGlideModeChoices,  3},
    // 22
    {"accent",         "Accent",        ParamKind::Float,  0,     1,     0.5f, 0,   "",    nullptr,            0},
    // 23
    {"bend_range",     "Bend Range",    ParamKind::Int,    0,     12,    2,    0,   "st",  nullptr,            0},
    // 24
    {"lfo1_rate",      "LFO 1 Rate",    ParamKind::Choice, 0,     16,    2,    0,   "",    kLfoRateChoices,    17},
    // 25
    {"lfo1_shape",     "LFO 1 Shape",   ParamKind::Choice, 0,     5,     0,    0,   "",    kLfoShapeChoices,   6},
    // 26
    {"lfo1_cutoff",    "LFO 1 to Cutoff",ParamKind::Float,-4,     4,     0.5f, 0,   "oct", nullptr,            0},
    // 27
    {"lfo1_phase",     "LFO 1 Phase",   ParamKind::Float,  0,     1,     0,    0,   "",    nullptr,            0},
    // 28
    {"lfo2_rate",      "LFO 2 Rate",    ParamKind::Choice, 0,     16,    11,   0,   "",    kLfoRateChoices,    17},
    // 29
    {"lfo2_shape",     "LFO 2 Shape",   ParamKind::Choice, 0,     5,     1,    0,   "",    kLfoShapeChoices,   6},
    // 30
    {"lfo2_dest",      "LFO 2 Dest",    ParamKind::Choice, 0,     5,     0,    0,   "",    kLfo2DestChoices,   6},
    // 31
    {"lfo2_depth",     "LFO 2 Depth",   ParamKind::Float,  0,     1,     0,    0,   "",    nullptr,            0},
    // 32
    {"lfo2_phase",     "LFO 2 Phase",   ParamKind::Float,  0,     1,     0,    0,   "",    nullptr,            0},
    // 33
    {"rumble_level",   "Rumble",        ParamKind::Float,  0,     1,     0.3f, 0,   "",    nullptr,            0},
    // 34
    {"rumble_send",    "Rumble Send",   ParamKind::Float,  0,     1,     0.25f,0,   "",    nullptr,            0},
    // 35
    {"rumble_kick",    "Ghost Kick",    ParamKind::Float,  0,     1,     0.6f, 0,   "",    nullptr,            0},
    // 36
    {"rumble_rate",    "Kick Rate",     ParamKind::Choice, 0,     3,     0,    0,   "",    kRumbleRateChoices, 4},
    // 37
    {"rumble_decay",   "Rumble Decay",  ParamKind::Float,  0.2f,  8,     2.5f, 2,   "s",   nullptr,            0},
    // 38
    {"rumble_tone",    "Rumble Tone",   ParamKind::Float,  40,    250,   110,  100, "Hz",  nullptr,            0},
    // 39
    {"rumble_drive",   "Rumble Drive",  ParamKind::Float,  0,     1,     0.4f, 0,   "",    nullptr,            0},
    // 40
    {"rumble_duck",    "Duck",          ParamKind::Float,  0,     1,     0.8f, 0,   "",    nullptr,            0},
    // 41
    {"sat_drive",      "Saturation",    ParamKind::Float,  0,     1,     0.2f, 0,   "",    nullptr,            0},
    // 42
    {"width",          "Width",         ParamKind::Float,  0,     1,     0.25f,0,   "",    nullptr,            0},
    // 43
    {"mono_freq",      "Mono Below",    ParamKind::Float,  60,    250,   120,  0,   "Hz",  nullptr,            0},
    // 44
    {"master_gain",    "Master",        ParamKind::Float, -24,    6,    -3,    0,   "dB",  nullptr,            0},
    // 45
    {"seq_on",         "Sequencer",     ParamKind::Bool,   0,     1,     0,    0,   "",    nullptr,            0},
    // 46
    {"seq_sync",       "Seq Sync",      ParamKind::Choice, 0,     1,     0,    0,   "",    kSeqSyncChoices,    2},
    // 47
    {"seq_bpm",        "Seq BPM",       ParamKind::Float,  60,    200,   128,  0,   "bpm", nullptr,            0},
    // 48
    {"seq_rate",       "Seq Rate",      ParamKind::Choice, 0,     3,     1,    0,   "",    kSeqRateChoices,    4},
    // 49
    {"seq_length",     "Seq Length",    ParamKind::Int,    1,     16,    16,   0,   "",    nullptr,            0},
    // 50
    {"seq_gate",       "Seq Gate",      ParamKind::Float,  0.05f, 1,     0.5f, 0,   "",    nullptr,            0},
    // 51
    {"seq_swing",      "Swing",         ParamKind::Float,  0,     1,     0,    0,   "",    nullptr,            0},
    // 52
    {"seq_root",       "Root",          ParamKind::Int,    24,    60,    33,   0,   "",    nullptr,            0},
    // 53
    {"seq_scale",      "Scale",         ParamKind::Choice, 0,     4,     1,    0,   "",    kSeqScaleChoices,   5},
    // 54
    {"seq_mutate",     "Mutate",        ParamKind::Float,  0,     1,     0.25f,0,   "",    nullptr,            0},
    // 55
    {"seq_evolve",     "Evolve",        ParamKind::Choice, 0,     5,     0,    0,   "",    kSeqEvolveChoices,  6},
    // 56
    {"seq_seed",       "Seed",          ParamKind::Int,    0,     9999,  1,    0,   "",    nullptr,            0},
};

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
