// SPDX-License-Identifier: AGPL-3.0-only
// Phase 7.2 spec tests: the console drawn as designed: the head's and the console's words on their baselines, and a
// quiet spectrum glass (docs/specs/phase-7.2-window-as-designed.md, checks P7.2.06 and P7.2.07; docs/design/window.md
// sections 4.2 to 4.7).
// spec-preset: plugin

#include "doctest.h"

#include "spec_window.h"
#include "ui/Theme.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace spec;

namespace {

namespace theme = undertow::ui::theme;

// The pixel row a line of capitals and digits stands on: the lowest row of its box with at least 30% of the ink of
// its inkiest row (as P6.18 reads the faceplate).
int standsOn(const juce::Image& image, juce::Rectangle<int> box) {
    std::vector<double> rows;
    for (int y = box.getY(); y < box.getBottom(); ++y) {
        double ink = 0.0;
        for (int x = box.getX(); x < box.getRight(); ++x)
            ink += std::max(0.0, luma(image.getPixelAt(x, y)) - 0.03);
        rows.push_back(ink);
    }
    const double most = rows.empty() ? 0.0 : *std::max_element(rows.begin(), rows.end());
    for (int i = static_cast<int>(rows.size()) - 1; i >= 0; --i)
        if (most > 0.0 && rows[static_cast<std::size_t>(i)] >= 0.3 * most)
            return box.getY() + i;
    return -1;
}

float width(const juce::Font& font, const juce::String& text) { return juce::GlyphArrangement::getStringWidth(font, text); }

enum class Align { left, centre, right };

struct Word {
    juce::String text;
    juce::Rectangle<int> area;  // where to look for it
    float x;                    // its left edge, centre or right edge
    Align align;
    float baseline;             // 0: not read here (a label drawn over the curve)
};

// The box of a word's ink (luma 0.2 and up) inside its recorded box: what a reader sees overlap.
juce::Rectangle<int> inkBox(const juce::Image& image, juce::Rectangle<int> box) {
    int x0 = 1 << 30, y0 = 1 << 30, x1 = -1, y1 = -1;
    const auto b = box.getIntersection(image.getBounds());
    for (int y = b.getY(); y < b.getBottom(); ++y)
        for (int x = b.getX(); x < b.getRight(); ++x)
            if (luma(image.getPixelAt(x, y)) >= 0.2) {
                x0 = std::min(x0, x), y0 = std::min(y0, y), x1 = std::max(x1, x), y1 = std::max(y1, y);
            }
    return x1 < 0 ? juce::Rectangle<int>() : juce::Rectangle<int>(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
}

}  // namespace

TEST_CASE("P7.2.06 the head's and the console's words stand on the design's baselines, where the design puts them, "
          "and no two overlap") {
    Fixture f;
    f.perform(kPlayingMoment);  // the line playing: the share and the sub's number show
    const auto image = f.shot();
    const auto drawn = f.editor->drawnTexts();

    // the share's digits, whatever the share reads
    juce::String share;
    for (const auto& t : drawn)
        if (juce::Rectangle<int>(790, 360, 70, 40).contains(t.bounds.getCentre()) && t.text.trim().containsOnly("0123456789"))
            share = t.text.trim();
    REQUIRE_MESSAGE(share.isNotEmpty(), "while the line plays the spectrum shows its share below 150 Hz, at x 798, baseline "
                                        "390");
    const float stampEnd = 798.0f + width(theme::legendFont(7.5f, 1.6f), "SPECTRUM");
    const float percentX = 798.0f + width(theme::numberLightFont(22.0f), share) + 2.0f;
    const float labelX = percentX + width(theme::numberFont(12.0f, 0.0f), "%") + 6.0f;
    const float bandCentre = (plotX(30.0) + plotX(150.0)) / 2.0f;
    const auto dot = juce::String::charToString(0x00B7);
    const std::vector<Word> words{
        {"UNDERTOW", {0, 0, 200, 48}, 24.0f, Align::left, 29.5f},
        {"2", {180, 0, 30, 48}, 188.0f, Align::left, 29.5f},
        {"METAKOSMIA", {220, 0, 160, 48}, 230.0f, Align::left, 28.5f},
        {"130.0", {488, 12, 70, 26}, 502.0f, Align::left, 29.0f},
        {"LIVE", {1140, 0, 60, 48}, 1151.0f, Align::left, 27.0f},
        {"SPECTRUM", {790, 340, 90, 30}, 798.0f, Align::left, 360.0f},
        {"20 Hz - 2 kHz", {790, 340, 200, 30}, stampEnd + 8.0f, Align::left, 360.0f},
        {share, {790, 360, 70, 40}, 798.0f, Align::left, 390.0f},
        {"%", {800, 360, 80, 40}, percentX, Align::left, 390.0f},
        {"BELOW 150 Hz", {810, 360, 130, 40}, labelX, Align::left, 390.0f},
        {"49.00", {990, 338, 160, 70}, 1134.0f, Align::right, 386.0f},
        {"Hz", {1130, 360, 40, 40}, 1160.0f, Align::right, 386.0f},
        {"G1 " + dot + " THE SUB", {1040, 392, 130, 22}, 1160.0f, Align::right, 406.0f},
        {"30-150 Hz", {820, 550, 140, 20}, bandCentre, Align::centre, 0.0f},
        {"STEPS AWAY FROM HOME", {870, 296, 200, 20}, 884.0f, Align::left, 308.5f},
    };
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> boxes;
    for (const auto& w : words) {
        INFO("\"" << w.text << "\": expected at x " << w.x << ", baseline " << w.baseline);
        const auto* t = findText(drawn, w.text, w.area);
        CHECK_MESSAGE(t != nullptr, "the window draws it there (texts::draw records every word with its box)");
        if (t == nullptr)
            continue;
        const auto b = t->bounds.toFloat();
        const float at = w.align == Align::left ? b.getX() : (w.align == Align::right ? b.getRight() : b.getCentreX());
        CHECK_MESSAGE(std::abs(at - w.x) <= 2.5f, "its box is recorded at " << str(t->bounds) << ", its "
                      << (w.align == Align::left ? "left edge" : (w.align == Align::right ? "right edge" : "centre")) << " "
                      << at << " px; the design puts it at " << w.x);
        if (w.baseline > 0.0f) {
            const int row = standsOn(image, t->bounds);
            CHECK_MESSAGE(std::abs(static_cast<float>(row) - (w.baseline - 0.5f)) <= 1.5f, "its letters stand on pixel row "
                          << row << "; on the design's baseline they stand on row " << w.baseline - 0.5f << ", within 1.5 px. "
                          "A box for texts::draw runs from the baseline less the font's ascent to the baseline plus its descent");
        }
        boxes.push_back({w.text, inkBox(image, t->bounds)});
    }
    for (std::size_t i = 0; i < boxes.size(); ++i)
        for (std::size_t j = i + 1; j < boxes.size(); ++j)
            CHECK_MESSAGE(!boxes[i].second.intersects(boxes[j].second), "the ink of \"" << boxes[i].first << "\" at "
                          << str(boxes[i].second) << " overlaps the ink of \"" << boxes[j].first << "\" at "
                          << str(boxes[j].second));
}

TEST_CASE("P7.2.07 the spectrum's glass is quiet: a `+` every 32 px and dotted level lines, both text_phantom, and no "
          "line across it") {
    Fixture f;
    f.set("seq_mode", 1.0f);  // Free: no hint over the plot
    f.hold(idleFrame(), 30);  // silent: no curve, no share, no number
    const auto image = f.shot();
    const auto plot = kPlot;
    const float band0 = plotX(30.0) - 3.0f, band1 = plotX(150.0) + 3.0f;
    int loud = 0;
    juce::Point<int> first;
    for (int y = plot.getY() + 2; y < plot.getBottom() - 2; ++y)
        for (int x = plot.getX() + 2; x < plot.getRight() - 30; ++x) {
            if (x >= band0 && x <= band1)
                continue;
            if (luma(image.getPixelAt(x, y)) >= 0.13) {
                if (loud++ == 0)
                    first = {x, y};
            }
        }
    CHECK_MESSAGE(loud == 0, loud << " pixels of the plot's empty glass reach luma 0.13 (the first at " << first.x << ", "
                             << first.y << "): its crosses and level lines are text_phantom (#201820), with no line across "
                             "the glass");
    for (int y = 418; y <= 546; y += 32)
        for (int x = 800; x <= plot.getRight() - 30; x += 32) {
            if (x >= band0 - 4 && x <= band1 + 4)
                continue;
            INFO("the cross at " << x << ", " << y);
            CHECK_MESSAGE(luma(image.getPixelAt(x, y)) >= 0.06, "a `+` (5 px, text_phantom) is centred on every 32 px point "
                                                                "from 800, 354");
            double brightest = 0.0;
            for (int dy = -3; dy <= 3; ++dy)
                for (int dx = -3; dx <= 3; ++dx)
                    brightest = std::max(brightest, luma(image.getPixelAt(x + dx, y + dy)));
            CHECK_MESSAGE(brightest < 0.13, "the cross is text_phantom; around it the glass reaches luma " << brightest);
        }
}
