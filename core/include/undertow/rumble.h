#pragma once

namespace undertow {

class Rumble {
public:
    void prepare(double sampleRate);         // allocates the reverb
    void reset();                            // silence the tail, open the duck
    void setParams(const float* values);     // the rumble_* parameters; kNumParams values, real units
    float process(float send, double ppq, double bpm, bool running);
    double duckGain() const;                 // the duck's gain now, 1 = open

private:
    static constexpr int kLines = 4;
    static constexpr double kDenorm = 1e-20;

    // Reverb line delays in seconds (31–62 ms)
    static constexpr double kLineDelays[4] = {0.0311, 0.0377, 0.0443, 0.0623};

    // Per-sample state
    double sampleRate_ = 48000.0;

    // Parameters (read from setParams)
    float level_ = 0.3f;
    float send_ = 0.25f;      // not used inside; engine uses it
    float kick_ = 0.6f;
    int   rate_ = 0;          // 0=1/4, 1=1/2, 2=1bar, 3=1/8
    float decay_ = 2.5f;
    float tone_ = 110.0f;
    float drive_ = 0.4f;
    float duck_ = 0.8f;

    // Kick state
    double kickPhase_ = 0.0;   // accumulated phase in cycles
    double kickTime_ = 0.0;    // seconds since kick fired
    bool   kickActive_ = false;
    double prevPpq_ = 0.0;
    bool   prevRunning_ = false;
    bool   firstSample_ = true;

    // Reverb delay lines (sized in prepare)
    int    delayLen_[kLines] = {};
    int    delayIdx_[kLines] = {};
    float* delayBuf_[kLines] = {};
    double revLpState_[kLines] = {};  // one-pole low-pass per line

    // 24 dB/oct low-pass (tone) — four one-pole stages
    double lpTone_[4] = {};
    // 12 dB/oct low-pass (2x tone) — two one-pole stages
    double lpPost_[2] = {};

    // Duck state
    double duckGain_ = 1.0;
    double duckBeatsSinceDip_ = 0.0;  // beats since the 2ms dip ended
    bool   duckInDip_ = false;        // in the initial 2ms linear fall
    double duckDipSamples_ = 0.0;     // samples elapsed in the 2ms dip

    // Grid in beats
    double gridBeats() const;
    // Fire a ghost kick
    void fireKick();
    // One-pole low-pass: y += coeff * (x - y), returns y
    static double onePoleLp(double& state, double input, double coeff);
    // Flush denormals
    static double flush(double x);
};

}  // namespace undertow
