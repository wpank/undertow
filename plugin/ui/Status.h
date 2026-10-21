#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "UiFrame.h"

class Status {
public:
    Status();

    void advance(const UiFrame& frame, double seconds);
    void paint(juce::Graphics& g);
    void paintOver(juce::Graphics& g);

private:
    UiFrame frame_{};
    double clockPhase_ = 0.0;
    double borderFlash_ = 0.0;
    double stripeOffset_ = 0.0;
    float meterNeedle_ = 0.0f;
    float rumbleNeedle_ = 0.0f;
};
