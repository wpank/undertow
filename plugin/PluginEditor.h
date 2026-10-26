#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ui/Displays.h"
#include "ui/Look.h"
#include "ui/Status.h"
#include "ui/Texts.h"
#include "ui/Theme.h"
#include "ui/UiFrame.h"
#include "ui/UnitArray.h"

#include <array>
#include <memory>
#include <vector>

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
    std::vector<DrawnText> drawnTexts();

    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void initialiseControls();
    void paintControlText(juce::Graphics&);

    UndertowProcessor& processor_;
    Look look_;
    Displays displays_;
    Status status_;
    UnitArray unitArray_;
    UiFrame currentFrame_{};
    double timerSeconds_ = 1.0 / 60.0;
    juce::uint32 lastTimerTickMs_ = 0;
    std::array<std::unique_ptr<juce::Slider>, undertow::kNumParams> sliders_{};
    std::array<std::unique_ptr<juce::ToggleButton>, undertow::kNumParams> buttons_{};
    std::array<std::unique_ptr<SliderAttachment>, undertow::kNumParams> sliderAttachments_{};
    std::array<std::unique_ptr<ButtonAttachment>, undertow::kNumParams> buttonAttachments_{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UndertowEditor)
};
