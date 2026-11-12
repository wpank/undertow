#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class Controls {
public:
    void attach(juce::AudioProcessorValueTreeState&, juce::Component&) {}
    void detach() {}
    juce::Component* controlFor(const juce::String&) const { return nullptr; }
    void setLight(int, float) {}
    void setColumnDim(int, bool) {}
    void setHostClock(bool) {}
    void setHover(const juce::String&) {}
    void paint(juce::Graphics&) {}
    void dropCache() {}
};
