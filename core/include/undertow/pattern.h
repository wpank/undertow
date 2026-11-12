#pragma once

#include "undertow/params.h"

#include <array>

namespace undertow {

// A step as it plays: its pitch in semitones from the root, and its velocity from 0 to 1, where 0 is a rest.
struct Step {
    int pitch = 0;
    float velocity = 0.0f;
};

class Pattern {
public:
    void setParams(const float* values);  // kNumParams values in real units
    int length() const;                   // seq_length, 1 .. 8
    Step home(int index) const;           // step `index` (0 .. 7) as set, before the drift
    Step at(long long count, int root) const;  // what count `count` plays on MIDI note `root`, the drift included

private:
    std::array<Step, kSteps> home_{};
    int length_ = 1;
    float drift_ = 0.0f;
    int subOctave_ = 1;
};

}  // namespace undertow
