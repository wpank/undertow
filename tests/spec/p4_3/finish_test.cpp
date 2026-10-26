// Phase 4.3 spec tests: the editor's finish: the words as the design writes them, the clip bands, the number's Hz and
// the pump field (docs/specs/phase-4.3-finish.md, P4.3.05 to P4.3.08).
// spec-preset: plugin

#include "doctest.h"

#include "spec_ui.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace spec;

namespace {

const juce::Rectangle<int> kHead{0, 0, 1200, 48}, kGround{0, 672, 1200, 88}, kChest{312, 56, 576, 464},
    kPump{912, 80, 264, 96};
const auto kPumpInside = kPump.reduced(2);  // the field without a frame drawn on its edge

std::string text(const juce::Rectangle<int>& r) { return r.toString().toStdString(); }

std::string listed(const std::vector<DrawnText>& drawn, juce::Rectangle<int> area) {
    std::string out;
    for (const auto& t : drawn)
        if (area.intersects(t.bounds))
            out += " \"" + t.text.toStdString() + "\" at " + text(t.bounds) + ";";
    return out.empty() ? " nothing" : out;
}

// A character that only mojibake makes: a C1 control (UTF-8's continuation bytes read as Latin-1) or U+FFFD.
bool garbled(const juce::String& s) {
    for (auto p = s.getCharPointer(); !p.isEmpty(); ++p) {
        const auto c = *p;
        if ((c >= 0x80 && c <= 0x9F) || c == 0xFFFD)
            return true;
    }
    return false;
}

int inkIn(const juce::Image& image, juce::Rectangle<int> box) {
    juce::Image::BitmapData data(image, juce::Image::BitmapData::readOnly);
    int ink = 0;
    for (int y = std::max(0, box.getY()); y < std::min(image.getHeight(), box.getBottom()); ++y)
        for (int x = std::max(0, box.getX()); x < std::min(image.getWidth(), box.getRight()); ++x)
            ink += luminance(data.getPixelColour(x, y)) >= 0.2 ? 1 : 0;
    return ink;
}

bool near(juce::Colour c, int r, int g, int b, int d) {
    const int dr = c.getRed() - r, dg = c.getGreen() - g, db = c.getBlue() - b;
    return dr * dr + dg * dg + db * db <= d * d;
}

} // namespace

TEST_CASE("P4.3.05 the words are the design's: exact Unicode, nothing garbled, and the HEAD on one line") {
    Fixture f;
    f.shot(playingFrame());
    const auto drawn = f.editor->drawnTexts();
    for (const auto& t : drawn)
        CHECK_MESSAGE(!garbled(t.text), "\"" << t.text.toStdString() << "\" at " << text(t.bounds)
                                             << " is garbled: a UTF-8 literal went through juce::String(const char*); use "
                                                "juce::String::fromUTF8 or juce::CharPointer_UTF8");
    const auto mark = juce::String::fromUTF8("\xe2\x8c\x88 U N D E R T O W \xe2\x8c\x8b");
    const auto lit = juce::String::fromUTF8("\xe2\x97\x89 LIVE"), dark = juce::String::fromUTF8("\xe2\x97\x8b LIVE");
    bool markFound = false, litFound = false;
    std::vector<const DrawnText*> head;
    for (const auto& t : drawn) {
        if (!kHead.contains(t.bounds))
            continue;
        head.push_back(&t);
        markFound = markFound || t.text.trim() == mark;
        litFound = litFound || t.text.trim() == lit;
    }
    INFO("the HEAD draws:" << listed(drawn, kHead));
    CHECK_MESSAGE(markFound, "the mark must read exactly \"" << mark.toStdString() << "\"");
    CHECK_MESSAGE(litFound, "while the output plays the lamp must read exactly \"" << lit.toStdString() << "\"");
    int top = 1 << 30, bottom = -1;
    for (const auto* t : head) {
        top = std::min(top, t->bounds.getCentreY());
        bottom = std::max(bottom, t->bounds.getCentreY());
    }
    CHECK_MESSAGE(bottom - top <= 4, "the HEAD's words sit on one line (the mock's \"METAKOSMIA ⌈ U N D E R T O W ⌋  128.0 BPM "
                                     "...  LIVE\"); their centres run from y "
                                         << top << " to " << bottom);

    Fixture g;
    g.shot(idleFrame());
    bool darkFound = false;
    for (const auto& t : g.editor->drawnTexts())
        darkFound = darkFound || (kHead.contains(t.bounds) && t.text.trim() == dark);
    CHECK_MESSAGE(darkFound, "while the output is silent the lamp must read exactly \"" << dark.toStdString() << "\"");
}

TEST_CASE("P4.3.06 the clip bands are 16 px with their legend, and everything outside them stays readable") {
    Fixture calm, alarm;
    const auto still = calm.shot(playingFrame()), clipping = alarm.shot(clippingFrame());
    const auto drawnStill = calm.editor->drawnTexts(), drawnClipping = alarm.editor->drawnTexts();
    juce::Image::BitmapData a(still, juce::Image::BitmapData::readOnly), b(clipping, juce::Image::BitmapData::readOnly);
    // 16 px bands: the rows just inside them, 16 to 23 and 736 to 743, gain no stripes.
    for (const int from : {16, 736}) {
        int added = 0, pixels = 0;
        for (int y = from; y < from + 8; ++y)
            for (int x = 0; x < 1200; ++x, ++pixels)
                added += (roseBright(b.getPixelColour(x, y)) && !roseBright(a.getPixelColour(x, y))) ? 1 : 0;
        CHECK_MESSAGE(added <= pixels / 50, "while clipping, rows " << from << " to " << from + 7 << " gain " << added
                                                                    << " rose_bright pixels: the hazard bands are 16 px, y 0 to 15 and 744 to 759");
    }
    // The legend: a bg_void plate in the middle of the top band.
    const auto legend = juce::String::fromUTF8("\xe2\x8c\x88 CLIP \xe2\x8c\x8b");
    const DrawnText* plate = nullptr;
    for (const auto& t : drawnClipping)
        if (t.text.contains(legend) && t.text.contains("OUTPUT -0.3 dBFS"))
            plate = &t;
    CHECK_MESSAGE(plate != nullptr, "while clipping the top band carries the legend \"" << legend.toStdString()
                                                                                       << "  OUTPUT -0.3 dBFS\"; the editor draws:"
                                                                                       << listed(drawnClipping, {0, 0, 1200, 24}));
    if (plate != nullptr) {
        CHECK_MESSAGE((plate->bounds.getY() >= 0 && plate->bounds.getBottom() <= 18), "the legend (" << text(plate->bounds)
                                                                                                     << ") sits in the top band");
        CHECK_MESSAGE(std::abs(plate->bounds.getCentreX() - 600) <= 150, "the legend sits in the middle of the band; it is at "
                                                                             << text(plate->bounds));
        CHECK_MESSAGE(inkIn(clipping, plate->bounds) >= 10, "the legend is recorded at " << text(plate->bounds) << " but not drawn");
    }
    // Readable while clipping: the HEAD's words and GROUND's labels keep their pixels outside the bands.
    for (const auto& t : drawnStill) {
        const bool inHead = kHead.contains(t.bounds), inGround = kGround.contains(t.bounds);
        if (!inHead && !inGround)
            continue;
        const auto box = t.bounds.getIntersection({0, 16, 1200, 728});
        int changed = 0, pixels = 0;
        for (int y = box.getY(); y < box.getBottom(); ++y)
            for (int x = box.getX(); x < box.getRight(); ++x, ++pixels)
                changed += std::abs(luminance(a.getPixelColour(x, y)) - luminance(b.getPixelColour(x, y))) > 0.08 ? 1 : 0;
        CHECK_MESSAGE(changed <= std::max(2, pixels * 3 / 100), "\"" << t.text.toStdString() << "\" (" << text(t.bounds) << ") changes "
                                                                     << changed << " of its " << pixels
                                                                     << " pixels outside the bands while clipping: it must stay readable");
    }
}

TEST_CASE("P4.3.07 Hz follows the number on its baseline, and the number sits below the scope") {
    Fixture f;
    const auto image = f.shot(playingFrame());
    juce::Image::BitmapData data(image, juce::Image::BitmapData::readOnly);
    // The digits: bone. Hz and the note name: bone_dim.
    int left = 1 << 30, right = -1, top = 1 << 30, bottom = -1;
    for (int y = kChest.getY(); y < kChest.getBottom(); ++y)
        for (int x = kChest.getX(); x < kChest.getRight(); ++x)
            if (near(data.getPixelColour(x, y), 200, 184, 144, 36) && data.getPixelColour(x, y).getBrightness() >= 0.6f) {
                left = std::min(left, x);
                right = std::max(right, x);
                top = std::min(top, y);
                bottom = std::max(bottom, y);
            }
    REQUIRE_MESSAGE(right > left, "the number 55.00 must be drawn in bone");
    int hz = 0, hzBottom = -1;
    for (int y = top - 4; y <= bottom + 4; ++y)
        for (int x = right + 4; x < kChest.getRight(); ++x)
            if (near(data.getPixelColour(x, y), 138, 122, 90, 26)) {
                ++hz;
                hzBottom = std::max(hzBottom, y);
            }
    CHECK_MESSAGE(hz >= 20, "Hz must follow the number: no bone_dim pixels right of 55.00 (its digits end at x " << right
                                                                                                                 << ", y " << top << " to "
                                                                                                                 << bottom << ")");
    if (hz >= 20)
        CHECK_MESSAGE(std::abs(hzBottom - bottom) <= 3, "Hz must sit on the number's baseline: its bottom is y " << hzBottom
                                                                                                                << ", the digits' " << bottom);
    const auto scope = f.editor->scopeBounds();
    CHECK_MESSAGE(top >= scope.getBottom() - 12, "the number (digits from y " << top << ") must sit below the scope (bottom y "
                                                                             << scope.getBottom()
                                                                             << "), not across its trace");
}

TEST_CASE("P4.3.08 the rumble pump field fills RUMBLE/04's display as the rumble plays, and stays dark without it") {
    auto run = [](float rumble) {
        Fixture f;
        UiFrame frame = playingFrame();
        frame.rumble = rumble;
        frame.ppq = 0.0;
        for (int i = 0; i < 240; ++i) {  // 4 s at 60 Hz: 8.5 beats at 128 BPM
            frame.ppq += 1.0 / 60.0 * 128.0 / 60.0;
            f.editor->advance(frame, 1.0 / 60.0);
        }
        const auto image = f.editor->createComponentSnapshot(f.editor->getLocalBounds(), true, 1.0f);
        juce::Image::BitmapData data(image, juce::Image::BitmapData::readOnly);
        int lit = 0;
        for (int y = kPumpInside.getY(); y < kPumpInside.getBottom(); ++y)
            for (int x = kPumpInside.getX(); x < kPumpInside.getRight(); ++x)
                lit += (luminance(data.getPixelColour(x, y)) >= 0.12 && roseHue(data.getPixelColour(x, y))) ? 1 : 0;
        return lit;
    };
    const int area = kPumpInside.getWidth() * kPumpInside.getHeight();
    const int playing = run(0.6f), silent = run(0.0f);
    CHECK_MESSAGE(playing >= area / 25, "after 8 beats of rumble the pump field (" << text(kPump) << ") lights only " << playing << " of the "
                                                                                  << area << " pixels inside its edge: RUMBLE/04 shows the "
                                                                                             "rumble folded by the beat (docs/design/ui-spec.md 4.6)");
    CHECK_MESSAGE(silent <= area / 200, "with no rumble the pump field must stay dark; " << silent << " of the " << area
                                                                                       << " pixels inside its edge are lit");
}
