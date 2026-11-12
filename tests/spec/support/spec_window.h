// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

// Helpers for the phase 6 window tests (plugin preset): a frozen window driven by fixed frames, a performance that
// plays the default pattern into frames as a host would, the design's places (docs/design/window.md section 3), and
// pixel statistics over snapshots.

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "ui/Frame.h"  // WindowFrame (docs/specs/phase-6-window.md, task 2)
#include "undertow/params.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "doctest.h"

// doctest prints a `const char*` as a pointer (it comes out as 1). The checks' messages name ids, legends and readouts
// held in C strings, so print those as text.
namespace doctest {
template <>
struct StringMaker<const char*> {
    static String convert(const char* in) { return in != nullptr ? String(in) : String("(null)"); }
};
}  // namespace doctest

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace spec {

constexpr double kWindowPi = 3.14159265358979323846;
constexpr double kStepSeconds = 60.0 / 130.0 / 4.0;  // a 1/16 step at 130 BPM

// ---------------------------------------------------------------------------------------------------------------
// The design's places (docs/design/window.md section 3): a knob's centre and dial, a switch's rectangle.

struct DesignPlace {
    const char* id;
    bool knob;
    float x, y, w, h;   // a knob: centre x, y and dial w (h = w); a switch: x, y, w, h
    const char* legend; // drawn under a knob; "" for the steps (row labels) and the switches without one
};

inline const std::array<DesignPlace, 49>& designPlaces() {
    static const std::array<DesignPlace, 49> places{{
        {"vco1_wave", false, 30, 380, 52, 18, ""},     {"vco1_level", true, 56, 430, 40, 40, "LEVEL"},
        {"vco2_wave", false, 94, 380, 52, 18, ""},     {"vco2_tune", true, 120, 518, 40, 40, "TUNE"},
        {"vco2_level", true, 120, 430, 40, 40, "LEVEL"}, {"vco_fm", true, 56, 518, 40, 40, "FM 1-2"},
        {"noise_level", true, 184, 430, 40, 40, "LEVEL"}, {"sub_level", true, 248, 430, 40, 40, "LEVEL"},
        {"sub_octave", false, 222, 380, 52, 18, ""},   {"pitch_env", true, 336, 430, 40, 40, "AMOUNT"},
        {"pitch_decay", true, 336, 518, 40, 40, "DECAY"}, {"vcf_cutoff", true, 440, 474, 64, 64, "CUTOFF"},
        {"vcf_res", true, 516, 430, 40, 40, "RES"},     {"vcf_drive", true, 516, 518, 40, 40, "DRIVE"},
        {"vcf_env", true, 596, 430, 40, 40, "AMOUNT"},  {"vcf_decay", true, 596, 518, 40, 40, "DECAY"},
        {"vca_attack", false, 660, 418, 92, 24, "ATTACK"}, {"vca_decay", true, 706, 518, 40, 40, "DECAY"},
        {"seq_mode", false, 840, 112, 336, 24, "MODE"}, {"seq_rate", true, 931, 200, 40, 40, "RATE"},
        {"seq_length", true, 1029, 200, 40, 40, "LENGTH"}, {"seq_drift", true, 833, 276, 40, 40, "DRIFT"},
        {"seq_root", true, 833, 200, 40, 40, "ROOT"},   {"seq_bpm", true, 1127, 200, 40, 40, "TEMPO"},
        {"step1_pitch", true, 145, 200, 40, 40, ""},    {"step2_pitch", true, 227, 200, 40, 40, ""},
        {"step3_pitch", true, 309, 200, 40, 40, ""},    {"step4_pitch", true, 391, 200, 40, 40, ""},
        {"step5_pitch", true, 473, 200, 40, 40, ""},    {"step6_pitch", true, 555, 200, 40, 40, ""},
        {"step7_pitch", true, 637, 200, 40, 40, ""},    {"step8_pitch", true, 719, 200, 40, 40, ""},
        {"step1_vel", true, 145, 276, 40, 40, ""},      {"step2_vel", true, 227, 276, 40, 40, ""},
        {"step3_vel", true, 309, 276, 40, 40, ""},      {"step4_vel", true, 391, 276, 40, 40, ""},
        {"step5_vel", true, 473, 276, 40, 40, ""},      {"step6_vel", true, 555, 276, 40, 40, ""},
        {"step7_vel", true, 637, 276, 40, 40, ""},      {"step8_vel", true, 719, 276, 40, 40, ""},
        {"rumble_level", true, 719, 678, 40, 40, "LEVEL"}, {"rumble_send", true, 309, 678, 40, 40, "SEND"},
        {"rumble_kick", true, 145, 678, 40, 40, "KICK"}, {"rumble_rate", true, 227, 678, 40, 40, "RATE"},
        {"rumble_decay", true, 391, 678, 40, 40, "DECAY"}, {"rumble_tone", true, 473, 678, 40, 40, "TONE"},
        {"rumble_drive", true, 555, 678, 40, 40, "DRIVE"}, {"rumble_duck", true, 637, 678, 40, 40, "DUCK"},
        {"master_gain", true, 820, 678, 44, 44, "MASTER"},
    }};
    return places;
}

inline const DesignPlace& designPlace(const char* id) {
    for (const auto& p : designPlaces())
        if (std::strcmp(p.id, id) == 0)
            return p;
    FAIL("no design place for " << id);
    return designPlaces()[0];
}

// The rest of the design's geometry (sections 2 and 4).
inline juce::Rectangle<int> designCell(int step) { return {108 + 82 * step, 96, 74, 56}; }
inline juce::Rectangle<int> designTrace(int step) { return {112 + 82 * step, 114, 66, 34}; }
inline const juce::Rectangle<int> kSpectrumGlass{784, 338, 392, 262};
inline const juce::Rectangle<int> kPlot{798, 404, 364, 166};
inline const juce::Rectangle<int> kArc{884, 259, 292, 36};
inline const juce::Rectangle<int> kMeter{868, 672, 268, 12};
inline const juce::Rectangle<int> kClock{488, 12, 224, 26};
inline const juce::Rectangle<int> kBoneBox{940, 340, 230, 72};
inline const juce::Rectangle<int> kTopBand{0, 0, 1200, 10};
inline const juce::Rectangle<int> kBottomBand{0, 750, 1200, 10};
inline float plotX(double hz) { return 798.0f + 364.0f * static_cast<float>(std::log(hz / 20.0) / std::log(100.0)); }
inline float plotY(double db) { return 404.0f + 166.0f * static_cast<float>((-18.0 - std::clamp(db, -96.0, -18.0)) / 78.0); }
inline float meterX(double db) { return 870.0f + 264.0f * static_cast<float>((std::clamp(db, -48.0, 0.0) + 48.0) / 48.0); }

// ---------------------------------------------------------------------------------------------------------------
// Frames

// A rolling sub line under the default pattern: the sub at 49 Hz and VCO 1 at 98 Hz, each hit decaying, at the
// pattern's velocities, a hit every 1/16 at 130 BPM. `seconds` from the line's start.
inline float rollingLine(double seconds) {
    static const float velocity[8] = {0.0f, 0.22f, 1.0f, 0.55f, 0.0f, 0.25f, 0.9f, 0.6f};
    if (seconds < 0.0)
        return 0.0f;
    const long long hit = static_cast<long long>(seconds / kStepSeconds);
    double level = 0.0;
    for (long long k = std::max(0LL, hit - 4); k <= hit; ++k) {
        const double t = seconds - static_cast<double>(k) * kStepSeconds;
        const double attack = t < 0.004 ? 0.5 - 0.5 * std::cos(kWindowPi * t / 0.004) : 1.0;  // half a cosine, as the VCA
        level += velocity[k % 8] * attack * std::exp(-t / 0.085);
    }
    const double w = 2.0 * kWindowPi * 49.0 * seconds;
    return static_cast<float>(0.26 * std::min(level, 1.2) * (std::sin(w) + 0.45 * std::sin(2.0 * w + 0.4) + 0.12 * std::sin(3.0 * w)));
}

// What the meters and the scope say `seconds` into the line, playing step (seconds / step) mod 8 on G2, with the
// pattern's count offset by `countOffset` (40: pass 5 of the arc, where the default drift moves two steps).
inline WindowFrame playingAt(double seconds, long long countOffset = 40) {
    WindowFrame f;
    const long long count = static_cast<long long>(seconds / kStepSeconds);
    f.step = static_cast<int>(count % 8);
    f.count = count + countOffset;
    f.root = 43;
    f.note = 43;
    f.noteHz = 98.0f;
    f.bpm = 130.0;
    f.ppq = seconds * 130.0 / 60.0;
    f.hostClock = true;
    f.sampleRate = 48000.0;
    f.rumble = 0.03f;
    double sum = 0.0;
    float peak = 0.0f;
    for (int i = 0; i < undertow::kScopeSamples; ++i) {
        const float v = rollingLine(seconds - static_cast<double>(undertow::kScopeSamples - 1 - i) / 48000.0);
        f.scope[static_cast<std::size_t>(i)] = v;
        if (i >= undertow::kScopeSamples - 2400)
            sum += static_cast<double>(v) * v;
        if (i >= undertow::kScopeSamples - 512)
            peak = std::max(peak, std::abs(v));
    }
    f.rms = static_cast<float>(std::sqrt(sum / 2400.0));
    f.peak = peak;
    return f;
}

// A new instance in the Standalone app: no note, no host, the window's own clock at 130 BPM.
inline WindowFrame idleFrame() {
    WindowFrame f;
    f.bpm = 130.0;
    return f;
}

// A pure sine in the scope, as the output, with the meters to match.
inline WindowFrame sineFrame(double hz, double db) {
    WindowFrame f = idleFrame();
    const double a = std::pow(10.0, db / 20.0);
    for (int i = 0; i < undertow::kScopeSamples; ++i)
        f.scope[static_cast<std::size_t>(i)] = static_cast<float>(a * std::sin(2.0 * kWindowPi * hz * i / 48000.0));
    f.rms = static_cast<float>(a / std::sqrt(2.0));
    f.peak = static_cast<float>(a);
    f.note = 43;
    f.noteHz = 98.0f;
    return f;
}

// ---------------------------------------------------------------------------------------------------------------
// The window

struct Fixture {
    juce::ScopedJuceInitialiser_GUI gui;
    std::unique_ptr<juce::AudioProcessor> processor{createPluginFilter()};
    std::unique_ptr<UndertowEditor> editor;

    Fixture() {
        editor.reset(dynamic_cast<UndertowEditor*>(processor->createEditor()));
        REQUIRE_MESSAGE(editor != nullptr, "createEditor() must return an UndertowEditor");
        editor->freeze();
        editor->setVisible(true);  // so hit-testing (getComponentAt) finds its children
    }
    ~Fixture() { editor.reset(); }

    UndertowProcessor& undertow() { return *dynamic_cast<UndertowProcessor*>(processor.get()); }

    // Set a parameter in real units, as a host would.
    void set(const char* id, float value) {
        auto* p = undertow().parameters().getParameter(id);
        REQUIRE_MESSAGE(p != nullptr, "no parameter " << id);
        p->setValueNotifyingHost(p->convertTo0to1(value));
    }
    float get(const char* id) {
        auto* p = undertow().parameters().getParameter(id);
        REQUIRE(p != nullptr);
        return p->convertFrom0to1(p->getValue());
    }

    // Hold one frame for `frames` ticks of 1/60 s.
    void hold(const WindowFrame& frame, int frames = 30) {
        for (int i = 0; i < frames; ++i)
            editor->advance(frame, 1.0 / 60.0);
    }

    // Play the rolling line from 0 to `seconds` at 60 ticks a second.
    void perform(double seconds, long long countOffset = 40) {
        for (double t = 1.0 / 60.0; t <= seconds + 1e-9; t += 1.0 / 60.0)
            editor->advance(playingAt(t, countOffset), 1.0 / 60.0);
    }

    juce::Image shot(float scale = 1.0f) { return editor->createComponentSnapshot(editor->getLocalBounds(), true, scale); }

    juce::Rectangle<int> boundsOf(const char* id) {
        auto* c = editor->controlFor(id);
        REQUIRE_MESSAGE(c != nullptr, "controlFor(\"" << id << "\") found no control");
        return editor->getLocalArea(c, c->getLocalBounds());
    }
};

// The moment the review image `window-playing.png` shows: 45 ms after step 3's hit, on the line's second pass.
constexpr double kPlayingMoment = 10 * kStepSeconds + 0.045;

// ---------------------------------------------------------------------------------------------------------------
// Pixels

// Luma on the sRGB values (0 .. 1). Near-black is under 0.06.
inline double luma(juce::Colour c) { return (0.2126 * c.getRed() + 0.7152 * c.getGreen() + 0.0722 * c.getBlue()) / 255.0; }

// The rose hue band, 300 to 15 degrees, with half a degree of slack under 300 for colours with red equal to blue.
inline bool roseHue(juce::Colour c) {
    const double hue = c.getHue() * 360.0;
    return hue >= 299.5 || hue < 15.0;
}

// A pixel with enough colour to count: hairlines and the darkest anti-aliasing are not colour statements.
inline bool coloured(juce::Colour c) { return c.getSaturation() >= 0.15f && c.getBrightness() >= 0.15f; }

inline bool boneLike(juce::Colour c) {
    auto near = [&](int r, int g, int b, int d) {
        const int dr = c.getRed() - r, dg = c.getGreen() - g, db = c.getBlue() - b;
        return dr * dr + dg * dg + db * db <= d * d;
    };
    return c.getBrightness() >= 0.3f && (near(200, 184, 144, 36) || near(138, 122, 90, 24));
}

inline bool roseBright(juce::Colour c) { return roseHue(c) && c.getSaturation() >= 0.15f && c.getBrightness() >= 0.7f; }

struct Region {
    int pixels = 0, voidPixels = 0, nearWhite = 0, colouredPixels = 0, rose = 0, bone = 0, bright = 0;
    double lumaSum = 0;
    juce::Rectangle<int> boneBox;  // in the image's pixels
};

// Statistics over `area` (window coordinates) of a snapshot taken at `scale`.
inline Region measure(const juce::Image& image, juce::Rectangle<int> area, float scale = 1.0f) {
    Region r;
    juce::Image::BitmapData data(image, juce::Image::BitmapData::readOnly);
    const auto scaled = (area.toFloat() * scale).toNearestInt().getIntersection(image.getBounds());
    int minX = 1 << 30, minY = 1 << 30, maxX = -1, maxY = -1;
    for (int y = scaled.getY(); y < scaled.getBottom(); ++y)
        for (int x = scaled.getX(); x < scaled.getRight(); ++x) {
            const auto c = data.getPixelColour(x, y);
            ++r.pixels;
            const double l = luma(c);
            r.lumaSum += l;
            r.voidPixels += l < 0.06 ? 1 : 0;
            r.nearWhite += (c.getRed() > 215 && c.getGreen() > 215 && c.getBlue() > 215) ? 1 : 0;
            if (coloured(c)) {
                ++r.colouredPixels;
                r.rose += roseHue(c) ? 1 : 0;
            }
            r.bright += roseBright(c) ? 1 : 0;
            if (boneLike(c)) {
                ++r.bone;
                minX = std::min(minX, x);
                minY = std::min(minY, y);
                maxX = std::max(maxX, x);
                maxY = std::max(maxY, y);
            }
        }
    if (maxX >= 0)
        r.boneBox = juce::Rectangle<int>(minX, minY, maxX - minX + 1, maxY - minY + 1);
    return r;
}

inline double meanLuma(const Region& r) { return r.pixels > 0 ? r.lumaSum / r.pixels : 0.0; }

// Pixels of drawn ink in `box` (window coordinates, a 1x snapshot): luma at least 0.2.
inline int inkIn(const juce::Image& image, juce::Rectangle<int> box) {
    juce::Image::BitmapData data(image, juce::Image::BitmapData::readOnly);
    int ink = 0;
    const auto b = box.getIntersection(image.getBounds());
    for (int y = b.getY(); y < b.getBottom(); ++y)
        for (int x = b.getX(); x < b.getRight(); ++x)
            ink += luma(data.getPixelColour(x, y)) >= 0.2 ? 1 : 0;
    return ink;
}

// The drawn word `text` whose box lies inside `area`, or nullptr.
inline const DrawnText* findText(const std::vector<DrawnText>& drawn, const juce::String& text,
                                 juce::Rectangle<int> area = {0, 0, 1200, 760}) {
    for (const auto& t : drawn)
        if (t.text.trim() == text && area.contains(t.bounds.getCentre()))
            return &t;
    return nullptr;
}

inline std::string str(juce::Rectangle<int> r) { return r.toString().toStdString(); }

}  // namespace spec
