#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <cstddef>

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
inline const juce::Rectangle<int> pumpFieldBounds{912, 80, 264, 96};
inline const juce::Colour heatRamp[4] = {bgVoid, roseDeep, roseEmber, rose};

inline constexpr int cellSize = 64;
inline constexpr int limbCellSpacing = 68;
inline constexpr int groundCellSpacing = 56;
inline constexpr int labelHeight = 10;
inline constexpr int readoutHeight = 12;
inline constexpr int labelGap = 0;
inline constexpr int controlSize = 40;
inline constexpr int stampHeight = 14;
inline constexpr int stampWidth = 84;

struct ControlCell {
    juce::Rectangle<int> bounds;
    juce::Rectangle<int> controlBounds;
    juce::Rectangle<int> labelBounds() const {
        return {bounds.getX(), controlBounds.getBottom() + labelGap - 4, bounds.getWidth(), labelHeight};
    }
    juce::Rectangle<int> readoutBounds() const {
        return {bounds.getX(), controlBounds.getBottom() + labelGap + labelHeight, bounds.getWidth(), readoutHeight};
    }
};

struct ControlSlot {
    int parameter = -1;
    const char* label = "";
    ControlCell cell{};
    bool isLamp = false;
};

struct ControlPanel {
    juce::String stamp;
    juce::Rectangle<int> area;
    juce::Rectangle<int> stampBounds;
    std::array<ControlSlot, 12> controls{};
    int controlCount = 0;
};

inline ControlCell makeCell(int x, int y, int controlW = controlSize, int controlH = controlSize) {
    const juce::Rectangle<int> bounds{x, y, cellSize, cellSize};
    const juce::Rectangle<int> controlBounds{x + (cellSize - controlW) / 2, y + (cellSize - controlH) / 2, controlW, controlH};
    return {bounds, controlBounds};
}

inline constexpr int limbColumnX(int panelX, int column) { return panelX + 4 + column * limbCellSpacing; }
inline constexpr int groundColumnX(int column) { return 40 + column * groundCellSpacing; }

inline const ControlPanel oscPanel{
    "OSC/01",
    {16, 56, 280, 160},
    {24, 64, stampWidth, stampHeight},
    {{
        {undertow::param::OscWave, "WAVE", makeCell(limbColumnX(16, 0), 100)},
        {undertow::param::OscPw, "PW", makeCell(limbColumnX(16, 1), 100)},
        {undertow::param::OscLevel, "LEVEL", makeCell(limbColumnX(16, 2), 100)},
        {undertow::param::OscOctave, "OCT", makeCell(limbColumnX(16, 3), 100)},
        {undertow::param::OscFine, "FINE", makeCell(limbColumnX(16, 0), 152)},
        {undertow::param::SubLevel, "SUB", makeCell(limbColumnX(16, 1), 152)},
        {undertow::param::SubOctave, "SUB OCT", makeCell(limbColumnX(16, 2), 152)},
    }},
    7
};

inline const ControlPanel filterPanel{
    "FILTER/02",
    {16, 216, 280, 144},
    {24, 224, stampWidth, stampHeight},
    {{
        {undertow::param::FilterCutoff, "CUTOFF", makeCell(limbColumnX(16, 0), 244)},
        {undertow::param::FilterRes, "RES", makeCell(limbColumnX(16, 1), 244)},
        {undertow::param::FilterDrive, "DRIVE", makeCell(limbColumnX(16, 2), 244)},
        {undertow::param::FilterEnv, "ENV", makeCell(limbColumnX(16, 3), 244)},
        {undertow::param::FilterTrack, "TRACK", makeCell(limbColumnX(16, 0), 292)},
    }},
    5
};

inline const ControlPanel envPanel{
    "ENV/03",
    {16, 360, 280, 160},
    {24, 368, stampWidth, stampHeight},
    {{
        {undertow::param::AmpAttack, "A", makeCell(limbColumnX(16, 0), 404)},
        {undertow::param::AmpDecay, "D", makeCell(limbColumnX(16, 1), 404)},
        {undertow::param::AmpSustain, "S", makeCell(limbColumnX(16, 2), 404)},
        {undertow::param::AmpRelease, "R", makeCell(limbColumnX(16, 3), 404)},
        {undertow::param::FiltAttack, "F A", makeCell(limbColumnX(16, 0), 460)},
        {undertow::param::FiltDecay, "F D", makeCell(limbColumnX(16, 1), 460)},
        {undertow::param::FiltSustain, "F S", makeCell(limbColumnX(16, 2), 460)},
        {undertow::param::FiltRelease, "F R", makeCell(limbColumnX(16, 3), 460)},
    }},
    8
};

inline const ControlPanel rumblePanel{
    "RUMBLE/04",
    {904, 56, 280, 264},
    {912, 64, stampWidth, stampHeight},
    {{
        {undertow::param::RumbleLevel, "LEVEL", makeCell(limbColumnX(904, 0), 180)},
        {undertow::param::RumbleSend, "SEND", makeCell(limbColumnX(904, 1), 180)},
        {undertow::param::RumbleKick, "KICK", makeCell(limbColumnX(904, 2), 180)},
        {undertow::param::RumbleRate, "RATE", makeCell(limbColumnX(904, 3), 180)},
        {undertow::param::RumbleDecay, "DECAY", makeCell(limbColumnX(904, 0), 248)},
        {undertow::param::RumbleTone, "TONE", makeCell(limbColumnX(904, 1), 248)},
        {undertow::param::RumbleDrive, "DRIVE", makeCell(limbColumnX(904, 2), 248)},
        {undertow::param::RumbleDuck, "DUCK", makeCell(limbColumnX(904, 3), 248)},
    }},
    8
};

inline const ControlPanel modPanel{
    "MOD/05",
    {904, 320, 280, 200},
    {912, 328, stampWidth, stampHeight},
    {{
        {undertow::param::Lfo1Rate, "1 RATE", makeCell(limbColumnX(904, 0), 356)},
        {undertow::param::Lfo1Shape, "1 SHAPE", makeCell(limbColumnX(904, 1), 356)},
        {undertow::param::Lfo1Cutoff, "1 CUT", makeCell(limbColumnX(904, 2), 356)},
        {undertow::param::Lfo1Phase, "1 PHASE", makeCell(limbColumnX(904, 3), 356)},
        {undertow::param::Lfo2Rate, "2 RATE", makeCell(limbColumnX(904, 0), 424)},
        {undertow::param::Lfo2Shape, "2 SHAPE", makeCell(limbColumnX(904, 1), 412)},
        {undertow::param::Lfo2Dest, "2 DEST", makeCell(limbColumnX(904, 2), 412)},
        {undertow::param::Lfo2Depth, "2 DEPTH", makeCell(limbColumnX(904, 3), 412)},
        {undertow::param::Lfo2Phase, "2 PHASE", makeCell(limbColumnX(904, 3), 456, 32, 32)},
    }},
    9
};

inline const ControlPanel groundSeqPanel{
    "SEQ/06",
    groundBounds,
    {24, 536, stampWidth, stampHeight},
    {{
        {undertow::param::SeqOn, "SEQ", makeCell(groundColumnX(8), 684, 40, 24), true},
        {undertow::param::SeqSync, "SYNC", makeCell(groundColumnX(9), 684)},
        {undertow::param::SeqBpm, "BPM", makeCell(groundColumnX(10), 684)},
        {undertow::param::SeqRate, "RATE", makeCell(groundColumnX(11), 684)},
        {undertow::param::SeqLength, "LEN", makeCell(groundColumnX(12), 684)},
        {undertow::param::SeqGate, "GATE", makeCell(groundColumnX(13), 684)},
        {undertow::param::SeqSwing, "SWING", makeCell(groundColumnX(14), 684)},
        {undertow::param::SeqRoot, "ROOT", makeCell(groundColumnX(15), 684)},
        {undertow::param::SeqScale, "SCALE", makeCell(groundColumnX(16), 684)},
        {undertow::param::SeqMutate, "MUTATE", makeCell(groundColumnX(17), 684)},
        {undertow::param::SeqEvolve, "EVOLVE", makeCell(groundColumnX(18), 684)},
        {undertow::param::SeqSeed, "SEED", makeCell(groundColumnX(19), 684)},
    }},
    12
};

inline const ControlPanel voicePanel{
    "VOICE/07",
    groundBounds,
    {40, 680, stampWidth, stampHeight},
    {{
        {undertow::param::GlideMs, "GLIDE", makeCell(groundColumnX(0), 684)},
        {undertow::param::GlideMode, "MODE", makeCell(groundColumnX(1), 684)},
        {undertow::param::Accent, "ACCENT", makeCell(groundColumnX(2), 684)},
        {undertow::param::BendRange, "BEND", makeCell(groundColumnX(3), 684)},
    }},
    4
};

inline const ControlPanel outPanel{
    "OUT/08",
    groundBounds,
    {264, 680, stampWidth, stampHeight},
    {{
        {undertow::param::SatDrive, "SAT", makeCell(groundColumnX(4), 684)},
        {undertow::param::Width, "WIDTH", makeCell(groundColumnX(5), 684)},
        {undertow::param::MonoFreq, "MONO", makeCell(groundColumnX(6), 684)},
        {undertow::param::MasterGain, "MASTER", makeCell(groundColumnX(7), 684)},
    }},
    4
};

inline const ControlPanel controlPanels[] = {
    oscPanel,
    filterPanel,
    envPanel,
    rumblePanel,
    modPanel,
    voicePanel,
    outPanel,
    groundSeqPanel,
};

inline const ControlPanel* findPanelForParameter(int parameter) {
    for (const auto& panel : controlPanels) {
        for (int i = 0; i < panel.controlCount; ++i) {
            if (panel.controls[static_cast<std::size_t>(i)].parameter == parameter)
                return &panel;
        }
    }
    return nullptr;
}

inline const ControlSlot* findSlotForParameter(int parameter) {
    for (const auto& panel : controlPanels) {
        for (int i = 0; i < panel.controlCount; ++i) {
            const auto& slot = panel.controls[static_cast<std::size_t>(i)];
            if (slot.parameter == parameter)
                return &slot;
        }
    }
    return nullptr;
}

} // namespace undertow::ui::theme
