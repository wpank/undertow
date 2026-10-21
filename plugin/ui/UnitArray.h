#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include <undertow/sequencer.h>

#include "UiFrame.h"

class UnitArray final : public juce::Component {
public:
    std::function<undertow::Pattern()> pattern;
    std::function<void(int index, StepEdit edit)> onEdit;
    void advance(const UiFrame&, double) {}
    juce::Rectangle<int> stepBounds(int) const { return {}; }
    int stepAt(juce::Point<int>) const { return -1; }
    static undertow::Pattern edited(undertow::Pattern pattern, int, StepEdit) { return pattern; }
};
