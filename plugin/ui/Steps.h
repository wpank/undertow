#pragma once

#include "Frame.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

class Steps final : public juce::Component {
public:
    std::function<float(int parameterIndex)> value;
    std::function<void(int step)> onClick;
    void advance(const WindowFrame&, double) {}
    float light(int) const { return 0.0f; }
    juce::Rectangle<int> cellBounds(int) const { return {}; }
    int stepAt(juce::Point<int>) const { return -1; }
    void dropCache() {}
};
