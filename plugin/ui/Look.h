#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Texts.h"

class Look final : private juce::LookAndFeel_V4 {
public:
    Look();

    void install(juce::Component&);
    void uninstall(juce::Component&);
    void paint(juce::Graphics&);
    void drawStaticLayer(juce::Graphics&, float physicalScale);
    void dropCache();

private:
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;
    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;

    void ensureCache(juce::Graphics&);

    juce::LookAndFeel_V4 baseLook_;
    juce::Image cachedLayer_;
    juce::Point<int> cachedSize_{};
    float cachedScale_ = 0.0f;
};
