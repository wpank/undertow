// SPDX-License-Identifier: AGPL-3.0-only
// Phase 7.3 spec tests: the playing step row's cells as designed (docs/specs/phase-7.3-step-row-as-designed.md, checks
// P7.3.01 to P7.3.03; docs/design/window.md section 4.1).
// spec-preset: plugin

#include "doctest.h"

#include "spec_window.h"

#include <algorithm>
#include <cmath>

using namespace spec;

namespace {

WindowFrame stepFrame(int step, long long count, double hz = 98.0) {
    WindowFrame f = sineFrame(hz, -10.0);
    f.step = step;
    f.count = count;
    f.root = 43;
    f.bpm = 130.0;
    f.hostClock = true;
    return f;
}

// The luma of a 2x snapshot's pixel.
double at(const juce::Image& image, int px, int py) { return luma(image.getPixelAt(px, py)); }

// The mean luma of a window rectangle in a 2x snapshot.
double meanIn(const juce::Image& image, juce::Rectangle<float> area) {
    double sum = 0.0;
    int n = 0;
    for (float y = area.getY(); y < area.getBottom(); y += 0.5f)
        for (float x = area.getX(); x < area.getRight(); x += 0.5f) {
            sum += at(image, juce::roundToInt(x * 2.0f), juce::roundToInt(y * 2.0f));
            ++n;
        }
    return sum / n;
}

// A cell's upper strip (right of its number, above its trace) and a strip low in it (under the right of its trace,
// which a trace three ticks old has not reached).
juce::Rectangle<float> upperStrip(int step) {
    const auto cell = designCell(step);
    return {static_cast<float>(cell.getX() + 20), static_cast<float>(cell.getY() + 3), 36.0f, 12.0f};
}
juce::Rectangle<float> lowerStrip(int step) {
    const auto cell = designCell(step);
    return {static_cast<float>(cell.getX() + 40), static_cast<float>(cell.getY() + 42), 28.0f, 10.0f};
}

// The 2x rows of a trace's middle line, y 130.5 to 131.5.
constexpr int kMiddleTop = 261, kMiddleBottom = 262;

double middleLuma(const juce::Image& image, int x) {  // the brightest of a 1x column's four middle-line pixels
    double hi = 0.0;
    for (int py = kMiddleTop; py <= kMiddleBottom; ++py)
        for (int px = 2 * x; px <= 2 * x + 1; ++px)
            hi = std::max(hi, at(image, px, py));
    return hi;
}

}  // namespace

TEST_CASE("P7.3.01 the lit cell's ember fill at the design's strength, fading downwards") {
    Fixture f;
    f.set("seq_drift", 0.0f);
    f.hold(stepFrame(1, 41), 8);
    f.hold(stepFrame(2, 42), 1);
    f.hold(stepFrame(3, 43), 3);  // step 4 plays, lit, three ticks in
    const auto image = f.shot(2.0f);

    const int lit = 3, unlit = 6;  // step 4 plays; step 7 hasn't played: its face is the cell without the light
    const double top = meanIn(image, upperStrip(lit)), low = meanIn(image, lowerStrip(lit));
    const double topRise = top - meanIn(image, upperStrip(unlit));
    const double lowRise = low - meanIn(image, lowerStrip(unlit));
    CHECK_MESSAGE(top <= 0.13, "the lit cell's fill is rose_ember at 55% of the light at its top, and nothing else: "
                               "its upper strip reads luma " << top << " (the design's reads about 0.10)");
    CHECK_MESSAGE(topRise >= 0.03, "the light fills the lit cell: its upper strip is only " << topRise << " brighter "
                                   "than a cell that isn't lit");
    CHECK_MESSAGE(lowRise <= 0.6 * topRise, "the ember fill fades downwards, 55% of the light at the top to 12% at the "
                  "bottom: low in the cell the light adds " << lowRise << " luma against " << topRise << " at its "
                  "top (at most 0.6 times as much)");
}

TEST_CASE("P7.3.02 a trace is a band standing on the middle line, filled from it to the wave, its edges the "
          "brightest") {
    Fixture f;
    f.set("seq_drift", 0.0f);
    f.hold(stepFrame(1, 41, 24.5), 8);   // step 2 plays through a 24.5 Hz sine: its trace crosses its whole cell
    f.hold(stepFrame(2, 42, 24.5), 16);  // step 3 plays on, and step 2's light has gone (250 ms)
    const auto image = f.shot(2.0f);
    const auto trace = designTrace(1);

    int onLine = 0;
    for (int x = trace.getX(); x < trace.getRight(); ++x)
        onLine += middleLuma(image, x) >= 0.08 ? 1 : 0;
    CHECK_MESSAGE(onLine >= trace.getWidth() * 9 / 10, "each column's band reaches from the middle line to the wave "
                  "(its min and max are taken with 0), so the band stands on the line; only " << onLine << " of step "
                  "2's " << trace.getWidth() << " columns touch it: a thin line?");

    // In each 2x pixel column, walk from the middle line out to the band's far end, on the side the wave is: its outer
    // third (the 1 px edge) must outshine its middle third (the 20% fill).
    int tall = 0, edged = 0;
    for (int px = 2 * trace.getX() + 4; px < 2 * trace.getRight() - 4; ++px) {
        int up = 0, down = 0;
        while (kMiddleTop - up > 2 * trace.getY() && at(image, px, kMiddleTop - up) >= 0.07)
            ++up;
        while (kMiddleBottom + down < 2 * trace.getBottom() && at(image, px, kMiddleBottom + down) >= 0.07)
            ++down;
        const int span = std::max(up, down), step = up >= down ? -1 : 1;
        const int from = up >= down ? kMiddleTop : kMiddleBottom;
        if (span < 12)  // a band under 6 px, near the wave's zero crossings, is all edge
            continue;
        ++tall;
        double outer = 0.0, middle = 1.0;
        for (int k = span - span / 3; k < span; ++k)
            outer = std::max(outer, at(image, px, from + step * k));
        for (int k = span / 3; k < span - span / 3; ++k)
            middle = std::min(middle, at(image, px, from + step * k));
        edged += outer >= 1.5 * middle ? 1 : 0;
    }
    CHECK_MESSAGE(tall >= 60, "a 24.5 Hz sine at -10 dBFS fills tall bands across step 2's trace; found " << tall
                  << " pixel columns with a band of 6 px or more");
    CHECK_MESSAGE(edged >= tall * 8 / 10, "the band is filled at 20% of the trace's alpha between 1 px edges at its "
                  "alpha (and 70%), so its far edge is the brightest part of it; in " << tall - edged << " of " << tall
                  << " pixel columns the band's middle is as bright as its edge");
}

TEST_CASE("P7.3.03 a rest's dashes are 1 px; a sounding cell that hasn't played shows a flat line, a rest none") {
    Fixture f;
    f.set("seq_drift", 0.0f);
    f.hold(stepFrame(1, 41), 8);
    f.hold(stepFrame(3, 43), 3);  // steps 2 and 4 have played; steps 6, 7 and 8 haven't; steps 1 and 5 are rests
    const auto image = f.shot(2.0f);

    for (const int s : {0, 4}) {
        INFO("step " << s + 1 << ", a rest");
        const auto cell = designCell(s).toFloat();
        int dashes = 0;
        for (float y = cell.getY(); y < cell.getBottom(); y += 0.5f)
            for (float x = cell.getX(); x < cell.getRight(); x += 0.5f)
                if (!cell.reduced(2.0f).contains(x, y))
                    dashes += at(image, juce::roundToInt(x * 2.0f), juce::roundToInt(y * 2.0f)) >= 0.08 ? 1 : 0;
        CHECK_MESSAGE(dashes <= 700, "a rest's dashes are 1 px of rule2: they light about 520 of the 2016 pixels of "
                      "the 2 px ring inside its edge, but here " << dashes << ". The dashed path is the dashes' "
                      "outline: fill it, never stroke it again");
        const auto trace = designTrace(s);
        double line = 0.0;
        for (int x = trace.getX() + 1; x < trace.getX() + 18; ++x)  // the middle line beside the word REST
            line = std::max(line, middleLuma(image, x));
        for (int x = trace.getRight() - 16; x < trace.getRight() - 1; ++x)
            line = std::max(line, middleLuma(image, x));
        CHECK_MESSAGE(line <= 0.06, "a rest has no trace and no line: its middle line reads luma " << line);
    }

    for (const int s : {5, 6, 7}) {
        INFO("step " << s + 1 << ", sounding, not played yet");
        const auto trace = designTrace(s);
        int lit = 0;
        for (int x = trace.getX(); x < trace.getRight(); ++x)
            lit += middleLuma(image, x) >= 0.08 ? 1 : 0;
        CHECK_MESSAGE(lit >= trace.getWidth() * 9 / 10, "a sounding cell that has not played yet shows a 1 px "
                      "rose_deep line across its middle; it lights " << lit << " of its " << trace.getWidth()
                      << " columns");
        const double off = std::max(meanIn(image, {static_cast<float>(trace.getX()), 127.0f, 66.0f, 1.0f}),
                                    meanIn(image, {static_cast<float>(trace.getX()), 135.0f, 66.0f, 1.0f}));
        CHECK_MESSAGE(off <= 0.05, "the line is 1 px: 4 px above and below it the cell reads luma " << off);
    }
}
