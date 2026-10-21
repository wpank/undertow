#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "UiFrame.h"

class Displays {
public:
    void advance(const UiFrame&, double) {}
    void paint(juce::Graphics&) {}
};
