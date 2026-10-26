// Phase 5 spec tests: the one number at the design's size, every glyph of it drawn, and the last pitch held while it
// fades (docs/specs/phase-5-ship.md, P5.15 to P5.17; docs/design/ui-spec.md 4.2).
// spec-preset: plugin

#include "doctest.h"

#include "spec_ui.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <tuple>
#include <vector>

using namespace spec;

namespace {

const juce::Rectangle<int> kChest{312, 56, 576, 464};

// The ink of the number: bone at full strength. Hz and the note name are bone_dim, too dark to count.
struct Ink {
    int left = 1 << 30, right = -1, top = 1 << 30, bottom = -1, pixels = 0;
    bool found() const { return right >= left; }
    int width() const { return right - left + 1; }
    int height() const { return bottom - top + 1; }
    std::string box() const {
        return "x " + std::to_string(left) + " to " + std::to_string(right) + ", y " + std::to_string(top) + " to " +
               std::to_string(bottom);
    }
};

Ink boneInk(const juce::Image& image, juce::Rectangle<int> area) {
    juce::Image::BitmapData data(image, juce::Image::BitmapData::readOnly);
    Ink ink;
    for (int y = std::max(0, area.getY()); y < std::min(image.getHeight(), area.getBottom()); ++y)
        for (int x = std::max(0, area.getX()); x < std::min(image.getWidth(), area.getRight()); ++x) {
            const auto c = data.getPixelColour(x, y);
            const int dr = c.getRed() - 200, dg = c.getGreen() - 184, db = c.getBlue() - 144;
            if (dr * dr + dg * dg + db * db > 36 * 36 || c.getBrightness() < 0.6f)
                continue;
            ink.left = std::min(ink.left, x);
            ink.right = std::max(ink.right, x);
            ink.top = std::min(ink.top, y);
            ink.bottom = std::max(ink.bottom, y);
            ++ink.pixels;
        }
    return ink;
}

// The same text as the design draws it: bone, 56 px mono, bold as phase 4 drew it, alone on bg_void, measured the same
// way.
Ink designInk(const juce::String& text) {
    juce::Image image(juce::Image::ARGB, 640, 160, true);
    {
        juce::Graphics g(image);
        g.fillAll(juce::Colour::fromRGB(0x06, 0x06, 0x08));
        g.setColour(juce::Colour::fromRGB(0xC8, 0xB8, 0x90).withAlpha(0.96f));
        g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 56.0f, juce::Font::bold)));
        g.drawSingleLineText(text, 40, 110);
    }
    return boneInk(image, image.getBounds());
}

UiFrame noteFrame(int note, float hz) {
    UiFrame frame = playingFrame();
    frame.note = note;
    frame.noteHz = hz;
    return frame;
}

// The number as recorded, without a unit drawn in the same string: "110.00" from "110.00" or "110.00 Hz".
juce::String numberIn(const std::vector<DrawnText>& drawn) {
    for (const auto& t : drawn) {
        auto s = t.text.trim();
        if (s.endsWith("Hz"))
            s = s.dropLastCharacters(2).trim();
        if (kChest.contains(t.bounds) && s.isNotEmpty() && s.containsOnly("0123456789.") && s.containsChar('.'))
            return s;
    }
    return {};
}

} // namespace

TEST_CASE("P5.15 every glyph of the number is drawn: its ink spans the whole string, 55.00 and 110.00, never 55.0") {
    for (const auto& [note, hz, digits] : std::vector<std::tuple<int, float, const char*>>{{33, 55.0f, "55.00"}, {45, 110.0f, "110.00"}}) {
        Fixture f;
        const auto ink = boneInk(f.shot(noteFrame(note, hz)), kChest);
        const auto design = designInk(digits);
        const std::string shown = digits;
        REQUIRE_MESSAGE(ink.found(), "the number " << shown << " must be drawn in bone in the CHEST");
        REQUIRE(design.found());
        // At whatever size the digits stand, the whole string spans this much ink (P5.16 holds the size).
        const int expected = static_cast<int>(std::lround(design.width() * static_cast<double>(ink.height()) / design.height()));
        CHECK_MESSAGE(ink.width() >= expected - 4,
                      "the number " << shown << " draws only part of itself: its ink spans " << ink.width() << " px (" << ink.box()
                                    << "), and the whole string at that height spans " << expected
                                    << " px. A box exactly as wide as the string makes Graphics::drawText drop the glyph that "
                                       "doesn't fit (55.0 for 55.00): give the box room, or size it from the glyphs' own extent");
    }
}

TEST_CASE("P5.16 the number stands at the design's 56 px: its digits as tall as 55.00 in 56 px mono") {
    Fixture f;
    const auto ink = boneInk(f.shot(playingFrame()), kChest);
    const auto design = designInk("55.00");
    REQUIRE_MESSAGE(ink.found(), "the number 55.00 must be drawn in bone in the CHEST");
    REQUIRE(design.found());
    CHECK_MESSAGE(ink.height() >= design.height() - 2,
                  "the digits stand " << ink.height() << " px tall (" << ink.box() << "); 55.00 in 56 px mono stands " << design.height()
                                      << " px. docs/design/ui-spec.md 4.2: the pitch as 55.00 in bone 56 px mono, the brightest thing "
                                         "on the panel");
}

TEST_CASE("P5.17 while the number fades after the note ends, it shows the last pitch, not a stand-in") {
    Fixture f;
    f.shot(noteFrame(45, 110.0f), 60);  // A2 at 110 Hz
    UiFrame idle = playingFrame();
    idle.note = -1;  // the engine's meters after the note: no note, noteHz 0
    idle.noteHz = 0.0f;
    f.shot(idle, 6);  // 100 ms into the fade
    const auto drawn = f.editor->drawnTexts();
    const auto number = numberIn(drawn);
    std::string listed;
    for (const auto& t : drawn)
        if (kChest.contains(t.bounds))
            listed += " \"" + t.text.toStdString() + "\";";
    REQUIRE_MESSAGE(number.isNotEmpty(), "100 ms after the note ends the number is still fading out (docs/design/ui-spec.md 4.2: the "
                                         "last pitch fades over 600 ms); the CHEST draws:"
                                             << listed);
    CHECK_MESSAGE(number == "110.00", "the fading number reads \"" << number.toStdString()
                                                                 << "\"; it must keep the last pitch, 110.00 (A2): hold the pitch "
                                                                    "while the note plays, since the meters report 0 Hz once it ends");
}
