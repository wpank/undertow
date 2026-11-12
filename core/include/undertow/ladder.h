#pragma once

namespace undertow {

class Ladder {
public:
    void prepare(double sampleRate);
    void reset();
    void setCutoff(double hz);
    void setResonance(double r);
    void setDrive(double d);
    float process(float x);

private:
    static constexpr double kMinCutoff = 20.0;
    static constexpr double kMaxCutoffRatio = 0.45;
    static constexpr double kMaxResonance = 1.0;
    static constexpr double kMaxDrive = 1.0;
    static constexpr double kDenorm = 1e-20;

    static double shape(double x);
    static double flush(double x);

    double sampleRate_ = 48000.0;
    double cutoff_ = 20.0;
    double resonance_ = 0.0;
    double drive_ = 0.0;
    double driveGain_ = 0.5;

    double s_[4] = {};
    double prevU_ = 0.0;
};

}  // namespace undertow
