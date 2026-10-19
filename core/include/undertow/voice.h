#pragma once
#include "undertow/envelope.h"
#include "undertow/filter.h"
#include "undertow/oscillator.h"
#include "undertow/params.h"

namespace undertow {

class MonoVoice {
public:
    void prepare(double sampleRate);
    void reset();
    void setParams(const float* values);
    void noteOn(int note, float velocity);
    void noteOff(int note);
    void allNotesOff();
    void setPitchBend(float amount);
    float process();
    bool isActive() const;
    int currentNote() const;
    double currentHz() const;

private:
    struct Smoother {
        double target = 0.0;
        double current = 0.0;
        double coeff = 0.0;
        void init(double v) { target = current = v; }
        void setTarget(double v) { target = v; }
        void tick() { current += coeff * (target - current); }
        double value() const { return current; }
    };

    void recomputePitch();
    void updateFilterCutoff(double filtEnvLevel);

    Oscillator osc_;
    SineOscillator sub_;
    LowpassFilter filter_;
    Envelope ampEnv_;
    Envelope filtEnv_;

    double sampleRate_ = 48000.0;
    bool prepared_ = false;
    bool firstSetParams_ = true;
    const float* params_ = nullptr;

    // Note state
    int heldNotes_[16] = {};
    int numHeld_ = 0;
    int currentNote_ = -1;
    bool isAccent_ = false;
    float accentGain_ = 1.0f;
    double accentOctaves_ = 0.0;
    float pitchBend_ = 0.0f;
    int bendRange_ = 2;
    int glideMode_ = 1;   // 0=Off, 1=Legato, 2=Always
    double glideMs_ = 50.0;

    // Glide state: linear interpolation in semitones from glideFromNote_ to glideToNote_
    // glidePos_ goes 0..1 over glideMs
    double glideFromNote_ = 0.0;
    double glideToNote_ = 0.0;
    double glidePos_ = 1.0;
    bool gliding_ = false;

    // Pitch
    double noteHz_ = 0.0;      // 440 * 2^((note-69)/12), without bend
    double playedHz_ = 0.0;    // noteHz with glide and bend

    // Smoothers
    Smoother oscLevelSmooth_;
    Smoother subLevelSmooth_;
    Smoother oscPwSmooth_;
    Smoother filterResSmooth_;
    Smoother filterDriveSmooth_;
    Smoother accentGainSmooth_;
    Smoother log2CutoffSmooth_;  // log2 of cutoff Hz
};

}  // namespace undertow
