#pragma once

#include <array>

struct UiFrame {
    float energy = 0.0f;
    float peak = 0.0f;
    float rms = 0.0f;
    bool clipping = false;
    int note = -1;
    float noteHz = 0.0f;
    int step = -1;
    int length = 16;
    float resonance = 0.0f;
    float rumble = 0.0f;
    double ppq = 0.0;
    double bpm = 120.0;
    bool hostClock = false;
    std::array<float, 2048> scope{};
};

enum class StepEdit { ToggleGate, ToggleAccent, ToggleSlide, Up, Down };
