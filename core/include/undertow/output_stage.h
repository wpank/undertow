#pragma once

#include <array>

namespace undertow {

float softClip(float x);

class OutputStage {
public:
    void prepare(double sampleRate);
    void reset();
    void setParams(const float* values);
    float process(float x);
    bool clipped() const;

private:
    double sampleRate_ = 48000.0;
    double dcX1_ = 0.0;
    double dcY1_ = 0.0;
    double dcR_ = 0.0;
    std::array<std::array<double, 5>, 2> hpCoeff_ = {};
    std::array<std::array<double, 2>, 2> hpState_ = {};
    double masterTarget_ = 1.0;
    double masterCurrent_ = 1.0;
    double gainSmooth_ = 0.0;
    int clipSamplesRemaining_ = 0;
    int clipHoldSamples_ = 0;
};

}  // namespace undertow
