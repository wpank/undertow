#pragma once

#include <undertow/engine.h>

#include <array>

struct WindowFrame {
    float peak = 0.0f;
    float rms = 0.0f;
    bool clipping = false;
    int note = -1;
    float noteHz = 0.0f;
    int step = -1;
    long long count = 0;
    int root = -1;
    float rumble = 0.0f;
    double ppq = 0.0;
    double bpm = 120.0;
    bool hostClock = false;
    double sampleRate = 48000.0;
    std::array<float, undertow::kScopeSamples> scope{};
};
