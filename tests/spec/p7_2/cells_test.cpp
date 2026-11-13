// SPDX-License-Identifier: AGPL-3.0-only
// Phase 7.2 spec tests: the step cells drawn as designed (docs/specs/phase-7.2-window-as-designed.md, check P7.2.05;
// docs/design/window.md section 4.1).
// spec-preset: plugin

#include "doctest.h"

#include "spec_window.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace spec;

namespace {

WindowFrame stepFrame(int step, long long count) {
    WindowFrame f = sineFrame(98.0, -10.0);
    f.step = step;
    f.count = count;
    f.root = 43;
    f.bpm = 130.0;
    f.hostClock = true;
    return f;
}

// The mean colour of a window rectangle in a 2x snapshot.
juce::Colour meanColour(const juce::Image& image, juce::Rectangle<float> area) {
    double r = 0, g = 0, b = 0;
    int n = 0;
    for (float y = area.getY(); y < area.getBottom(); y += 0.5f)
        for (float x = area.getX(); x < area.getRight(); x += 0.5f) {
            const auto c = image.getPixelAt(juce::roundToInt(x * 2.0f), juce::roundToInt(y * 2.0f));
            r += c.getRed(), g += c.getGreen(), b += c.getBlue();
            ++n;
        }
    return juce::Colour(static_cast<juce::uint8>(r / n), static_cast<juce::uint8>(g / n), static_cast<juce::uint8>(b / n));
}

// The mean luma of the ring from `from` to `to` px outside a cell, in a 2x snapshot.
double ringLuma(const juce::Image& image, juce::Rectangle<int> cell, float from, float to) {
    const auto outer = cell.toFloat().expanded(to), inner = cell.toFloat().expanded(from);
    double sum = 0.0;
    int n = 0;
    for (float y = outer.getY(); y < outer.getBottom(); y += 0.5f)
        for (float x = outer.getX(); x < outer.getRight(); x += 0.5f)
            if (!inner.contains(x, y)) {
                sum += luma(image.getPixelAt(juce::roundToInt(x * 2.0f), juce::roundToInt(y * 2.0f)));
                ++n;
            }
    return sum / n;
}

}  // namespace

TEST_CASE("P7.2.05 the cells as designed: a rest's 1 px rule2 dashes, the lit cell's ember fill and soft glow, a "
          "translucent trace band between 1 px edges, and a sweep head only in the playing cell") {
    Fixture f;
    f.set("seq_drift", 0.0f);
    f.hold(stepFrame(1, 41), 8);  // step 2 plays through: its trace crosses its whole cell
    f.hold(stepFrame(2, 42), 1);  // step 3 plays for one tick: its trace stops part of the way across
    f.hold(stepFrame(3, 43), 3);  // step 4 plays, lit
    const auto image = f.shot(2.0f);

    for (const int s : {0, 4}) {  // steps 1 and 5 are rests
        INFO("step " << s + 1 << ", a rest");
        const auto cell = designCell(s).toFloat();
        double brightest = 0.0;
        int dashes = 0;
        for (float y = cell.getY(); y < cell.getBottom(); y += 0.5f)
            for (float x = cell.getX(); x < cell.getRight(); x += 0.5f) {
                if (cell.reduced(2.0f).contains(x, y))
                    continue;
                const double l = luma(image.getPixelAt(juce::roundToInt(x * 2.0f), juce::roundToInt(y * 2.0f)));
                brightest = std::max(brightest, l);
                dashes += l >= 0.08 ? 1 : 0;
            }
        CHECK_MESSAGE(dashes >= 40, "a rest is outlined in dashes (3 on, 3 off)");
        CHECK_MESSAGE(brightest <= 0.2, "a rest's dashes are 1 px of rule2 (#30222E), quieter than any cell that plays; the "
                                        "brightest reads luma " << brightest << ". The dashed path is filled, never stroked "
                                        "again");
    }

    const auto lit = designCell(3), unlit = designCell(6);  // step 4 plays; step 7 hasn't played
    const auto strip = [](juce::Rectangle<int> cell) {
        return juce::Rectangle<float>(static_cast<float>(cell.getX() + 20), static_cast<float>(cell.getY() + 3), 36.0f, 12.0f);
    };
    const auto fill = meanColour(image, strip(lit));
    CHECK_MESSAGE(luma(fill) >= 0.06, "the lit cell has an ember fill fading downwards (rose_ember, 55% at the top); its "
                                      "upper strip reads luma " << luma(fill));
    CHECK_MESSAGE(roseHue(fill), "the lit cell's fill is rose_ember, in the rose hue band");
    CHECK_MESSAGE(luma(meanColour(image, strip(unlit))) <= 0.045, "a cell that isn't lit is glass, with no fill");
    const double glow = ringLuma(image, lit, 2.0f, 4.0f), ground = ringLuma(image, unlit, 2.0f, 4.0f);
    CHECK_MESSAGE(glow >= ground + 0.015, "the lit cell has a soft outer glow (rose_bright, 16%, 5 px wide around it); 2 to 4 "
                                          "px outside it the luma is " << glow << " against " << ground << " around a cell "
                                          "that isn't lit");

    {  // step 2's whole trace: a translucent band between antialiased 1 px edges, never solid columns
        const auto whole = designTrace(1);
        double brightest = 0.0;
        for (int py = whole.getY() * 2; py < whole.getBottom() * 2; ++py)
            for (int px = whole.getX() * 2; px < whole.getRight() * 2; ++px)
                brightest = std::max(brightest, luma(image.getPixelAt(px, py)));
        int strong = 0, between = 0;
        for (int py = whole.getY() * 2; py < whole.getBottom() * 2; ++py)
            for (int px = whole.getX() * 2; px < whole.getRight() * 2; ++px) {
                const double l = luma(image.getPixelAt(px, py));
                strong += l > 0.7 * brightest ? 1 : 0;
                between += l >= 0.2 * brightest && l <= 0.7 * brightest ? 1 : 0;
            }
        CHECK_MESSAGE(strong >= 40, "step 2's trace crosses its cell");
        CHECK_MESSAGE(between >= strong / 2, "the trace is a band filled at 20 to 30% between antialiased 1 px edges, so "
                      "most of its light is half-tones; " << between << " pixels are half-tones against " << strong
                      << " at full strength: solid columns?");
    }

    const auto trace = designTrace(2);  // step 3's trace, stopped part way: no sweep head now that step 4 plays
    for (int x = trace.getX(); x < trace.getRight(); ++x) {
        int bright = 0;
        for (float y = static_cast<float>(trace.getY()); y < static_cast<float>(trace.getBottom()); y += 0.5f)
            bright += luma(image.getPixelAt(x * 2, juce::roundToInt(y * 2.0f))) >= 0.3 ? 1 : 0;
        CHECK_MESSAGE(bright < trace.getHeight() * 2 * 8 / 10, "only the playing cell has a sweep head, but step 3's column at "
                      "x " << x << " is bright over " << bright / 2 << " of its " << trace.getHeight() << " px");
    }
}
