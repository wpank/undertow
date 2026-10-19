#pragma once

// Helpers for the phase 4 editor tests (plugin preset only): fixed UiFrames, an editor fixture driven by them, and
// pixel statistics over snapshots (luminance, the rose hue band, bone, rose_bright).

#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <cmath>
#include <memory>

#include "doctest.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace spec {

constexpr double kUiPi = 3.14159265358979323846;



inline UiFrame idleFrame() {
    UiFrame f;
    f.bpm = 128.0;
    return f;
}

inline UiFrame playingFrame() {
    UiFrame f;
    f.energy = 0.6f;
    f.peak = 0.5f;
    f.rms = 0.25f;
    f.note = 33;
    f.noteHz = 55.0f;
    f.step = 3;
    f.length = 16;
    f.resonance = 0.35f;
    f.rumble = 0.2f;
    f.ppq = 13.25;
    f.bpm = 128.0;
    f.hostClock = true;
    for (std::size_t i = 0; i < f.scope.size(); ++i)
        f.scope[i] = static_cast<float>(0.5 * std::sin(2.0 * kUiPi * 55.0 * static_cast<double>(i) / 48000.0));
    return f;
}

inline UiFrame clippingFrame() {
    UiFrame f = playingFrame();
    f.energy = 1.0f;
    f.peak = 0.966f;
    f.clipping = true;
    return f;
}

struct Fixture {
    juce::ScopedJuceInitialiser_GUI gui;
    std::unique_ptr<juce::AudioProcessor> processor{createPluginFilter()};
    std::unique_ptr<UndertowEditor> editor;

    Fixture() {
        editor.reset(dynamic_cast<UndertowEditor*>(processor->createEditor()));
        REQUIRE_MESSAGE(editor != nullptr, "createEditor() must return an UndertowEditor");
        editor->freeze();
    }
    ~Fixture() { editor.reset(); }

    juce::Image shot(const UiFrame& frame, int frames = 30, float scale = 1.0f) {
        for (int i = 0; i < frames; ++i)
            editor->advance(frame, 1.0 / 60.0);
        return editor->createComponentSnapshot(editor->getLocalBounds(), true, scale);
    }
};

inline double luminance(juce::Colour c) { return (0.2126 * c.getRed() + 0.7152 * c.getGreen() + 0.0722 * c.getBlue()) / 255.0; }

inline bool roseHue(juce::Colour c) {
    const double hue = c.getHue() * 360.0;
    return hue >= 300.0 || hue < 15.0;
}

inline bool coloured(juce::Colour c) { return c.getSaturation() >= 0.15f && c.getBrightness() >= 0.12f; }

inline bool boneLike(juce::Colour c) {
    auto near = [&](int r, int g, int b, int d) {
        const int dr = c.getRed() - r, dg = c.getGreen() - g, db = c.getBlue() - b;
        return dr * dr + dg * dg + db * db <= d * d;
    };
    return near(200, 184, 144, 36) || near(138, 122, 90, 24);
}

inline bool roseBright(juce::Colour c) { return roseHue(c) && c.getSaturation() >= 0.15f && c.getBrightness() >= 0.7f; }

struct Region {
    int pixels = 0, voidPixels = 0, nearWhite = 0, colouredPixels = 0, rose = 0, bone = 0, bright = 0;
    double luminanceSum = 0;
    juce::Rectangle<int> boneBox;
};

// Statistics over `area` of a snapshot taken at `scale`; `area` is in editor coordinates.
inline Region measure(const juce::Image& image, juce::Rectangle<int> area, float scale = 1.0f) {
    Region r;
    juce::Image::BitmapData data(image, juce::Image::BitmapData::readOnly);
    const auto scaled = (area.toFloat() * scale).toNearestInt().getIntersection(image.getBounds());
    int minX = 1 << 30, minY = 1 << 30, maxX = -1, maxY = -1;
    for (int y = scaled.getY(); y < scaled.getBottom(); ++y)
        for (int x = scaled.getX(); x < scaled.getRight(); ++x) {
            const auto c = data.getPixelColour(x, y);
            ++r.pixels;
            const double l = luminance(c);
            r.luminanceSum += l;
            r.voidPixels += l < 0.06 ? 1 : 0;
            r.nearWhite += (c.getRed() > 215 && c.getGreen() > 215 && c.getBlue() > 215) ? 1 : 0;
            if (coloured(c)) {
                ++r.colouredPixels;
                r.rose += roseHue(c) ? 1 : 0;
            }
            r.bright += roseBright(c) ? 1 : 0;
            if (boneLike(c) && c.getBrightness() >= 0.3f) {
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

inline double mean(const Region& r) { return r.pixels > 0 ? r.luminanceSum / r.pixels : 0.0; }


} // namespace spec
