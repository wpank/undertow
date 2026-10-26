#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

struct DrawnText {
    juce::String text;
    juce::Rectangle<int> bounds;
};

namespace texts {

inline std::vector<DrawnText>*& record() {
    static std::vector<DrawnText>* activeRecord = nullptr;
    return activeRecord;
}

inline void draw(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> box,
                 juce::Justification justification) {
    if (auto* activeRecord = record())
        activeRecord->push_back({text, box});
    g.drawText(text, box, justification, false);
}

} // namespace texts
