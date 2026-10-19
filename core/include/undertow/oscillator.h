#pragma once

#include <cstddef>

namespace undertow {

enum class Wave { Saw, Square };

class Oscillator {
public:
    void prepare(double sampleRate);
    void setWave(Wave wave);
    void setFrequency(double frequencyHz);
    void setPulseWidth(double width);
    void reset(double phase = 0.0);
    float process();

private:
    double sampleRate_ = 44100.0;
    double frequency_ = 0.0;
    double phase_ = 0.0;
    double pulseWidth_ = 0.5;
    Wave wave_ = Wave::Saw;

    double polyBlep(double phase, double dt) const;
};

class SineOscillator {
public:
    void prepare(double sampleRate);
    void setFrequency(double frequencyHz);
    void reset(double phase = 0.0);
    float process();
    double phase() const { return phase_; }

private:
    double sampleRate_ = 44100.0;
    double frequency_ = 0.0;
    double phase_ = 0.0;
};

} // namespace undertow
