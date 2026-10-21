#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace undertow::ui::theme {

inline const juce::Colour bgVoid = juce::Colour::fromRGB(0x06, 0x06, 0x08);
inline const juce::Colour bgRaised = juce::Colour::fromRGB(0x0C, 0x0A, 0x0E);
inline const juce::Colour bgMid = juce::Colour::fromRGB(0x08, 0x08, 0x10);
inline const juce::Colour border = juce::Colour::fromRGB(0x18, 0x14, 0x20);
inline const juce::Colour borderActive = juce::Colour::fromFloatRGBA(170.0f / 255.0f, 112.0f / 255.0f, 136.0f / 255.0f, 0.27f);
inline const juce::Colour rose = juce::Colour::fromRGB(0xAA, 0x70, 0x88);
inline const juce::Colour roseBright = juce::Colour::fromRGB(0xCC, 0x90, 0xA8);
inline const juce::Colour roseDim = juce::Colour::fromRGB(0x7A, 0x50, 0x60);
inline const juce::Colour roseEmber = juce::Colour::fromRGB(0x48, 0x28, 0x38);
inline const juce::Colour roseDeep = juce::Colour::fromRGB(0x3A, 0x20, 0x30);
inline const juce::Colour bone = juce::Colour::fromRGB(0xC8, 0xB8, 0x90);
inline const juce::Colour boneDim = juce::Colour::fromRGB(0x8A, 0x7A, 0x5A);
inline const juce::Colour textPrimary = juce::Colour::fromRGB(0x98, 0x80, 0x90);
inline const juce::Colour textDim = juce::Colour::fromRGB(0x58, 0x48, 0x58);
inline const juce::Colour textGhost = juce::Colour::fromRGB(0x30, 0x28, 0x30);
inline const juce::Colour textPhantom = juce::Colour::fromRGB(0x20, 0x18, 0x20);
inline const juce::Colour dream = juce::Colour::fromRGB(0x58, 0x58, 0x78);
inline const juce::Colour scanlineDark = juce::Colour::fromFloatRGBA(5.0f / 255.0f, 5.0f / 255.0f, 7.0f / 255.0f, 0.12f);

inline juce::Font sansFont(float height, juce::Font::FontStyleFlags style = juce::Font::plain) {
    return juce::Font(juce::FontOptions(height, style));
}

inline juce::Font monoFont(float height, juce::Font::FontStyleFlags style = juce::Font::plain) {
    return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), height, style));
}

inline constexpr int editorWidth = 1200;
inline constexpr int editorHeight = 760;
inline const juce::Rectangle<int> headBounds{0, 0, 1200, 48};
inline const juce::Rectangle<int> leftLimbBounds{16, 56, 280, 464};
inline const juce::Rectangle<int> chestBounds{312, 56, 576, 464};
inline const juce::Rectangle<int> rightLimbBounds{904, 56, 280, 464};
inline const juce::Rectangle<int> gutBounds{16, 528, 1168, 136};
inline const juce::Rectangle<int> groundBounds{0, 672, 1200, 88};
inline const juce::Rectangle<int> topHazardBounds{0, 0, 1200, 16};
inline const juce::Rectangle<int> bottomHazardBounds{0, 744, 1200, 16};
inline const juce::Rectangle<int> scopeBounds{360, 188, 480, 200};
inline const juce::Colour heatRamp[4] = {bgVoid, roseDeep, roseEmber, rose};

} // namespace undertow::ui::theme
