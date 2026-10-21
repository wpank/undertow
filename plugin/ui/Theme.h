#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

#include <undertow/params.h>

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

struct ControlCell {
    juce::Rectangle<int> bounds;
    juce::Rectangle<int> controlBounds;
};

struct ControlPanel {
    juce::String stamp;
    juce::Rectangle<int> area;
    std::array<int, 4> parameters{};
    int parameterCount = 0;
    std::array<ControlCell, 4> cells{};
};

inline const ControlPanel leftPanel{
    "OSC",
    leftLimbBounds,
    {undertow::param::OscWave, undertow::param::OscPw, undertow::param::OscLevel, undertow::param::OscOctave},
    4,
    {{{{16, 72, 120, 88}, {24, 80, 104, 72}},
      {{152, 72, 120, 88}, {160, 80, 104, 72}},
      {{16, 176, 120, 88}, {24, 184, 104, 72}},
      {{152, 176, 120, 88}, {160, 184, 104, 72}}}}
};

inline const ControlPanel leftLowerPanel{
    "FLT",
    leftLimbBounds,
    {undertow::param::OscFine, undertow::param::SubLevel, undertow::param::SubOctave, undertow::param::FilterCutoff},
    4,
    {{{{16, 280, 120, 88}, {24, 288, 104, 72}},
      {{152, 280, 120, 88}, {160, 288, 104, 72}},
      {{16, 384, 120, 88}, {24, 392, 104, 72}},
      {{152, 384, 120, 88}, {160, 392, 104, 72}}}}
};

inline const ControlPanel chestLeftPanel{
    "ENV",
    chestBounds,
    {undertow::param::FilterRes, undertow::param::FilterDrive, undertow::param::FilterEnv, undertow::param::FilterTrack},
    4,
    {{{{16, 72, 120, 88}, {24, 80, 104, 72}},
      {{152, 72, 120, 88}, {160, 80, 104, 72}},
      {{16, 176, 120, 88}, {24, 184, 104, 72}},
      {{152, 176, 120, 88}, {160, 184, 104, 72}}}}
};

inline const ControlPanel chestRightPanel{
    "AMP",
    chestBounds,
    {undertow::param::FiltAttack, undertow::param::FiltDecay, undertow::param::FiltSustain, undertow::param::FiltRelease},
    4,
    {{{{312, 72, 120, 88}, {320, 80, 104, 72}},
      {{448, 72, 120, 88}, {456, 80, 104, 72}},
      {{312, 176, 120, 88}, {320, 184, 104, 72}},
      {{448, 176, 120, 88}, {456, 184, 104, 72}}}}
};

inline const ControlPanel rightTopPanel{
    "LFO",
    rightLimbBounds,
    {undertow::param::AmpAttack, undertow::param::AmpDecay, undertow::param::AmpSustain, undertow::param::AmpRelease},
    4,
    {{{{904, 72, 120, 88}, {912, 80, 104, 72}},
      {{1040, 72, 120, 88}, {1048, 80, 104, 72}},
      {{904, 176, 120, 88}, {912, 184, 104, 72}},
      {{1040, 176, 120, 88}, {1048, 184, 104, 72}}}}
};

inline const ControlPanel rightBottomPanel{
    "RMB",
    rightLimbBounds,
    {undertow::param::Lfo1Rate, undertow::param::Lfo1Shape, undertow::param::Lfo1Cutoff, undertow::param::Lfo1Phase},
    4,
    {{{{904, 280, 120, 88}, {912, 288, 104, 72}},
      {{1040, 280, 120, 88}, {1048, 288, 104, 72}},
      {{904, 384, 120, 88}, {912, 392, 104, 72}},
      {{1040, 384, 120, 88}, {1048, 392, 104, 72}}}}
};

inline const ControlPanel lowerRightPanel{
    "SEQ",
    rightLimbBounds,
    {undertow::param::Lfo2Rate, undertow::param::Lfo2Shape, undertow::param::Lfo2Dest, undertow::param::Lfo2Depth},
    4,
    {{{{904, 488, 120, 88}, {912, 496, 104, 72}},
      {{1040, 488, 120, 88}, {1048, 496, 104, 72}},
      {{904, 592, 120, 88}, {912, 600, 104, 72}},
      {{1040, 592, 120, 88}, {1048, 600, 104, 72}}}}
};

inline const ControlPanel extraPanel{
    "MOD",
    rightLimbBounds,
    {undertow::param::GlideMs, undertow::param::GlideMode, undertow::param::Accent, undertow::param::BendRange},
    4,
    {{{{904, 696, 120, 88}, {912, 704, 104, 72}},
      {{1040, 696, 120, 88}, {1048, 704, 104, 72}},
      {{904, 800, 120, 88}, {912, 808, 104, 72}},
      {{1040, 800, 120, 88}, {1048, 808, 104, 72}}}}
};

inline const ControlPanel extraPanel2{
    "SEQ",
    rightLimbBounds,
    {undertow::param::Lfo2Phase, undertow::param::RumbleLevel, undertow::param::RumbleSend, undertow::param::RumbleKick},
    4,
    {{{{904, 904, 120, 88}, {912, 912, 104, 72}},
      {{1040, 904, 120, 88}, {1048, 912, 104, 72}},
      {{904, 1008, 120, 88}, {912, 1016, 104, 72}},
      {{1040, 1008, 120, 88}, {1048, 1016, 104, 72}}}}
};

inline const ControlPanel extraPanel3{
    "SEQ",
    rightLimbBounds,
    {undertow::param::RumbleRate, undertow::param::RumbleDecay, undertow::param::RumbleTone, undertow::param::RumbleDrive},
    4,
    {{{{904, 1112, 120, 88}, {912, 1120, 104, 72}},
      {{1040, 1112, 120, 88}, {1048, 1120, 104, 72}},
      {{904, 1216, 120, 88}, {912, 1224, 104, 72}},
      {{1040, 1216, 120, 88}, {1048, 1224, 104, 72}}}}
};

inline const ControlPanel extraPanel4{
    "SEQ",
    rightLimbBounds,
    {undertow::param::RumbleDuck, undertow::param::SatDrive, undertow::param::Width, undertow::param::MonoFreq},
    4,
    {{{{904, 1320, 120, 88}, {912, 1328, 104, 72}},
      {{1040, 1320, 120, 88}, {1048, 1328, 104, 72}},
      {{904, 1424, 120, 88}, {912, 1432, 104, 72}},
      {{1040, 1424, 120, 88}, {1048, 1432, 104, 72}}}}
};

inline const ControlPanel extraPanel5{
    "SEQ",
    rightLimbBounds,
    {undertow::param::MasterGain, undertow::param::SeqOn, undertow::param::SeqSync, undertow::param::SeqBpm},
    4,
    {{{{904, 1528, 120, 88}, {912, 1536, 104, 72}},
      {{1040, 1528, 120, 88}, {1048, 1536, 104, 72}},
      {{904, 1632, 120, 88}, {912, 1640, 104, 72}},
      {{1040, 1632, 120, 88}, {1048, 1640, 104, 72}}}}
};

inline const ControlPanel extraPanel6{
    "SEQ",
    rightLimbBounds,
    {undertow::param::SeqRate, undertow::param::SeqLength, undertow::param::SeqGate, undertow::param::SeqSwing},
    4,
    {{{{904, 1736, 120, 88}, {912, 1744, 104, 72}},
      {{1040, 1736, 120, 88}, {1048, 1744, 104, 72}},
      {{904, 1840, 120, 88}, {912, 1848, 104, 72}},
      {{1040, 1840, 120, 88}, {1048, 1848, 104, 72}}}}
};

inline const ControlPanel extraPanel7{
    "SEQ",
    rightLimbBounds,
    {undertow::param::SeqRoot, undertow::param::SeqScale, undertow::param::SeqMutate, undertow::param::SeqEvolve},
    4,
    {{{{904, 1944, 120, 88}, {912, 1952, 104, 72}},
      {{1040, 1944, 120, 88}, {1048, 1952, 104, 72}},
      {{904, 2048, 120, 88}, {912, 2056, 104, 72}},
      {{1040, 2048, 120, 88}, {1048, 2056, 104, 72}}}}
};

inline const ControlPanel extraPanel8{
    "SEQ",
    rightLimbBounds,
    {undertow::param::SeqSeed, undertow::param::Lfo2Phase, undertow::param::RumbleLevel, undertow::param::RumbleSend},
    4,
    {{{{904, 2152, 120, 88}, {912, 2160, 104, 72}},
      {{1040, 2152, 120, 88}, {1048, 2160, 104, 72}},
      {{904, 2256, 120, 88}, {912, 2264, 104, 72}},
      {{1040, 2256, 120, 88}, {1048, 2264, 104, 72}}}}
};

inline const ControlPanel controlPanels[] = {
    leftPanel, leftLowerPanel, chestLeftPanel, chestRightPanel, rightTopPanel, rightBottomPanel, lowerRightPanel, extraPanel, extraPanel2, extraPanel3, extraPanel4, extraPanel5, extraPanel6, extraPanel7, extraPanel8
};

} // namespace undertow::ui::theme
