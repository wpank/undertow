#pragma once
#include <string_view>

namespace undertow {

enum class ParamKind { Float, Int, Choice, Bool };

struct ParamSpec {
    const char* id;
    const char* name;
    ParamKind kind;
    float min;
    float max;
    float def;
    float centre;
    const char* unit;
    const char* const* choices;
    int numChoices;
};

constexpr int kNumParams = 49;
constexpr int kSteps = 8;

const ParamSpec& paramSpec(int index);
int findParam(std::string_view id);

namespace param {
enum : int {
    Vco1Wave, Vco1Level, Vco2Wave, Vco2Tune, Vco2Level, VcoFm, NoiseLevel, SubLevel, SubOctave, PitchEnv, PitchDecay,
    VcfCutoff, VcfRes, VcfDrive, VcfEnv, VcfDecay,
    VcaAttack, VcaDecay,
    SeqMode, SeqRate, SeqLength, SeqDrift, SeqRoot, SeqBpm,
    Step1Pitch, Step2Pitch, Step3Pitch, Step4Pitch, Step5Pitch, Step6Pitch, Step7Pitch, Step8Pitch,
    Step1Vel, Step2Vel, Step3Vel, Step4Vel, Step5Vel, Step6Vel, Step7Vel, Step8Vel,
    RumbleLevel, RumbleSend, RumbleKick, RumbleRate, RumbleDecay, RumbleTone, RumbleDrive, RumbleDuck,
    MasterGain,
};
}  // namespace param

}  // namespace undertow
