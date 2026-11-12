#pragma once

#include "undertow/decay.h"
#include "undertow/ladder.h"
#include "undertow/noise.h"
#include "undertow/vco.h"

namespace undertow {

class Voice {
public:
    void prepare(double sampleRate);
    void reset();
    void setParams(const float* values);
    void hit(int note, float velocity);
    float process();
    bool active() const;
    int note() const;
    double noteHz() const;
    double vco1Hz() const;
    double cutoffHz() const;
    float level() const;

private:
    static constexpr double kA4Hz = 440.0;
    static constexpr double kA4Note = 69.0;
    static constexpr double kSemitonesPerOctave = 12.0;
    static constexpr double kFastLevelAttackMs = 1.2;
    static constexpr double kFastFilterAttackMs = 0.6;
    static constexpr double kSlowLevelAttackMs = 100.0;
    static constexpr double kSlowFilterAttackMs = 500.0;
    static constexpr double kSmoothMs = 5.0;
    static constexpr float kRestartLevel = 0.1f;

    void setAttackMode(bool slow);
    void setImmediateTargets();
    void smoothParams();
    static double midiToHz(int note);

    double sampleRate_ = 48000.0;
    double smoothStep_ = 1.0;

    Vco vco1_;
    Vco vco2_;
    Noise noise_;
    Ladder ladder_;
    Decay pitchEnv_;
    Decay filterEnv_;
    Decay levelEnv_;

    float vco1Level_ = 0.0f;
    float vco2Level_ = 0.0f;
    float noiseLevel_ = 0.0f;
    float subLevel_ = 0.0f;
    float vco1LevelTarget_ = 0.0f;
    float vco2LevelTarget_ = 0.0f;
    float noiseLevelTarget_ = 0.0f;
    float subLevelTarget_ = 0.0f;

    double vco2Tune_ = 0.0;
    double vco2TuneTarget_ = 0.0;
    double vcoFm_ = 0.0;
    double vcoFmTarget_ = 0.0;
    double cutoffOctaves_ = 0.0;
    double cutoffOctavesTarget_ = 0.0;
    double resonance_ = 0.0;
    double resonanceTarget_ = 0.0;
    double drive_ = 0.0;
    double driveTarget_ = 0.0;

    float pitchAmount_ = 0.0f;
    float filterAmount_ = 0.0f;
    int subOctaves_ = -1;
    Wave vco1Wave_ = Wave::Triangle;
    Wave vco2Wave_ = Wave::Triangle;

    int note_ = -1;
    double noteHz_ = 0.0;
    double lastVco1Hz_ = 0.0;
    double lastCutoffHz_ = 0.0;
    float lastLevel_ = 0.0f;

    bool smoothReady_ = false;
    bool idle_ = true;
};

}  // namespace undertow
