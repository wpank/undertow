#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include <undertow/sequencer.h>

#include "UiFrame.h"

class UnitArray final : public juce::Component {
public:
    std::function<undertow::Pattern()> pattern;
    std::function<void(int index, StepEdit edit)> onEdit;

    void advance(const UiFrame& frame, double seconds);
    juce::Rectangle<int> stepBounds(int index) const;
    int stepAt(juce::Point<int> point) const;
    static undertow::Pattern edited(undertow::Pattern pattern, int index, StepEdit edit);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

private:
    void triggerEdit(int index, StepEdit edit);
    void repaintStep(int index);
    StepEdit dragEditForEvent(const juce::MouseEvent& event) const;

    UiFrame frame_{};
    int hoverIndex_ = -1;
    int dragIndex_ = -1;
    int dragStartY_ = 0;
    int dragStartSemitones_ = 0;
    int dragAccumulatedSteps_ = 0;
    bool dragging_ = false;
    bool moved_ = false;
    int lastLitIndex_ = -1;
    double highlightAgeSeconds_ = 0.0;
};
