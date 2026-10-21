#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ui/Displays.h"
#include "ui/Look.h"
#include "ui/Status.h"
#include "ui/UiFrame.h"
#include "ui/UnitArray.h"

class UndertowProcessor;

class UndertowEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit UndertowEditor(UndertowProcessor&);
    ~UndertowEditor() override;

    UiFrame captureFrame() const;
    void freeze();
    void advance(const UiFrame& frame, double seconds);
    juce::Rectangle<int> chestBounds() const;
    juce::Rectangle<int> scopeBounds() const;
    juce::Rectangle<int> stepBounds(int index) const;
    int stepAt(juce::Point<int> point) const;
    void editStep(int index, StepEdit edit);
    juce::Component* controlFor(const juce::String& parameterId) const;

    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;

private:
    void timerCallback() override;

    UndertowProcessor& processor_;
    Look look_;
    Displays displays_;
    Status status_;
    UnitArray unitArray_;
    UiFrame currentFrame_{};
    double timerSeconds_ = 1.0 / 60.0;
    juce::uint32 lastTimerTickMs_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UndertowEditor)
};
