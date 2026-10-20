#include "undertow/rumble.h"
#include "undertow/params.h"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace undertow {
namespace {
constexpr double kPi = 3.14159265358979323846;
// Householder reflection: A = I - (2/N)*J, so feedback_i = gained_i - (2/N)*sum
constexpr double kHouseholderScale = 2.0 / 4.0;
} // namespace

// ── helpers ──────────────────────────────────────────────────────────

double Rumble::gridBeats() const {
    // rumble_rate choices: 0=1/4 (1 beat), 1=1/2 (2 beats), 2=1 bar (4 beats), 3=1/8 (0.5 beats)
    static constexpr double kGrid[] = {1.0, 2.0, 4.0, 0.5};
    return kGrid[rate_];
}

double Rumble::onePoleLp(double& state, double input, double coeff) {
    state += coeff * (input - state);
    return state;
}

double Rumble::flush(double x) {
    return (std::fabs(x) < kDenorm) ? 0.0 : x;
}

void Rumble::fireKick() {
    kickTime_ = 0.0;
    kickPhase_ = 0.0;
    kickActive_ = true;
    // Start duck dip
    duckInDip_ = true;
    duckDipSamples_ = 0.0;
}

// ── prepare / reset ──────────────────────────────────────────────────

void Rumble::prepare(double sampleRate) {
    sampleRate_ = std::max(sampleRate, 1.0);

    // Allocate delay lines for the reverb
    for (int i = 0; i < kLines; ++i) {
        delayLen_[i] = static_cast<int>(std::ceil(kLineDelays[i] * sampleRate_));
        delete[] delayBuf_[i];
        delayBuf_[i] = new float[delayLen_[i]]();
        delayIdx_[i] = 0;
        revLpState_[i] = 0.0;
    }

    reset();
}

void Rumble::reset() {
    // Kick
    kickPhase_ = 0.0;
    kickTime_ = 0.0;
    kickActive_ = false;
    prevPpq_ = 0.0;
    prevRunning_ = false;
    firstSample_ = true;

    // Reverb
    for (int i = 0; i < kLines; ++i) {
        if (delayBuf_[i]) std::memset(delayBuf_[i], 0, sizeof(float) * delayLen_[i]);
        delayIdx_[i] = 0;
        revLpState_[i] = 0.0;
    }

    // Filters
    for (auto& s : lpTone_) s = 0.0;
    for (auto& s : lpPost_) s = 0.0;

    // Duck
    duckGain_ = 1.0;
    duckInDip_ = false;
    duckDipSamples_ = 0.0;
    duckBeatsSinceDip_ = 0.0;
}

// ── setParams ────────────────────────────────────────────────────────

void Rumble::setParams(const float* values) {
    level_ = values[param::RumbleLevel];
    send_  = values[param::RumbleSend];
    kick_  = values[param::RumbleKick];
    rate_  = static_cast<int>(std::round(values[param::RumbleRate]));
    decay_ = values[param::RumbleDecay];
    tone_  = values[param::RumbleTone];
    drive_ = values[param::RumbleDrive];
    duck_  = values[param::RumbleDuck];
}

// ── process ──────────────────────────────────────────────────────────

float Rumble::process(float send, double ppq, double bpm, bool running) {
    const double invRate = 1.0 / sampleRate_;

    // ── Beat grid kick triggering ────────────────────────────────────
    if (running && kick_ > 0.0f) {
        const double grid = gridBeats();

        if (firstSample_) {
            // First running sample: fire if ppq is on a grid point (within half a sample)
            const double beatsPerSample = bpm / (60.0 * sampleRate_);
            const double halfSampleBeats = 0.5 * beatsPerSample;
            const double frac = std::fmod(ppq, grid);
            const double dist = std::min(frac, grid - frac);
            if (dist <= halfSampleBeats) {
                fireKick();
            }
        } else {
            // Check if ppq crossed a grid point since prevPpq_
            const double prevGrid = std::floor(prevPpq_ / grid);
            const double currGrid = std::floor(ppq / grid);
            if (currGrid > prevGrid) {
                fireKick();
            }
            // Also detect ppq landing exactly on a grid point (loop jump)
            // If prevPpq_ was on a grid and we're still on it, don't re-trigger.
            // But if ppq jumped backward to a grid point (loop), fire.
            if (ppq < prevPpq_ && !firstSample_) {
                // Backward jump (loop): fire if landing on grid
                const double beatsPerSample = bpm / (60.0 * sampleRate_);
                const double halfSampleBeats = 0.5 * beatsPerSample;
                const double frac = std::fmod(ppq, grid);
                const double dist = std::min(frac, grid - frac);
                if (dist <= halfSampleBeats) {
                    fireKick();
                }
            }
        }
    }

    prevPpq_ = ppq;
    prevRunning_ = running;
    firstSample_ = false;

    // ── Ghost kick synthesis ─────────────────────────────────────────
    double kickOut = 0.0;
    if (kickActive_) {
        // Pitch: 45 + 105 * exp(-t / 0.030)
        const double pitchHz = 45.0 + 105.0 * std::exp(-kickTime_ / 0.030);
        // Envelope: exp(-t / 0.080)
        const double env = std::exp(-kickTime_ / 0.080);
        // Accumulate phase
        kickPhase_ += pitchHz * invRate;
        kickOut = env * std::sin(2.0 * kPi * kickPhase_) * kick_;
        kickTime_ += invRate;
        if (kickTime_ >= 0.300) {
            kickActive_ = false;
        }
    }

    // ── Reverb ───────────────────────────────────────────────────────
    // Input = kick + send
    const double revInput = kickOut + static_cast<double>(send);

    // Per-line gain: 10^(-3 * delay / decay)
    // Low-pass cutoff ~1.5 kHz in each line
    const double lpCoeff = 1.0 - std::exp(-2.0 * kPi * 1500.0 * invRate);

    double revOut[kLines];
    double sum = 0.0;
    for (int i = 0; i < kLines; ++i) {
        // Read from delay line
        const float delayed = delayBuf_[i][delayIdx_[i]];

        // Low-pass filter in the feedback path
        const double lpOut = onePoleLp(revLpState_[i], static_cast<double>(delayed), lpCoeff);

        // Per-line gain
        const double lineGain = std::pow(10.0, -3.0 * kLineDelays[i] / static_cast<double>(decay_));
        const double gained = lpOut * lineGain;

        sum += gained;
        revOut[i] = gained;
    }

    // Householder feedback: output_i = gained_i - sum/N
    // Then write input + feedback into delay line
    for (int i = 0; i < kLines; ++i) {
        const double feedback = revOut[i] - sum * kHouseholderScale;
        delayBuf_[i][delayIdx_[i]] = static_cast<float>(revInput + feedback);
        delayIdx_[i] = (delayIdx_[i] + 1) % delayLen_[i];
    }

    // Sum reverb output
    double revSum = 0.0;
    for (int i = 0; i < kLines; ++i) {
        revSum += revOut[i];
    }

    // ── 24 dB/oct low-pass at rumble_tone ────────────────────────────
    const double toneCoeff = 1.0 - std::exp(-2.0 * kPi * static_cast<double>(tone_) * invRate);
    double filtered = revSum;
    for (int i = 0; i < 4; ++i) {
        filtered = onePoleLp(lpTone_[i], filtered, toneCoeff);
    }

    // ── Saturation: tanh(k*x)/k, k = 1 + 9*drive ───────────────────
    const double k = 1.0 + 9.0 * static_cast<double>(drive_);
    if (k > 1.0) {
        filtered = std::tanh(k * filtered) / k;
    }

    // ── 12 dB/oct low-pass at 2 * rumble_tone ───────────────────────
    const double postCoeff = 1.0 - std::exp(-2.0 * kPi * 2.0 * static_cast<double>(tone_) * invRate);
    for (int i = 0; i < 2; ++i) {
        filtered = onePoleLp(lpPost_[i], filtered, postCoeff);
    }

    // ── Duck ─────────────────────────────────────────────────────────
    const double grid = gridBeats();
    const double beatsPerSample = bpm / (60.0 * sampleRate_);

    if (duckInDip_) {
        // Linear fall from current duckGain_ toward (1 - duck_) over 2 ms
        // The dip takes 2 ms total; duckGain falls linearly to (1 - duck_)
        const double dipTarget = 1.0 - static_cast<double>(duck_);
        const double dipDurationSamples = 0.002 * sampleRate_;
        duckDipSamples_ += 1.0;
        if (duckDipSamples_ >= dipDurationSamples) {
            duckGain_ = dipTarget;
            duckInDip_ = false;
            duckBeatsSinceDip_ = 0.0;
        } else {
            // Linear interpolation from 1.0 to dipTarget
            // We start from whatever duckGain was when dip started
            // Actually, re-read spec: "the gain falls linearly to 1-duck within 2 ms"
            // This means it goes from its current value down to 1-duck in 2ms
            duckGain_ = 1.0 + (dipTarget - 1.0) * (duckDipSamples_ / dipDurationSamples);
        }
    } else if (duck_ > 0.0f && duckGain_ < 1.0) {
        // Recovery: 1 - duck * exp(-beats / (0.2 * grid))
        duckBeatsSinceDip_ += beatsPerSample;
        duckGain_ = 1.0 - static_cast<double>(duck_) * std::exp(-duckBeatsSinceDip_ / (0.2 * grid));
    }

    // ── Output ───────────────────────────────────────────────────────
    double output = filtered * duckGain_ * static_cast<double>(level_);

    // Flush denormals
    output = flush(output);

    // Flush filter states collectively
    {
        bool allSmall = true;
        for (auto s : lpTone_) if (std::fabs(s) >= kDenorm) { allSmall = false; break; }
        if (allSmall) for (auto& s : lpTone_) s = 0.0;
    }
    {
        bool allSmall = true;
        for (auto s : lpPost_) if (std::fabs(s) >= kDenorm) { allSmall = false; break; }
        if (allSmall) for (auto& s : lpPost_) s = 0.0;
    }
    for (int i = 0; i < kLines; ++i) {
        revLpState_[i] = flush(revLpState_[i]);
    }

    // If level is exactly 0, output exact silence
    if (level_ == 0.0f) return 0.0f;

    return static_cast<float>(output);
}

// ── duckGain ─────────────────────────────────────────────────────────

double Rumble::duckGain() const {
    return duckGain_;
}

}  // namespace undertow
