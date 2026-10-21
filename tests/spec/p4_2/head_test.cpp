// Phase 4.2 spec tests: the HEAD and the bone number as the design has them (docs/specs/phase-4.2-layout.md, P4.2.06 and
// P4.2.07; docs/design/ui-spec.md sections 4.2 and 4.9).
// spec-preset: plugin
// spec-needs: plugin/ui/Texts.h

#include "doctest.h"

#include "spec_ui.h"

#include <cmath>
#include <regex>
#include <string>
#include <vector>

using namespace spec;

namespace {

const juce::Rectangle<int> kHead{0, 0, 1200, 48}, kChest{312, 56, 576, 464};

std::string text(const juce::Rectangle<int>& r) { return r.toString().toStdString(); }

// Pixels of a drawn word: brighter than the HEAD strip, the panels and the noise floor (luminance 0.2 and up).
int inkIn(const juce::Image& image, juce::Rectangle<int> box) {
    juce::Image::BitmapData data(image, juce::Image::BitmapData::readOnly);
    int ink = 0;
    for (int y = std::max(0, box.getY()); y < std::min(image.getHeight(), box.getBottom()); ++y)
        for (int x = std::max(0, box.getX()); x < std::min(image.getWidth(), box.getRight()); ++x)
            ink += luminance(data.getPixelColour(x, y)) >= 0.2 ? 1 : 0;
    return ink;
}

// The drawn text inside `area` that `pattern` finds (searching within the text), or null.
const DrawnText* findText(const std::vector<DrawnText>& drawn, const std::string& pattern, juce::Rectangle<int> area) {
    const std::regex re(pattern);
    for (const auto& t : drawn)
        if (area.contains(t.bounds) && std::regex_search(t.text.toStdString(), re))
            return &t;
    return nullptr;
}

std::string listed(const std::vector<DrawnText>& drawn, juce::Rectangle<int> area) {
    std::string out;
    for (const auto& t : drawn)
        if (area.intersects(t.bounds))
            out += " \"" + t.text.toStdString() + "\" at " + text(t.bounds) + ";";
    return out.empty() ? " nothing" : out;
}

} // namespace

TEST_CASE("P4.2.06 the HEAD: the mark at the left, the clock in the middle, the output meter and LIVE at the right") {
    Fixture f;
    UiFrame frame = playingFrame();  // 128 BPM from the host, at beat 13.25: bar 4, beat 2, sixteenth 2
    const auto image = f.shot(frame);
    const auto drawn = f.editor->drawnTexts();
    INFO("the HEAD draws:" << listed(drawn, kHead));
    struct Field {
        const char* what;
        std::string pattern;
        int fromX, toX;  // where the text's box starts and ends, within these
    };
    const Field fields[] = {
        {"the mark's METAKOSMIA", "^\\s*METAKOSMIA\\s*$", 0, 320},
        {"the mark's UNDERTOW (wide tracking allowed)", "U\\s*N\\s*D\\s*E\\s*R\\s*T\\s*O\\s*W", 0, 400},
        {"the tempo, one decimal: 128.0 BPM", "(^|[^\\d.])128\\.0 BPM", 360, 840},
        {"the position, bar.beat.sixteenth: 004.2.2", "(^|[^\\d.])004\\.2\\.2([^\\d.]|$)", 360, 840},
        {"the HOST badge", "(^|\\W)HOST(\\W|$)", 360, 900},
        {"the LIVE lamp", "LIVE", 960, 1200},
    };
    for (const auto& field : fields) {
        const auto* t = findText(drawn, field.pattern, kHead);
        CHECK_MESSAGE(t != nullptr, "the HEAD must draw " << std::string(field.what));
        if (t == nullptr)
            continue;
        CHECK_MESSAGE((t->bounds.getX() >= field.fromX && t->bounds.getRight() <= field.toX),
                      std::string(field.what) << " is drawn at " << text(t->bounds) << "; it belongs between x " << field.fromX << " and " << field.toX);
        CHECK_MESSAGE(inkIn(image, t->bounds) >= 6, std::string(field.what) << " is recorded at " << text(t->bounds) << " but nothing is drawn there");
    }

    // On the internal clock the badge reads INT.
    UiFrame internal = playingFrame();
    internal.hostClock = false;
    f.shot(internal);
    const auto drawnInternal = f.editor->drawnTexts();
    CHECK_MESSAGE(findText(drawnInternal, "(^|\\W)INT(\\W|$)", kHead) != nullptr,
                  "on the internal clock the badge reads INT; the HEAD draws:" << listed(drawnInternal, kHead));
    CHECK_MESSAGE(findText(drawnInternal, "(^|\\W)HOST(\\W|$)", kHead) == nullptr, "on the internal clock no HOST badge shows");

    // The output meter sits at the right: between a silent and a loud frame (the same clock) only the HEAD's right part
    // changes.
    UiFrame silent = playingFrame(), loud = playingFrame();
    silent.peak = 0.0f;
    silent.rms = 0.0f;
    silent.energy = 0.0f;
    loud.peak = 0.7f;
    loud.rms = 0.3f;
    loud.energy = 0.8f;
    Fixture quietFixture, loudFixture;
    const auto quiet = quietFixture.shot(silent), noisy = loudFixture.shot(loud);
    juce::Image::BitmapData a(quiet, juce::Image::BitmapData::readOnly), b(noisy, juce::Image::BitmapData::readOnly);
    int changed = 0, leftmost = 1200;
    for (int y = 0; y < kHead.getBottom(); ++y)
        for (int x = 0; x < kHead.getRight(); ++x) {
            const auto p = a.getPixelColour(x, y), q = b.getPixelColour(x, y);
            if (std::abs(luminance(p) - luminance(q)) > 0.03) {
                ++changed;
                leftmost = std::min(leftmost, x);
            }
        }
    CHECK_MESSAGE(changed >= 20, "the HEAD's output meter must move with the output (" << changed << " pixels changed)");
    CHECK_MESSAGE(leftmost >= 900, "the output meter and the LIVE lamp sit at the HEAD's right; pixels changed from x " << leftmost);
}

TEST_CASE("P4.2.07 the bone number: the pitch as 55.00 Hz with the note name A1 under it, in the CHEST's lower third") {
    Fixture f;
    const auto image = f.shot(playingFrame());  // note 33, 55 Hz
    const auto drawn = f.editor->drawnTexts();
    INFO("the CHEST draws:" << listed(drawn, kChest));
    const auto* digits = findText(drawn, "^\\s*55\\.00\\s*(Hz)?\\s*$", kChest);
    const auto* hz = findText(drawn, "(^|\\s|\\d)Hz\\s*$", kChest);
    const auto* name = findText(drawn, "^\\s*A1\\s*$", kChest);
    CHECK_MESSAGE(digits != nullptr, "the number shows the pitch with two decimals: 55.00 (not the MIDI note)");
    CHECK_MESSAGE(hz != nullptr, "the number's unit Hz follows it");
    CHECK_MESSAGE(name != nullptr, "the note name A1 shows under the number");
    if (digits != nullptr && name != nullptr) {
        CHECK_MESSAGE(name->bounds.getY() >= digits->bounds.getCentreY(), "the note name (" << text(name->bounds)
                                                                                         << ") sits under the number (" << text(digits->bounds) << ")");
        CHECK_MESSAGE(inkIn(image, digits->bounds) >= 100, "55.00 is recorded at " << text(digits->bounds) << " but little is drawn there");
    }
    // The pixels: bone, in one block, five characters wide at 56 px, in the lower third of the CHEST.
    const auto playing = measure(image, kChest);
    CHECK_MESSAGE(playing.bone >= 400, "only " << playing.bone << " bone pixels in the CHEST; the number is 56 px tall");
    CHECK_MESSAGE(playing.boneBox.getWidth() >= 120, "the bone pixels span " << text(playing.boneBox)
                                                                             << "; 55.00 at 56 px is about five characters, 150 px or more");
    const int lowerThird = kChest.getY() + kChest.getHeight() * 2 / 3;
    CHECK_MESSAGE(playing.boneBox.getCentreY() >= lowerThird, "the number is centred at y " << playing.boneBox.getCentreY()
                                                                                         << "; the CHEST's lower third starts at " << lowerThird);

    // A new note: 110.00 and A2.
    Fixture g;
    UiFrame a2 = playingFrame();
    a2.note = 45;
    a2.noteHz = 110.0f;
    g.shot(a2, 60);
    const auto drawnA2 = g.editor->drawnTexts();
    CHECK_MESSAGE(findText(drawnA2, "^\\s*110\\.00\\s*(Hz)?\\s*$", kChest) != nullptr, "note 45 shows 110.00; the CHEST draws:"
                                                                                       << listed(drawnA2, kChest));
    CHECK_MESSAGE(findText(drawnA2, "^\\s*A2\\s*$", kChest) != nullptr, "note 45 is A2");
}
