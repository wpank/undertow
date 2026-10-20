#pragma once

namespace undertow {

class Envelope {
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void prepare(double sampleRate);
    void reset();                       // Idle at 0
    void setAttackMs(double ms);
    void setDecayMs(double ms);
    void setSustain(double level);
    void setReleaseMs(double ms);
    void noteOn();
    void noteOff();
    float process();                    // advance one sample, return the new level
    float level() const;
    Stage stage() const;

private:
    void recomputeAttack();
    void recomputeDecay();
    void recomputeRelease();

    double sampleRate_ = 48000.0;
    double attackMs_ = 0.5;
    double decayMs_ = 1.0;
    double sustain_ = 1.0;
    double releaseMs_ = 1.0;

    Stage stage_ = Stage::Idle;
    double level_ = 0.0;
    double sustainTarget_ = 1.0;
    double sustainSmooth_ = 1.0;

    double attackInc_ = 0.0;    // per-sample increment during attack
    double decayMul_ = 1.0;     // per-sample multiplier during decay
    double releaseMul_ = 1.0;   // per-sample multiplier during release

    static constexpr double kSustainGlideRate = 0.004;

    static constexpr double kMinAttackMs = 0.5;
    static constexpr double kMaxAttackMs = 2000.0;
    static constexpr double kMinDecayMs = 1.0;
    static constexpr double kMaxDecayMs = 4000.0;
    static constexpr double kMinReleaseMs = 1.0;
    static constexpr double kMaxReleaseMs = 4000.0;
    static constexpr double kIdleThreshold = 1e-5;  // -100 dB
};

} // namespace undertow
