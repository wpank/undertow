#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace undertow::ui::theme {

inline const juce::Colour bgVoid = juce::Colour::fromRGB(0x06, 0x06, 0x08);
inline const juce::Colour border = juce::Colour::fromRGB(0x18, 0x14, 0x20);
inline const juce::Colour rose = juce::Colour::fromRGB(0xAA, 0x70, 0x88);

inline juce::Font sansFont(float height, juce::Font::FontStyleFlags style = juce::Font::plain) {
    return juce::Font(juce::FontOptions(height, style));
}

inline juce::Font monoFont(float height, juce::Font::FontStyleFlags style = juce::Font::plain) {
    return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), height, style));
}

inline constexpr int editorWidth = 1200;
inline constexpr int editorHeight = 760;

} // namespace undertow::ui::theme
