#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class Look {
public:
    void install(juce::Component&) {}
    void uninstall(juce::Component&) {}
    void paint(juce::Graphics&) {}
};
