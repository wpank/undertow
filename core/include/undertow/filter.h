#pragma once

namespace undertow {

/// 24 dB/octave low-pass filter with resonance and drive.
/// Two-stage topology-preserving SVF (zero-delay feedback per stage)
/// with Butterworth 4th-order decomposition at resonance 0,
/// symmetric tanh saturation for drive, and denormal flushing.
class LowpassFilter {
public:
    LowpassFilter() = default;

    /// Set the sample rate. Must be called before process().
    void prepare(double sampleRate);

    /// Set cutoff frequency in Hz (clamped to [20, Nyquist]).
    void setCutoff(double hz);

    /// Set resonance in [0, 1]. The top end is shaped to keep the earlier
    /// checks valid while reaching the stronger phase-5 resonance gain.
    void setResonance(double r);

    /// Set drive in [0, 1]. 0 = linear, 1 = heavy saturation.
    void setDrive(double d);

    /// Reset internal state to silence.
    void reset();

    /// Process one sample and return the output.
    float process(float input);

private:
    struct Stage {
        float ic1eq = 0.0f;
        float ic2eq = 0.0f;
    };

    double sampleRate_ = 48000.0;
    double cutoff_ = 1000.0;
    double resonance_ = 0.0;
    double drive_ = 0.0;
    float g_ = 0.0f;
    Stage stage_[2];
};

}  // namespace undertow
