#pragma once

#include <UndertowFonts.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <cmath>

namespace undertow::ui::theme {

inline const juce::Colour bgVoid{0xff060608};
inline const juce::Colour bgMid{0xff080810};
inline const juce::Colour glass{0xff070609};
inline const juce::Colour rule{0xff1e1620};
inline const juce::Colour rule2{0xff30222e};
inline const juce::Colour rose{0xffaa7088};
inline const juce::Colour roseBright{0xffcc90a8};
inline const juce::Colour roseDim{0xff7a5060};
inline const juce::Colour roseEmber{0xff482838};
inline const juce::Colour roseDeep{0xff3a2030};
inline const juce::Colour legend{0xff8e6474};
inline const juce::Colour value{0xff988090};
inline const juce::Colour track{0xff2e1b28};
inline const juce::Colour pointer{0xffa8829a};
inline const juce::Colour capHi{0xff1d151c};
inline const juce::Colour capMid{0xff120d12};
inline const juce::Colour capLo{0xff0a070a};
inline const juce::Colour capRim{0xff2e202c};
inline const juce::Colour bone{0xffc8b890};
inline const juce::Colour boneDim{0xff8a7a5a};
inline const juce::Colour textDim{0xff584858};
inline const juce::Colour textGhost{0xff302830};
inline const juce::Colour textPhantom{0xff201820};
inline const juce::Colour dream{0xff585878};

inline constexpr int editorWidth = 1200;
inline constexpr int editorHeight = 760;

inline const juce::Rectangle<int> headBounds{0, 0, 1200, 48};
inline const juce::Rectangle<int> sequencerBounds{24, 70, 736, 250};
inline const juce::Rectangle<int> patternBounds{784, 70, 392, 250};
inline const juce::Rectangle<int> voiceBounds{24, 338, 736, 240};
inline const juce::Rectangle<int> spectrumBounds{784, 338, 392, 262};
inline const juce::Rectangle<int> rumbleBounds{24, 614, 736, 120};
inline const juce::Rectangle<int> outputBounds{784, 614, 392, 120};
inline const juce::Rectangle<int> topHazardBounds{0, 0, 1200, 10};
inline const juce::Rectangle<int> bottomHazardBounds{0, 750, 1200, 10};
inline const juce::Rectangle<int> stepsBounds{100, 88, 664, 72};
inline const juce::Rectangle<int> spectrumPlotBounds{798, 404, 364, 166};
inline const juce::Rectangle<int> spectrumGlassBounds{784, 338, 392, 262};
inline const juce::Rectangle<int> driftArcBounds{884, 259, 292, 36};
inline const juce::Rectangle<int> meterBounds{868, 672, 268, 12};
inline const juce::Rectangle<int> clockBounds{488, 12, 224, 26};

struct ParameterPlace {
    const char* id;
    bool knob;
    float x;
    float y;
    float w;
    float h;
    const char* legend;
    bool alwaysShows;
};

inline const std::array<ParameterPlace, 49>& parameterPlaces() {
    static const std::array<ParameterPlace, 49> places{{
        {"vco1_wave", false, 30.0f, 380.0f, 52.0f, 18.0f, "", true},
        {"vco1_level", true, 56.0f, 430.0f, 40.0f, 40.0f, "LEVEL", false},
        {"vco2_wave", false, 94.0f, 380.0f, 52.0f, 18.0f, "", true},
        {"vco2_tune", true, 120.0f, 518.0f, 40.0f, 40.0f, "TUNE", false},
        {"vco2_level", true, 120.0f, 430.0f, 40.0f, 40.0f, "LEVEL", false},
        {"vco_fm", true, 56.0f, 518.0f, 40.0f, 40.0f, "FM 1-2", false},
        {"noise_level", true, 184.0f, 430.0f, 40.0f, 40.0f, "LEVEL", false},
        {"sub_level", true, 248.0f, 430.0f, 40.0f, 40.0f, "LEVEL", false},
        {"sub_octave", false, 222.0f, 380.0f, 52.0f, 18.0f, "", true},
        {"pitch_env", true, 336.0f, 430.0f, 40.0f, 40.0f, "AMOUNT", false},
        {"pitch_decay", true, 336.0f, 518.0f, 40.0f, 40.0f, "DECAY", false},
        {"vcf_cutoff", true, 440.0f, 474.0f, 64.0f, 64.0f, "CUTOFF", false},
        {"vcf_res", true, 516.0f, 430.0f, 40.0f, 40.0f, "RES", false},
        {"vcf_drive", true, 516.0f, 518.0f, 40.0f, 40.0f, "DRIVE", false},
        {"vcf_env", true, 596.0f, 430.0f, 40.0f, 40.0f, "AMOUNT", false},
        {"vcf_decay", true, 596.0f, 518.0f, 40.0f, 40.0f, "DECAY", false},
        {"vca_attack", false, 660.0f, 418.0f, 92.0f, 24.0f, "ATTACK", true},
        {"vca_decay", true, 706.0f, 518.0f, 40.0f, 40.0f, "DECAY", false},
        {"seq_mode", false, 840.0f, 112.0f, 336.0f, 24.0f, "MODE", true},
        {"seq_rate", true, 931.0f, 200.0f, 40.0f, 40.0f, "RATE", true},
        {"seq_length", true, 1029.0f, 200.0f, 40.0f, 40.0f, "LENGTH", true},
        {"seq_drift", true, 833.0f, 276.0f, 40.0f, 40.0f, "DRIFT", true},
        {"seq_root", true, 833.0f, 200.0f, 40.0f, 40.0f, "ROOT", true},
        {"seq_bpm", true, 1127.0f, 200.0f, 40.0f, 40.0f, "TEMPO", true},
        {"step1_pitch", true, 145.0f, 200.0f, 40.0f, 40.0f, "", true},
        {"step2_pitch", true, 227.0f, 200.0f, 40.0f, 40.0f, "", true},
        {"step3_pitch", true, 309.0f, 200.0f, 40.0f, 40.0f, "", true},
        {"step4_pitch", true, 391.0f, 200.0f, 40.0f, 40.0f, "", true},
        {"step5_pitch", true, 473.0f, 200.0f, 40.0f, 40.0f, "", true},
        {"step6_pitch", true, 555.0f, 200.0f, 40.0f, 40.0f, "", true},
        {"step7_pitch", true, 637.0f, 200.0f, 40.0f, 40.0f, "", true},
        {"step8_pitch", true, 719.0f, 200.0f, 40.0f, 40.0f, "", true},
        {"step1_vel", true, 145.0f, 276.0f, 40.0f, 40.0f, "", true},
        {"step2_vel", true, 227.0f, 276.0f, 40.0f, 40.0f, "", true},
        {"step3_vel", true, 309.0f, 276.0f, 40.0f, 40.0f, "", true},
        {"step4_vel", true, 391.0f, 276.0f, 40.0f, 40.0f, "", true},
        {"step5_vel", true, 473.0f, 276.0f, 40.0f, 40.0f, "", true},
        {"step6_vel", true, 555.0f, 276.0f, 40.0f, 40.0f, "", true},
        {"step7_vel", true, 637.0f, 276.0f, 40.0f, 40.0f, "", true},
        {"step8_vel", true, 719.0f, 276.0f, 40.0f, 40.0f, "", true},
        {"rumble_level", true, 719.0f, 678.0f, 40.0f, 40.0f, "LEVEL", false},
        {"rumble_send", true, 309.0f, 678.0f, 40.0f, 40.0f, "SEND", false},
        {"rumble_kick", true, 145.0f, 678.0f, 40.0f, 40.0f, "KICK", false},
        {"rumble_rate", true, 227.0f, 678.0f, 40.0f, 40.0f, "RATE", false},
        {"rumble_decay", true, 391.0f, 678.0f, 40.0f, 40.0f, "DECAY", false},
        {"rumble_tone", true, 473.0f, 678.0f, 40.0f, 40.0f, "TONE", false},
        {"rumble_drive", true, 555.0f, 678.0f, 40.0f, 40.0f, "DRIVE", false},
        {"rumble_duck", true, 637.0f, 678.0f, 40.0f, 40.0f, "DUCK", false},
        {"master_gain", true, 820.0f, 678.0f, 44.0f, 44.0f, "MASTER", false},
    }};
    return places;
}

inline const juce::Typeface::Ptr& legendTypeface() {
    static const juce::Typeface::Ptr typeface = juce::Typeface::createSystemTypefaceFor(
        UndertowFonts::MichromaRegular_ttf, UndertowFonts::MichromaRegular_ttfSize);
    return typeface;
}

inline const juce::Typeface::Ptr& numberTypeface() {
    static const juce::Typeface::Ptr typeface = juce::Typeface::createSystemTypefaceFor(
        UndertowFonts::GeistMonoRegular_ttf, UndertowFonts::GeistMonoRegular_ttfSize);
    return typeface;
}

inline const juce::Typeface::Ptr& numberLightTypeface() {
    static const juce::Typeface::Ptr typeface = juce::Typeface::createSystemTypefaceFor(
        UndertowFonts::GeistMonoLight_ttf, UndertowFonts::GeistMonoLight_ttfSize);
    return typeface;
}

inline juce::Font makeFont(const juce::Typeface::Ptr& typeface, float px, float tracking) {
    auto font = juce::Font(juce::FontOptions(typeface).withPointHeight(px));
    const auto height = font.getHeight();
    return font.withExtraKerningFactor(height > 0.0f ? tracking / height : 0.0f);
}

inline juce::Font legendFont(float px, float tracking = 0.0f) {
    return makeFont(legendTypeface(), px, tracking);
}

inline juce::Font numberFont(float px, float tracking = 0.2f) {
    return makeFont(numberTypeface(), px, tracking);
}

inline juce::Font numberLightFont(float px, float tracking = 0.0f) {
    return makeFont(numberLightTypeface(), px, tracking);
}

inline juce::Rectangle<int> stepCellBounds(int step) {
    if (step < 0 || step >= 8)
        return {};
    return {108 + 82 * step, 96, 74, 56};
}

inline int stepAt(juce::Point<int> point) {
    for (int step = 0; step < 8; ++step) {
        if (stepCellBounds(step).contains(point))
            return step;
    }
    return -1;
}

inline juce::Rectangle<int> plotBounds() {
    return spectrumPlotBounds;
}

inline juce::Rectangle<int> textBox(float anchorX, float baseline, int width, int height,
                                    juce::Justification justification) {
    const auto y = juce::roundToInt(std::floor(baseline - static_cast<float>(height)));
    int x = juce::roundToInt(std::floor(anchorX));

    if (justification.testFlags(juce::Justification::horizontallyCentred))
        x -= width / 2;
    else if (justification.testFlags(juce::Justification::right))
        x -= width;

    return {x, y, width, height};
}

}  // namespace undertow::ui::theme
