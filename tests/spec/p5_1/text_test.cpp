// Phase 5.1 spec tests: no word is cut. texts::draw draws every glyph whatever its box, and every word the editor
// draws is whole in its pixels (docs/specs/phase-5.1-renders.md, P5.1.06 and P5.1.07).
// spec-preset: plugin

#include "doctest.h"

#include "spec_ui.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace spec;

namespace {

// The font a word was drawn in, once DrawnText records it (the API in docs/specs/phase-5.1-renders.md).
template <typename T, typename = void>
struct RecordsFont : std::false_type {};
template <typename T>
struct RecordsFont<T, std::void_t<decltype(std::declval<const T&>().font)>> : std::true_type {};

template <typename Entry>
juce::Font fontOf(const Entry& entry) {
    if constexpr (RecordsFont<Entry>::value)
        return entry.font;
    else
        return juce::Font(juce::FontOptions{});
}

struct Span {
    int left = 1 << 30, right = -1;
    int width() const { return right >= left ? right - left + 1 : 0; }
};

// The columns of `box` that hold ink. Text here is always lighter than what it is drawn on, so ink is a pixel
// brighter than the box's background (its median luminance) by at least half the brightest pixel's margin.
Span inkSpan(const juce::Image& image, juce::Rectangle<int> box) {
    box = box.getIntersection(image.getBounds());
    Span span;
    if (box.isEmpty())
        return span;
    juce::Image::BitmapData data(image, juce::Image::BitmapData::readOnly);
    std::vector<double> levels;
    levels.reserve(static_cast<std::size_t>(box.getWidth() * box.getHeight()));
    for (int y = box.getY(); y < box.getBottom(); ++y)
        for (int x = box.getX(); x < box.getRight(); ++x)
            levels.push_back(luminance(data.getPixelColour(x, y)));
    auto sorted = levels;
    std::nth_element(sorted.begin(), sorted.begin() + static_cast<long>(sorted.size() / 2), sorted.end());
    const double median = sorted[sorted.size() / 2];
    const double brightest = *std::max_element(levels.begin(), levels.end());
    const double threshold = median + std::max(0.04, 0.5 * (brightest - median));
    std::size_t i = 0;
    for (int y = box.getY(); y < box.getBottom(); ++y)
        for (int x = box.getX(); x < box.getRight(); ++x, ++i)
            if (levels[i] >= threshold) {
                span.left = std::min(span.left, x);
                span.right = std::max(span.right, x);
            }
    return span;
}

// The word's own ink width at `font`: drawn alone, whole, white on black.
int ownInk(const juce::String& text, const juce::Font& font) {
    const int width = static_cast<int>(std::ceil(juce::GlyphArrangement::getStringWidth(font, text))) + 40;
    const int height = static_cast<int>(std::ceil(font.getHeight())) + 20;
    juce::Image image(juce::Image::ARGB, width, height, true);
    {
        juce::Graphics g(image);
        g.fillAll(juce::Colours::black);
        g.setColour(juce::Colours::white);
        g.setFont(font);
        g.drawSingleLineText(text, 20, 10 + static_cast<int>(std::ceil(font.getAscent())));
    }
    return inkSpan(image, image.getBounds()).width();
}

std::string text(const juce::Rectangle<int>& r) { return r.toString().toStdString(); }

} // namespace

TEST_CASE("P5.1.06 texts::draw draws every glyph whatever its box: a word wider than its box is drawn whole, and "
          "recorded where it was drawn") {
    using namespace undertow::ui::theme;
    const std::vector<std::pair<juce::String, juce::Font>> words{
        {"Hz", monoFont(18.0f)},
        {"55.00", monoFont(56.0f, juce::Font::bold)},
        {"SUB OCT", sansFont(10.0f)},
        {juce::String::fromUTF8("\xe2\x8c\x88 CLIP \xe2\x8c\x8b  OUTPUT -0.3 dBFS"), monoFont(11.0f, juce::Font::bold)}};
    for (const auto& word : words) {
        const int own = ownInk(word.first, word.second);
        const int needed = static_cast<int>(std::ceil(juce::GlyphArrangement::getStringWidth(word.second, word.first)));
        const std::vector<std::pair<const char*, juce::Justification>> justifications{
            {"left", juce::Justification::centredLeft}, {"centred", juce::Justification::centred},
            {"right", juce::Justification::centredRight}};
        for (const auto& placed : justifications) {
            const auto justification = placed.second;
            juce::Image image(juce::Image::ARGB, needed + 200, static_cast<int>(std::ceil(word.second.getHeight())) + 40, true);
            std::vector<DrawnText> drawn;
            {
                juce::Graphics g(image);
                g.fillAll(juce::Colours::black);
                g.setColour(juce::Colours::white);
                g.setFont(word.second);
                texts::record() = &drawn;
                // A box 6 px narrower than the word, in the middle of the image.
                texts::draw(g, word.first, {100, 20, std::max(1, needed - 6), static_cast<int>(std::ceil(word.second.getHeight()))},
                            justification);
                texts::record() = nullptr;
            }
            const auto ink = inkSpan(image, image.getBounds());
            const std::string where = placed.first;
            INFO("\"" << word.first.toStdString() << "\" at " << word.second.getHeight() << " px, justified " << where);
            CHECK_MESSAGE(ink.width() >= own - 2, "drawn in a box 6 px narrower than itself, the word shows " << ink.width()
                                                                                                            << " px of ink; whole it is " << own
                                                                                                            << " px. texts::draw must never drop a glyph "
                                                                                                               "(Graphics::drawText drops what doesn't fit its box)");
            REQUIRE(drawn.size() == 1);
            const auto& box = drawn.front().bounds;
            CHECK_MESSAGE((ink.width() == 0 || (ink.left >= box.getX() - 1 && ink.right <= box.getRight())),
                          "the word's ink runs x " << ink.left << " to " << ink.right << ", outside the box recorded for it ("
                                                   << text(box) << "): record the box the word was drawn in");
        }
    }
}

TEST_CASE("P5.1.07 every word the editor draws is whole: its box fits it at its font, and its ink spans its own width") {
    REQUIRE_MESSAGE(RecordsFont<DrawnText>::value, "DrawnText must record the font each word was drawn in (`juce::Font font`, "
                                                   "docs/specs/phase-5.1-renders.md); without it no check can tell a cut word");
    const std::vector<std::pair<std::string, UiFrame>> states{
        {"idle", idleFrame()}, {"playing", playingFrame()}, {"clipping", clippingFrame()}};
    for (const auto& state : states) {
        const std::string name = state.first;
        Fixture f;
        const auto image = f.shot(state.second);
        for (const auto& entry : f.editor->drawnTexts()) {
            if (entry.text.trim().isEmpty())
                continue;
            const auto font = fontOf(entry);
            const int needed = static_cast<int>(std::ceil(juce::GlyphArrangement::getStringWidth(font, entry.text)));
            const int own = ownInk(entry.text, font);
            const auto ink = inkSpan(image, entry.bounds);
            INFO(name << ": \"" << entry.text.toStdString() << "\" in " << text(entry.bounds) << " at " << font.getHeight() << " px");
            CHECK_MESSAGE(entry.bounds.getWidth() >= needed - 1, "its box is " << entry.bounds.getWidth() << " px wide; the word needs "
                                                                               << needed << " at its font");
            CHECK_MESSAGE(ink.width() >= own - 3, "it shows " << ink.width() << " px of ink in its box; whole it is " << own
                                                              << " px: a glyph is cut (\"Hz\" drawn as \"H\")");
        }
    }
}
