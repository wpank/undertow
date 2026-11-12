#pragma once

#include "Frame.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

class Console {
public:
    std::function<float(int parameterIndex)> value;
    void advance(const WindowFrame&, double) {}
    void paint(juce::Graphics&) {}
    void paintOver(juce::Graphics&) {}
    void dropCache() {}
    juce::Rectangle<int> plotBounds() const { return {}; }
};
