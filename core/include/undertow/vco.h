#pragma once

namespace undertow {

enum class Wave { Triangle = 0, Square = 1 };

class Vco {
public:
    void prepare(double sampleRate);
    void reset(double phase = 0.0);
    void setWave(Wave wave);
    void setFrequency(double hz);
    float process();
    float triangle() const;
    float sub(int octaves) const;
    double phase() const;

private:
    static double wrapPhase(double phase);
    static double triangleAt(double phase);
    static double polyBlep(double phase, double dt);

    double sampleRate_ = 48000.0;
    double frequency_ = 0.0;
    double phase_ = 0.0;
    double lastPhase_ = 0.0;
    unsigned int currentParity_ = 0;
    unsigned int lastParity_ = 0;
    Wave wave_ = Wave::Triangle;
};

}  // namespace undertow
