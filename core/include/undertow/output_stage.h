#pragma once
#include <cstddef>
#include <vector>

namespace undertow {

/// Soft clipper: identity for |x| <= 0.5, tanh knee above, ceiling at 0.966.
float softClip(float x);

class OutputStage {
public:
    void prepare(double sampleRate);
    void reset();
    void setParams(const float* values);  // sat_drive, width, mono_freq, master_gain
    void process(float mono, float& left, float& right);
    bool clipped() const;                 // over full scale in the last 0.5 s

private:
    /// Direct Form II Transposed biquad with double-precision state.
    struct Biquad {
        double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1 = 0, z2 = 0;

        void resetState() { z1 = 0; z2 = 0; }
        double process(double x) {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    double sampleRate_ = 48000.0;

    // DC blocker (one-pole high-pass at ~5 Hz)
    double dcX1_ = 0.0;
    double dcY1_ = 0.0;
    double dcR_ = 0.0;

    // Saturation: tanh(g*x)/g cross-faded with identity by satDrive_
    double satDrive_ = 0.0;

    // Master gain smoothing
    double masterTarget_ = 1.0;
    double masterCurrent_ = 1.0;
    double gainSmooth_ = 0.0;   // per-sample smoothing coefficient

    // 4th-order Linkwitz-Riley crossover (two cascaded 2nd-order Butterworth sections)
    Biquad lp_[2];   // low-pass sections (unused in process, kept for coefficient storage)
    Biquad hp_[2];   // high-pass sections
    double monoFreq_ = 120.0;

    // Additional one-pole HP on the side path for steeper low-frequency rejection
    double hpSideLp_ = 0.0;  // one-pole LP state
    double hpSideC_  = 0.0;  // one-pole coefficient

    // Width: theta = width * pi/3.5
    // The wider angle (vs the old pi/4) compensates for the mono path carrying full
    // power: the side signal needs more amplitude to decorrelate the high band.
    double width_ = 0.0;
    double cosTheta_ = 1.0;
    double sinTheta_ = 0.0;

    // Width delay line (12 ms)
    std::vector<float> delayLine_;
    std::size_t delaySize_ = 0;
    std::size_t delayWrite_ = 0;

    // Clip detector: holds for 0.5 s after any pre-clipper sample exceeds 1.0
    double clipCountdown_ = 0.0;
    double clipHold_ = 0.0;      // 0.5 * sampleRate

    void updateCrossover();
    void updateWidth();
};

}  // namespace undertow
