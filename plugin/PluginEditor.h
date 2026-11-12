#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class UndertowProcessor;

class UndertowEditor final : public juce::AudioProcessorEditor {
public:
    explicit UndertowEditor(UndertowProcessor&);

    void paint(juce::Graphics&) override;

private:
    UndertowProcessor& processor_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UndertowEditor)
};
