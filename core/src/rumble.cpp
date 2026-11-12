#include "undertow/rumble.h"
#include "undertow/params.h"

#include <algorithm>
#include <cmath>

namespace undertow {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kAttackSeconds = 0.008;
constexpr double kDuckDecaySeconds = 0.150;
constexpr double kGhostKickSeconds = 0.300;
constexpr double kOutputTrim = 0.67;
constexpr double kRumbleHpHz = 30.0;
constexpr double kFlushThreshold = 1e-20;
constexpr double kButterworthQ1 = 0.5411961;
constexpr double kButterworthQ2 = 1.3065630;
constexpr double kPreDelaySeconds = 0.012;
constexpr int kAllpassBaseSamples[4] = {142, 107, 379, 277};
constexpr double kAllpassGains[4] = {0.7, 0.7, 0.65, 0.65};
constexpr int kLineBaseSamples[8] = {2903, 3511, 4229, 4801, 5521, 6091, 6763, 7417};
constexpr double kLineRates[8] = {0.11, 0.13, 0.17, 0.19, 0.23, 0.29, 0.31, 0.37};

std::array<double, 5> makeLowpass(double sampleRate, double cutoffHz, double q) {
    std::array<double, 5> coeffs{};
    const double limited = std::clamp(cutoffHz, 1.0, 0.499 * sampleRate);
    const double w0 = 2.0 * kPi * limited / sampleRate;
    const double cosW0 = std::cos(w0);
    const double sinW0 = std::sin(w0);
    const double alpha = sinW0 / (2.0 * q);
    const double a0 = 1.0 + alpha;
    coeffs[0] = ((1.0 - cosW0) * 0.5) / a0;
    coeffs[1] = (1.0 - cosW0) / a0;
    coeffs[2] = coeffs[0];
    coeffs[3] = (-2.0 * cosW0) / a0;
    coeffs[4] = (1.0 - alpha) / a0;
    return coeffs;
}

std::array<double, 5> makeHighpass(double sampleRate, double cutoffHz, double q) {
    std::array<double, 5> coeffs{};
    const double limited = std::clamp(cutoffHz, 1.0, 0.499 * sampleRate);
    const double w0 = 2.0 * kPi * limited / sampleRate;
    const double cosW0 = std::cos(w0);
    const double sinW0 = std::sin(w0);
    const double alpha = sinW0 / (2.0 * q);
    const double a0 = 1.0 + alpha;
    coeffs[0] = ((1.0 + cosW0) * 0.5) / a0;
    coeffs[1] = (-(1.0 + cosW0)) / a0;
    coeffs[2] = coeffs[0];
    coeffs[3] = (-2.0 * cosW0) / a0;
    coeffs[4] = (1.0 - alpha) / a0;
    return coeffs;
}

double processBiquad(const std::array<double, 5>& coeffs, std::array<double, 2>& state, double x) {
    const double y = coeffs[0] * x + state[0];
    state[0] = coeffs[1] * x - coeffs[3] * y + state[1];
    state[1] = coeffs[2] * x - coeffs[4] * y;
    return y;
}

void flushBiquadState(std::array<double, 2>& state) {
    if (std::fabs(state[0]) < kFlushThreshold)
        state[0] = 0.0;
    if (std::fabs(state[1]) < kFlushThreshold)
        state[1] = 0.0;
}

int scaledSamples(int baseSamples, double sampleRate) {
    return std::max(1, static_cast<int>(std::lround(static_cast<double>(baseSamples) * sampleRate / 48000.0)));
}

double readDelay(const std::vector<float>& data, int writeIndex) {
    return static_cast<double>(data[static_cast<std::size_t>(writeIndex)]);
}

double readInterpolatedDelay(const std::vector<float>& data, int writeIndex, double delaySamples) {
    const double size = static_cast<double>(data.size());
    double read = static_cast<double>(writeIndex) - delaySamples;
    while (read < 0.0)
        read += size;
    while (read >= size)
        read -= size;

    const int index0 = static_cast<int>(read);
    const int index1 = (index0 + 1) % static_cast<int>(size);
    const double frac = read - static_cast<double>(index0);
    return static_cast<double>(data[static_cast<std::size_t>(index0)]) * (1.0 - frac) +
           static_cast<double>(data[static_cast<std::size_t>(index1)]) * frac;
}

void writeDelay(std::vector<float>& data, int& writeIndex, int wrapSize, double x) {
    data[static_cast<std::size_t>(writeIndex)] = static_cast<float>(std::fabs(x) < kFlushThreshold ? 0.0 : x);
    writeIndex = (writeIndex + 1) % wrapSize;
}

void hadamard8(double* values) {
    for (int step = 1; step < 8; step <<= 1) {
        for (int i = 0; i < 8; i += step << 1) {
            for (int j = 0; j < step; ++j) {
                const double a = values[i + j];
                const double b = values[i + j + step];
                values[i + j] = a + b;
                values[i + j + step] = a - b;
            }
        }
    }

    constexpr double kScale = 0.3535533905932738; // 1 / sqrt(8)
    for (int i = 0; i < 8; ++i)
        values[i] *= kScale;
}

} // namespace

double Rumble::gridBeats() const {
    static constexpr double kGrid[] = {1.0, 2.0, 4.0, 0.5};
    return kGrid[std::clamp(rate_, 0, 3)];
}

double Rumble::flush(double x) {
    return std::fabs(x) < kDenorm ? 0.0 : x;
}

void Rumble::fireKick() {
    kickTime_ = 0.0;
    kickPhase_ = 0.0;
    kickActive_ = kick_ > 0.0f;
    duckStartEnvelope_ = duckEnvelope_;
    duckTime_ = 0.0;
    duckActive_ = duck_ > 0.0f;
}

void Rumble::updateToneFilters() {
    rumbleHpCoeff_ = makeHighpass(sampleRate_, kRumbleHpHz, 1.0 / std::sqrt(2.0));
    toneCoeff_[0] = makeLowpass(sampleRate_, static_cast<double>(tone_), kButterworthQ1);
    toneCoeff_[1] = makeLowpass(sampleRate_, static_cast<double>(tone_), kButterworthQ2);
    preparedTone_ = tone_;
}

void Rumble::updateDecayGains() {
    const double decaySeconds = std::max(static_cast<double>(decay_), 1e-3);
    for (int i = 0; i < kLines; ++i) {
        const double delaySeconds = static_cast<double>(lineDelay_[i]) / sampleRate_;
        lineGain_[i] = std::pow(10.0, -3.0 * delaySeconds / decaySeconds);
    }
    preparedDecay_ = decay_;
}

void Rumble::prepare(double sampleRate) {
    sampleRate_ = std::max(sampleRate, 1.0);

    preDelaySamples_ = std::max(1, static_cast<int>(std::lround(kPreDelaySeconds * sampleRate_)));
    preDelayData_.assign(static_cast<std::size_t>(preDelaySamples_), 0.0f);
    preDelayWrite_ = 0;

    for (int i = 0; i < kAllpasses; ++i) {
        allpassDelay_[i] = scaledSamples(kAllpassBaseSamples[i], sampleRate_);
        allpassData_[i].assign(static_cast<std::size_t>(allpassDelay_[i]), 0.0f);
        allpassWrite_[i] = 0;
        allpassGains_[i] = kAllpassGains[i];
    }

    for (int i = 0; i < kLines; ++i) {
        lineDelay_[i] = scaledSamples(kLineBaseSamples[i], sampleRate_);
        lineData_[i].assign(static_cast<std::size_t>(lineDelay_[i] + 42), 0.0f);
        lineWrite_[i] = 0;
        linePhase_[i] = 2.0 * kPi * static_cast<double>(i) / static_cast<double>(kLines);
        linePhaseDelta_[i] = 2.0 * kPi * kLineRates[i] / sampleRate_;
        lineGain_[i] = 0.0;
    }

    preparedTone_ = -1.0;
    preparedDecay_ = -1.0;
    updateToneFilters();
    updateDecayGains();
    reset();
}

void Rumble::reset() {
    kickPhase_ = 0.0;
    kickTime_ = 0.0;
    kickActive_ = false;
    prevPpq_ = 0.0;
    prevRunning_ = false;
    firstSample_ = true;

    std::fill(preDelayData_.begin(), preDelayData_.end(), 0.0f);
    preDelayWrite_ = 0;
    for (int i = 0; i < kAllpasses; ++i) {
        std::fill(allpassData_[i].begin(), allpassData_[i].end(), 0.0f);
        allpassWrite_[i] = 0;
    }
    for (int i = 0; i < kLines; ++i) {
        std::fill(lineData_[i].begin(), lineData_[i].end(), 0.0f);
        lineWrite_[i] = 0;
        linePhase_[i] = 2.0 * kPi * static_cast<double>(i) / static_cast<double>(kLines);
    }

    rumbleHpState_ = {0.0, 0.0};
    toneState_[0] = {0.0, 0.0};
    toneState_[1] = {0.0, 0.0};

    duckGain_ = 1.0;
    duckEnvelope_ = 0.0;
    duckStartEnvelope_ = 0.0;
    duckTime_ = 0.0;
    duckActive_ = false;
}

void Rumble::setParams(const float* values) {
    level_ = values[param::RumbleLevel];
    send_  = values[param::RumbleSend];
    kick_  = values[param::RumbleKick];
    rate_  = static_cast<int>(std::lround(values[param::RumbleRate]));
    decay_ = values[param::RumbleDecay];
    tone_  = values[param::RumbleTone];
    drive_ = values[param::RumbleDrive];
    duck_  = values[param::RumbleDuck];

    if (static_cast<double>(tone_) != preparedTone_)
        updateToneFilters();
    if (static_cast<double>(decay_) != preparedDecay_)
        updateDecayGains();
}

float Rumble::process(float send, double ppq, double bpm, bool running) {
    const double invRate = 1.0 / sampleRate_;
    const double beatsPerSample = bpm / (60.0 * sampleRate_);
    const double halfSampleBeats = 0.5 * beatsPerSample;
    const double grid = gridBeats();
    const auto nearGrid = [&](double position) {
        double frac = std::fmod(position, grid);
        if (frac < 0.0)
            frac += grid;
        const double dist = std::min(frac, grid - frac);
        return dist <= halfSampleBeats;
    };

    bool trigger = false;
    if (running) {
        if ((firstSample_ || !prevRunning_) && nearGrid(ppq)) {
            trigger = true;
        } else if (!firstSample_) {
            if (ppq < prevPpq_) {
                trigger = nearGrid(ppq);
            } else if (std::floor(ppq / grid) > std::floor(prevPpq_ / grid)) {
                trigger = true;
            }
        }
    }
    if (trigger)
        fireKick();

    double kickOut = 0.0;
    if (kickActive_) {
        const double pitchHz = 45.0 + 105.0 * std::exp(-kickTime_ / 0.030);
        const double envelope = std::exp(-kickTime_ / 0.080);
        kickOut = envelope * std::sin(2.0 * kPi * kickPhase_) * static_cast<double>(kick_);
        kickPhase_ += pitchHz * invRate;
        kickTime_ += invRate;
        if (kickTime_ >= kGhostKickSeconds) {
            kickActive_ = false;
            kickTime_ = kGhostKickSeconds;
        }
    }

    double diffused = readDelay(preDelayData_, preDelayWrite_);
    writeDelay(preDelayData_, preDelayWrite_, preDelaySamples_, kickOut + static_cast<double>(send));

    for (int i = 0; i < kAllpasses; ++i) {
        const double delayed = readDelay(allpassData_[i], allpassWrite_[i]);
        const double w = diffused + allpassGains_[i] * delayed;
        diffused = delayed - allpassGains_[i] * w;
        writeDelay(allpassData_[i], allpassWrite_[i], allpassDelay_[i], w);
    }

    double lineOut[kLines] = {};
    for (int i = 0; i < kLines; ++i) {
        const double modulatedDelay = static_cast<double>(lineDelay_[i]) + 40.0 * std::sin(linePhase_[i]);
        lineOut[i] = readInterpolatedDelay(lineData_[i], lineWrite_[i], modulatedDelay) * lineGain_[i];
        linePhase_[i] += linePhaseDelta_[i];
        if (linePhase_[i] >= 2.0 * kPi)
            linePhase_[i] -= 2.0 * kPi;
    }

    double feedback[kLines];
    std::copy(std::begin(lineOut), std::end(lineOut), feedback);
    hadamard8(feedback);
    for (int i = 0; i < kLines; ++i)
        writeDelay(lineData_[i], lineWrite_[i], static_cast<int>(lineData_[i].size()), diffused + feedback[i]);

    double room = (lineOut[0] - lineOut[3] + lineOut[4] - lineOut[7]) * 0.125;
    room = processBiquad(rumbleHpCoeff_, rumbleHpState_, room);
    const double driveGain = 0.5 + 3.4 * static_cast<double>(drive_);
    room = std::tanh(driveGain * 6.0 * room) / std::tanh(driveGain);
    room = processBiquad(toneCoeff_[0], toneState_[0], room);
    room = processBiquad(toneCoeff_[1], toneState_[1], room);

    if (duck_ <= 0.0f) {
        duckEnvelope_ = 0.0;
        duckStartEnvelope_ = 0.0;
        duckActive_ = false;
        duckTime_ = 0.0;
        duckGain_ = 1.0;
    } else {
        if (duckActive_) {
            if (duckTime_ < kAttackSeconds) {
                const double phase = std::clamp(duckTime_ / kAttackSeconds, 0.0, 1.0);
                const double rise = 0.5 - 0.5 * std::cos(kPi * phase);
                duckEnvelope_ = duckStartEnvelope_ + (1.0 - duckStartEnvelope_) * rise;
            } else {
                duckEnvelope_ = std::exp(-(duckTime_ - kAttackSeconds) / kDuckDecaySeconds);
                if (duckEnvelope_ <= kDenorm) {
                    duckEnvelope_ = 0.0;
                    duckActive_ = false;
                }
            }
            duckTime_ += invRate;
        } else {
            duckEnvelope_ = 0.0;
        }
        duckGain_ = 1.0 - static_cast<double>(duck_) * duckEnvelope_;
    }

    room = flush(room);
    if (std::fabs(duckGain_ - 1.0) < kDenorm)
        duckGain_ = 1.0;
    flushBiquadState(rumbleHpState_);
    flushBiquadState(toneState_[0]);
    flushBiquadState(toneState_[1]);

    prevPpq_ = ppq;
    prevRunning_ = running;
    firstSample_ = false;

    if (level_ == 0.0f)
        return 0.0f;

    return static_cast<float>(room * duckGain_ * static_cast<double>(level_) * kOutputTrim);
}

double Rumble::duckGain() const {
    return duckGain_;
}

}  // namespace undertow
