#pragma once

namespace undertow {

class Decay {
public:
    void prepare(double sampleRate);
    void reset();
    void setAttack(double ms);
    void setDecay(double ms);
    void trigger(float peak);
    float process();
    float level() const;
    bool active() const;

private:
    static constexpr double kPi = 3.141592653589793238462643383279502884;
    static constexpr double kAttackShape = 0.5903;
    static constexpr double kSilence = 1e-5;
    static constexpr double kMinGlideMs = 4.0;

    enum class Stage { Idle, Attack, Decay };

    double attackMs_ = 0.0;
    double decayMs_ = 0.0;
    double sampleRate_ = 48000.0;
    double decayCoeff_ = 0.0;

    double level_ = 0.0;
    double start_ = 0.0;
    double peak_ = 0.0;
    double stageLen_ = 0.0;
    double stagePos_ = 0.0;
    double decayStart_ = 0.0;
    double decayPos_ = 0.0;
    Stage stage_ = Stage::Idle;
    bool active_ = false;

    double attackSamples(double ms) const;
    void updateDecayCoeff();
};

}  // namespace undertow
