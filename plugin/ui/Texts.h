#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

struct DrawnText {
    juce::String text;
    juce::Rectangle<int> bounds;
    juce::Font font{juce::FontOptions{}};
};

namespace texts {

inline std::vector<DrawnText>*& record() {
    static std::vector<DrawnText>* activeRecord = nullptr;
    return activeRecord;
}

inline void draw(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> box,
                 juce::Justification justification) {
    const auto font = g.getCurrentFont();
    auto drawnBox = box;
    const auto textWidth = juce::roundToInt(std::ceil(juce::GlyphArrangement::getStringWidth(font, text)));

    if (textWidth > drawnBox.getWidth()) {
        const auto extra = textWidth - drawnBox.getWidth();
        if (justification.testFlags(juce::Justification::horizontallyCentred)) {
            const auto leftExtra = extra / 2;
            drawnBox.setLeft(drawnBox.getX() - leftExtra);
            drawnBox.setRight(drawnBox.getRight() + (extra - leftExtra));
        } else if (justification.testFlags(juce::Justification::right)) {
            drawnBox.setLeft(drawnBox.getX() - extra);
        } else {
            drawnBox.setRight(drawnBox.getRight() + extra);
        }
    }

    if (auto* activeRecord = record())
        activeRecord->push_back({text, drawnBox, font});

    g.saveState();
    g.setFont(font);

    juce::GlyphArrangement glyphs;
    glyphs.addFittedText(font, text, static_cast<float>(drawnBox.getX()), static_cast<float>(drawnBox.getY()),
                         static_cast<float>(drawnBox.getWidth()), static_cast<float>(drawnBox.getHeight()), justification,
                         1, 1.0f);
    glyphs.draw(g);

    g.restoreState();
}

} // namespace texts
