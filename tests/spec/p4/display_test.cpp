// Phase 4 spec tests: the displays: palette, bone number, hazard stripes, tunnel and scope (docs/specs/phase-4-ui.md, P4.05 to P4.09).
// spec-preset: plugin
// spec-needs: plugin/ui/UiFrame.h

#include "doctest.h"

#include "spec_ui.h"
#include "undertow/params.h"
#include "undertow/sequencer.h"

#include <chrono>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

using namespace spec;

TEST_CASE("P4.05 the panel is void and rose: at least half void, no white, rose dominant, bone rare") {
    Fixture f;
    // At rest the CHEST is calm: the void ground with a dim tunnel, its grid and the AT field.
    const auto rest = measure(f.shot(idleFrame()), f.editor->chestBounds());
    CHECK_MESSAGE(rest.voidPixels >= rest.pixels * 6 / 10, "at rest only " << 100.0 * rest.voidPixels / rest.pixels
                                                                            << "% of the CHEST is void (luminance under 0.06); "
                                                                               "want 60% or more: fill the ground with bg_void "
                                                                               "and keep the tunnel dim without sound");
    const auto image = f.shot(playingFrame());
    const auto all = measure(image, f.editor->getLocalBounds());
    CHECK_MESSAGE(all.voidPixels >= all.pixels / 2, "only " << 100.0 * all.voidPixels / all.pixels
                                                            << "% of the panel is void (luminance under 0.06); restraint asks "
                                                               "for at least half");
    CHECK_MESSAGE(all.nearWhite == 0, all.nearWhite << " near-white pixels: ROSEDUST never uses white (JUCE's LookAndFeel_V4 draws "
                                                       "toggle text, ticks and tooltips white until the LookAndFeel sets their colours)");
    REQUIRE_MESSAGE(all.colouredPixels > 0, "the panel has no coloured pixels at all");
    CHECK_MESSAGE(all.rose >= all.colouredPixels * 3 / 4, "only " << 100.0 * all.rose / all.colouredPixels
                                                                  << "% of the coloured pixels are in the rose hue band (300 to 15 "
                                                                     "degrees); want 75% or more: colour is a taxonomy, mostly rose");
    CHECK_MESSAGE(all.bone <= all.pixels * 3 / 100, "bone covers " << 100.0 * all.bone / all.pixels
                                                                   << "% of the panel; it is for the one number only (3% at most)");
}


TEST_CASE("P4.06 the one number: the pitch in bone while a note plays, in one place in the CHEST, and no bone when idle") {
    Fixture f;
    const auto idle = measure(f.shot(idleFrame()), f.editor->getLocalBounds());
    CHECK_MESSAGE(idle.bone <= idle.pixels / 10000, "before any note the panel shows " << idle.bone
                                                                                        << " bone pixels; bone appears only "
                                                                                           "when there is a pitch to show");
    const auto playing = measure(f.shot(playingFrame()), f.editor->getLocalBounds());
    CHECK_MESSAGE(playing.bone >= playing.pixels / 2000, "while note 33 (55 Hz) plays the bone number must show (only "
                                                             << playing.bone << " bone pixels)");
    const auto chest = f.editor->chestBounds();
    CHECK_MESSAGE(chest.contains(playing.boneBox), "the bone pixels span " << playing.boneBox.toString()
                                                                           << ", outside the CHEST " << chest.toString()
                                                                           << ": bone is for the one number");
    CHECK_MESSAGE(playing.boneBox.getWidth() * playing.boneBox.getHeight() <= chest.getWidth() * chest.getHeight() * 15 / 100,
                  "the bone pixels spread over " << playing.boneBox.toString() << "; the one number sits in one place");
}


TEST_CASE("P4.07 the NERV hazard stripes show while the output clips, and only then") {
    Fixture f;
    const auto top = juce::Rectangle<int>(0, 0, 1200, 16), bottom = juce::Rectangle<int>(0, 744, 1200, 16);
    const auto calm = f.shot(playingFrame());
    const auto calmTop = measure(calm, top);
    CHECK_MESSAGE(calmTop.bright <= calmTop.pixels / 100, "without clipping the top band must be quiet ("
                                                              << 100.0 * calmTop.bright / calmTop.pixels << "% rose_bright)");
    const auto alarm = f.shot(clippingFrame());
    const auto alarmTop = measure(alarm, top), alarmBottom = measure(alarm, bottom);
    CHECK_MESSAGE(alarmTop.bright >= alarmTop.pixels / 4, "while clipping the top band must carry the hazard stripes (only "
                                                              << 100.0 * alarmTop.bright / alarmTop.pixels << "% rose_bright)");
    CHECK_MESSAGE(alarmBottom.bright >= alarmBottom.pixels / 4, "while clipping the bottom band must carry the hazard stripes (only "
                                                                    << 100.0 * alarmBottom.bright / alarmBottom.pixels
                                                                    << "% rose_bright)");
}


TEST_CASE("P4.08 the tunnel follows the sound: brighter with energy, and moving") {
    Fixture f;
    UiFrame quiet = idleFrame();
    UiFrame loud = idleFrame();
    loud.energy = 1.0f;
    const auto chest = f.editor->chestBounds();
    const double dark = mean(measure(f.shot(quiet, 60), chest));
    const auto first = f.shot(loud, 60);
    const double bright = mean(measure(first, chest));
    CHECK_MESSAGE(bright >= 1.4 * dark, "the CHEST's mean luminance is " << bright << " at full energy and " << dark
                                                                         << " at silence; the tunnel must be at least 1.4x "
                                                                            "brighter when the sound is loud");
    const auto second = f.shot(loud, 1);
    juce::Image::BitmapData a(first, juce::Image::BitmapData::readOnly), b(second, juce::Image::BitmapData::readOnly);
    double difference = 0;
    int count = 0;
    for (int y = chest.getY(); y < chest.getBottom(); y += 2)
        for (int x = chest.getX(); x < chest.getRight(); x += 2) {
            difference += std::fabs(luminance(a.getPixelColour(x, y)) - luminance(b.getPixelColour(x, y))) * 255.0;
            ++count;
        }
    CHECK_MESSAGE(difference / count >= 0.5, "two frames 1/60 s apart at full energy differ by only " << difference / count
                                                                                                      << " levels: the tunnel "
                                                                                                         "must move");
}


TEST_CASE("P4.09 the scope draws the waveform across its width") {
    Fixture f;
    const auto image = f.shot(playingFrame());
    const auto scope = f.editor->scopeBounds();
    REQUIRE_MESSAGE(f.editor->chestBounds().contains(scope), "the scope sits inside the CHEST");
    juce::Image::BitmapData data(image, juce::Image::BitmapData::readOnly);
    int covered = 0, top = scope.getBottom(), bottom = scope.getY();
    for (int x = scope.getX(); x < scope.getRight(); ++x) {
        bool hit = false;
        for (int y = scope.getY(); y < scope.getBottom(); ++y) {
            const auto c = data.getPixelColour(x, y);
            if (roseHue(c) && c.getSaturation() >= 0.15f && c.getBrightness() >= 0.45f) {
                hit = true;
                top = std::min(top, y);
                bottom = std::max(bottom, y);
            }
        }
        covered += hit ? 1 : 0;
    }
    CHECK_MESSAGE(covered >= scope.getWidth() * 9 / 10, "the trace covers " << covered << " of " << scope.getWidth()
                                                                           << " scope columns; draw the waveform across the "
                                                                              "scope's width");
    CHECK_MESSAGE(bottom - top >= scope.getHeight() * 2 / 5, "a 0.5 sine must span at least 40% of the scope's height; the "
                                                             "trace spans "
                                                                 << bottom - top << " of " << scope.getHeight() << " px");
}

