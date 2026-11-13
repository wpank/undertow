#pragma once

#include "ui/Console.h"
#include "ui/Controls.h"
#include "ui/Frame.h"
#include "ui/Steps.h"
#include "ui/Texts.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <vector>

class UndertowProcessor;

class UndertowEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit UndertowEditor(UndertowProcessor&);
    ~UndertowEditor() override;

    WindowFrame captureFrame() const;
    void freeze();
    void advance(const WindowFrame& frame, double seconds);
    juce::Component* controlFor(const juce::String& parameterId) const;
    void hover(const juce::String& parameterId);
    std::vector<DrawnText> drawnTexts();
    juce::Rectangle<int> stepCellBounds(int step) const;
    int stepAt(juce::Point<int> point) const;
    juce::Rectangle<int> plotBounds() const;
    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;

private:
    void timerCallback() override;
    float parameterValue(int index) const;
    void toggleStepRest(int step);
    void paintGround(juce::Graphics&);

    UndertowProcessor& processor_;
    Controls controls_;
    Steps steps_;
    Console console_;
    std::array<float, 8> rememberedStepVelocities_{};
    juce::Image groundCache_;
    float groundCacheScale_ = 0.0f;
    double lastTickMs_ = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UndertowEditor)
};
