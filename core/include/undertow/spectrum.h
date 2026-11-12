#pragma once

#include "undertow/engine.h"

#include <memory>

namespace undertow {

constexpr int kSpectrumSize = kScopeSamples;  // samples a frame analyses: the scope's length, kScopeSamples
constexpr int kSpectrumPoints = 220;          // the display's points, log-spaced from 20 Hz to 2 kHz

class Spectrum {
public:
    Spectrum();
    ~Spectrum();
    Spectrum(const Spectrum&) = delete;
    Spectrum& operator=(const Spectrum&) = delete;

    void setSampleRate(double sampleRate);    // then reset()
    void reset();                             // every point at the floor (-96 dB), the share at 0
    // One frame: the newest kSpectrumSize output samples, oldest first, `seconds` after the frame before.
    void analyse(const float* samples, double seconds);
    static double pointHz(int index);         // 20 * 100^(index / 219)
    float frameLevel(int index) const;        // this frame's band power in dB re a full-scale sine, at least -96
    float level(int index) const;             // frameLevel, smoothed: 25 ms rising, 300 ms falling
    float share() const;                      // the power from 20 to 150 Hz over the power above 20 Hz, smoothed 500 ms

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace undertow
