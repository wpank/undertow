#pragma once

#include <array>
#include <vector>

namespace undertow {

class Rumble {
public:
    void prepare(double sampleRate);         // allocates the reverb
    void reset();                            // silence the tail, open the duck
    void setParams(const float* values);     // the rumble_* parameters; kNumParams values, real units
    float process(float send, double ppq, double bpm, bool running);
    double duckGain() const;                 // the duck's gain now, 1 = open

private:
    static constexpr int kAllpasses = 4;
    static constexpr int kLines = 8;
    static constexpr double kDenorm = 1e-20;

    double sampleRate_ = 48000.0;

    float level_ = 0.3f;
    float send_ = 0.25f;      // not used inside; engine uses it
    float kick_ = 0.6f;
    int   rate_ = 0;          // 0=1/4, 1=1/2, 2=1bar, 3=1/8
    float decay_ = 2.5f;
    float tone_ = 110.0f;
    float drive_ = 0.4f;
    float duck_ = 0.8f;

    double kickPhase_ = 0.0;
    double kickTime_ = 0.0;
    bool   kickActive_ = false;
    double prevPpq_ = 0.0;
    bool   prevRunning_ = false;
    bool   firstSample_ = true;

    int preDelaySamples_ = 1;
    int preDelayWrite_ = 0;
    std::vector<float> preDelayData_;

    std::array<std::vector<float>, kAllpasses> allpassData_;
    std::array<int, kAllpasses> allpassWrite_ = {};
    std::array<int, kAllpasses> allpassDelay_ = {};
    std::array<double, kAllpasses> allpassGains_ = {};

    std::array<std::vector<float>, kLines> lineData_;
    std::array<int, kLines> lineWrite_ = {};
    std::array<int, kLines> lineDelay_ = {};
    std::array<double, kLines> lineGain_ = {};
    std::array<double, kLines> linePhase_ = {};
    std::array<double, kLines> linePhaseDelta_ = {};

    std::array<double, 5> rumbleHpCoeff_ = {};
    std::array<double, 2> rumbleHpState_ = {};
    std::array<std::array<double, 5>, 2> toneCoeff_ = {};
    std::array<std::array<double, 2>, 2> toneState_ = {};

    double duckGain_ = 1.0;
    double duckEnvelope_ = 0.0;
    double duckStartEnvelope_ = 0.0;
    double duckTime_ = 0.0;
    bool   duckActive_ = false;

    double preparedTone_ = -1.0;
    double preparedDecay_ = -1.0;

    double gridBeats() const;
    void fireKick();
    void updateToneFilters();
    void updateDecayGains();
    static double flush(double x);
};

}  // namespace undertow
