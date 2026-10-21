#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "UiFrame.h"

class Status {
public:
    void advance(const UiFrame&, double) {}
    void paint(juce::Graphics&) {}
    void paintOver(juce::Graphics&) {}
};
