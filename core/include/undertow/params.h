#pragma once
#include <string_view>

namespace undertow {

enum class ParamKind { Float, Int, Choice, Bool };

struct ParamSpec {
    const char* id;              // stable host id, e.g. "filter_cutoff"
    const char* name;            // display name, e.g. "Cutoff"
    ParamKind kind;
    float min;                   // real units; Choice: 0
    float max;                   // real units; Choice: numChoices - 1
    float def;                   // default, real units
    float centre;                // value at half knob travel (setSkewForCentre); 0 = linear
    const char* unit;            // "", "Hz", "ms", "s", "dB", "oct", "st", "ct", "bpm"
    const char* const* choices;  // Choice labels, else nullptr
    int numChoices;              // Choice: number of labels, else 0
};

constexpr int kNumParams = 57;

const ParamSpec& paramSpec(int index);   // 0 <= index < kNumParams, in docs/PRD.md section 4 order
int findParam(std::string_view id);      // the index of `id`, or -1

namespace param {
enum : int {  // the table's indices: the id in CamelCase
    OscWave, OscPw, OscLevel, OscOctave, OscFine, SubLevel, SubOctave,
    FilterCutoff, FilterRes, FilterDrive, FilterEnv, FilterTrack,
    AmpAttack, AmpDecay, AmpSustain, AmpRelease,
    FiltAttack, FiltDecay, FiltSustain, FiltRelease,
    GlideMs, GlideMode, Accent, BendRange,
    Lfo1Rate, Lfo1Shape, Lfo1Cutoff, Lfo1Phase,
    Lfo2Rate, Lfo2Shape, Lfo2Dest, Lfo2Depth, Lfo2Phase,
    RumbleLevel, RumbleSend, RumbleKick, RumbleRate, RumbleDecay, RumbleTone, RumbleDrive, RumbleDuck,
    SatDrive, Width, MonoFreq, MasterGain,
    SeqOn, SeqSync, SeqBpm, SeqRate, SeqLength, SeqGate, SeqSwing, SeqRoot, SeqScale, SeqMutate, SeqEvolve, SeqSeed,
};
}  // namespace param

}  // namespace undertow
