// SPDX-License-Identifier: AGPL-3.0-only
// Phase 6 spec tests: the console's live displays: the spectrum, the sub's number, the drift arc, the head, the meter,
// the hazard stripes and the rumble (docs/specs/phase-6-window.md, checks P6.26 to P6.34; docs/design/window.md
// sections 4.2 to 4.9).
// spec-preset: plugin
// spec-needs: plugin/ui/Console.cpp

#include "doctest.h"

#include "spec_window.h"

#include <cmath>
#include <string>
#include <utility>
#include <vector>

using namespace spec;

namespace {

// The topmost rose_bright pixel of the plot: the curve's peak. Returns {-1, -1} if there is none.
juce::Point<int> curvePeak(const juce::Image& image) {
    juce::Image::BitmapData data(image, juce::Image::BitmapData::readOnly);
    for (int y = kPlot.getY(); y < kPlot.getBottom(); ++y)
        for (int x = kPlot.getX() + 1; x < kPlot.getRight() - 1; ++x)
            if (roseBright(data.getPixelColour(x, y))) {
                // the peak's x: the middle of the bright run on this row
                int end = x;
                while (end + 1 < kPlot.getRight() && roseBright(data.getPixelColour(end + 1, y)))
                    ++end;
                return {(x + end) / 2, y};
            }
    return {-1, -1};
}

const DrawnText* textIn(const std::vector<DrawnText>& drawn, juce::Rectangle<int> area, const juce::String& text) {
    return findText(drawn, text, area);
}

int brightAround(const juce::Image& image, juce::Point<float> c, int r) {
    return measure(image, juce::Rectangle<float>(c.x - r, c.y - r, 2.0f * r, 2.0f * r).toNearestInt()).bright;
}

}  // namespace

TEST_CASE("P6.26 the spectrum draws a sine at its own frequency and level, and silence draws no curve") {
    for (const auto& [hz, db] : std::vector<std::pair<double, double>>{{49.0, -24.0}, {98.0, -30.0}, {150.0, -36.0}}) {
        Fixture f;
        f.hold(sineFrame(hz, db), 30);
        const auto peak = curvePeak(f.shot());
        INFO("a " << hz << " Hz sine at " << db << " dBFS");
        REQUIRE_MESSAGE(peak.x >= 0, "the curve (a 1.5 px rose_bright line) must be drawn in the plot");
        CHECK_MESSAGE(std::abs(peak.x - plotX(hz)) <= 3.0f, "the curve peaks at x " << peak.x << "; the plot puts " << hz
                                                           << " Hz at x " << plotX(hz) << " (log axis, 20 Hz to 2 kHz across "
                                                           "798 to 1162)");
        CHECK_MESSAGE(std::abs(peak.y - plotY(db)) <= 2.5f, "the curve peaks at y " << peak.y << "; the plot puts " << db
                                                           << " dB at y " << plotY(db) << " (-18 dB at the top, -96 at the "
                                                           "bottom)");
    }
    Fixture f;
    f.hold(idleFrame(), 30);
    CHECK_MESSAGE(curvePeak(f.shot()).x < 0, "silence draws no rose_bright curve: every point sits on the floor, below the "
                                             "plot's bottom edge, under the axis");
}

TEST_CASE("P6.27 the spectrum's legends: the stamp, the band, the axis and the level lines") {
    Fixture f;
    f.hold(idleFrame(), 2);
    const auto drawn = f.editor->drawnTexts();
    const auto* stamp = textIn(drawn, kSpectrumGlass, "SPECTRUM");
    REQUIRE_MESSAGE(stamp != nullptr, "the glass is stamped SPECTRUM");
    CHECK(std::abs(stamp->bounds.getX() - 798) <= 2);
    CHECK_MESSAGE(textIn(drawn, kSpectrumGlass, "20 Hz - 2 kHz") != nullptr, "the stamp gives the range, \"20 Hz - 2 kHz\"");
    const auto* band = textIn(drawn, kPlot, "30-150 Hz");
    REQUIRE_MESSAGE(band != nullptr, "the band is labelled \"30-150 Hz\" inside the plot");
    CHECK_MESSAGE(std::abs(band->bounds.getCentreX() - (plotX(30) + plotX(150)) / 2) <= 3.0f, "the band's label is centred on "
                                                                                             "the band");
    for (const auto& [hz, label] : std::vector<std::pair<double, const char*>>{
             {20, "20"}, {30, "30"}, {50, "50"}, {100, "100"}, {150, "150"}, {300, "300"}, {500, "500"}, {1000, "1k"}, {2000, "2k"}}) {
        const auto* t = textIn(drawn, {780, kPlot.getBottom() + 2, 400, 20}, label);
        CHECK_MESSAGE(t != nullptr, "the axis is labelled \"" << label << "\" under the plot");
        if (t != nullptr)
            CHECK_MESSAGE(std::abs(t->bounds.getCentreX() - plotX(hz)) <= 2.0f, "\"" << label << "\" is centred at x "
                                                                               << t->bounds.getCentreX() << "; " << hz
                                                                               << " Hz is at x " << plotX(hz));
    }
    for (const int db : {-36, -60, -84}) {
        const auto* t = textIn(drawn, kPlot, juce::String(db));
        CHECK_MESSAGE(t != nullptr, "the level line at " << db << " dB is labelled inside the plot");
        if (t != nullptr) {
            CHECK(std::abs(t->bounds.getRight() - (kPlot.getRight() - 2)) <= 2);
            CHECK(t->bounds.getBottom() <= plotY(db) + 1.0f);
        }
    }
}

TEST_CASE("P6.28 the share below 150 Hz while sound plays, and the idle hint in Key and Notes modes") {
    {
        Fixture f;
        f.hold(sineFrame(49.0, -12.0), 180);  // 3 s: the share settles (500 ms smoothing)
        const auto drawn = f.editor->drawnTexts();
        const auto* share = textIn(drawn, {790, 365, 200, 30}, "100");
        if (share == nullptr)
            share = textIn(drawn, {790, 365, 200, 30}, "99");
        CHECK_MESSAGE(share != nullptr, "a 49 Hz sine reads 99 or 100 (%) below 150 Hz, top left of the glass");
        CHECK_MESSAGE(textIn(drawn, kSpectrumGlass, "%") != nullptr, "the share reads as a percent");
        CHECK_MESSAGE(textIn(drawn, kSpectrumGlass, "BELOW 150 Hz") != nullptr, "the share says what it is: BELOW 150 Hz");
        CHECK_MESSAGE(textIn(drawn, kSpectrumGlass, "HOLD A KEY TO PLAY THE PATTERN") == nullptr, "no hint while sound plays");
    }
    {
        Fixture f;
        f.hold(idleFrame(), 2);
        const auto drawn = f.editor->drawnTexts();
        CHECK_MESSAGE(textIn(drawn, kSpectrumGlass, "BELOW 150 Hz") == nullptr, "silence shows no share");
        const auto* hint = textIn(drawn, kPlot, "HOLD A KEY TO PLAY THE PATTERN");
        REQUIRE_MESSAGE(hint != nullptr, "a new window in Key mode says what to do: HOLD A KEY TO PLAY THE PATTERN");
        CHECK_MESSAGE(std::abs(hint->bounds.getCentreX() - kPlot.getCentreX()) <= 3, "the hint is centred in the plot");
        f.set("seq_mode", 2.0f);
        f.hold(idleFrame(), 2);
        CHECK_MESSAGE(textIn(f.editor->drawnTexts(), kPlot, "PLAY MIDI NOTES") != nullptr, "in Notes mode the hint is PLAY MIDI "
                                                                                          "NOTES");
        f.set("seq_mode", 1.0f);
        f.hold(idleFrame(), 2);
        const auto free = f.editor->drawnTexts();
        CHECK_MESSAGE(textIn(free, kPlot, "PLAY MIDI NOTES") == nullptr, "Free mode needs no hint");
        CHECK_MESSAGE(textIn(free, kPlot, "HOLD A KEY TO PLAY THE PATTERN") == nullptr, "Free mode needs no hint");
    }
}

TEST_CASE("P6.29 the one number: the sub's pitch in bone, its name, and bone nowhere else") {
    Fixture f;
    f.hold(idleFrame(), 10);
    auto image = f.shot();
    CHECK_MESSAGE(measure(image, {0, 0, 1200, 760}).bone == 0, "before the first note nothing is bone");
    CHECK_MESSAGE(textIn(f.editor->drawnTexts(), kBoneBox, "Hz") == nullptr, "before the first note no number is drawn");

    auto frame = sineFrame(49.0, -18.0);  // the note G2 (98 Hz) playing; the sub at -1 oct is G1, 49 Hz
    f.hold(frame, 10);
    auto drawn = f.editor->drawnTexts();
    const auto* digits = textIn(drawn, kBoneBox, "49.00");
    REQUIRE_MESSAGE(digits != nullptr, "holding G2 with the sub an octave under, the number reads 49.00 (meters().noteHz "
                                       "halved)");
    CHECK_MESSAGE(std::abs(digits->bounds.getRight() - 1134) <= 2, "the digits end at x 1134");
    CHECK_MESSAGE(textIn(drawn, kBoneBox, "Hz") != nullptr, "the unit, Hz, follows the digits");
    const juce::String label = "G1 " + juce::String::charToString(0x00B7) + " THE SUB";
    const auto* name = textIn(drawn, kBoneBox, label);
    REQUIRE_MESSAGE(name != nullptr, "under the number: \"G1 · THE SUB\" (the middle dot is U+00B7, written with "
                                     "juce::String::charToString(0x00B7))");
    CHECK(std::abs(name->bounds.getRight() - 1160) <= 2);
    image = f.shot();
    const auto all = measure(image, {0, 0, 1200, 760});
    CHECK_MESSAGE(all.bone >= 200, "the number is bone; found " << all.bone << " bone-like pixels");
    CHECK_MESSAGE(kBoneBox.contains(all.boneBox), "bone-like pixels span " << str(all.boneBox) << "; only the number is bone, "
                                                  "inside " << str(kBoneBox));

    f.set("sub_octave", 0.0f);
    f.hold(frame, 30);
    drawn = f.editor->drawnTexts();
    CHECK_MESSAGE(textIn(drawn, kBoneBox, "98.00") != nullptr, "with the sub at the note's pitch (0 oct) the number reads 98.00");
    CHECK_MESSAGE(textIn(drawn, kBoneBox, "G2 " + juce::String::charToString(0x00B7) + " THE SUB") != nullptr, "and its name G2");

    f.set("sub_octave", 1.0f);
    f.hold(frame, 30);
    frame.note = 45;
    frame.noteHz = 110.0f;
    f.hold(frame, 1);
    drawn = f.editor->drawnTexts();
    CHECK_MESSAGE(textIn(drawn, kBoneBox, "49.00") == nullptr, "a new note moves the digits");
    CHECK_MESSAGE(textIn(drawn, kBoneBox, "55.00") == nullptr, "a new note moves the digits over 120 ms; they never jump");
    f.hold(frame, 30);
    CHECK_MESSAGE(textIn(f.editor->drawnTexts(), kBoneBox, "55.00") != nullptr, "after half a second the digits read the new "
                                                                                "sub, 55.00 (A1)");
    frame.note = -1;
    frame.noteHz = 0.0f;
    f.hold(frame, 40);
    image = f.shot();
    CHECK_MESSAGE(measure(image, {0, 0, 1200, 760}).bone == 0, "600 ms after the voice falls idle the number is text_ghost, not "
                                                               "bone");
    CHECK_MESSAGE(textIn(f.editor->drawnTexts(), kBoneBox, "55.00") != nullptr, "the last number stays, dimmed, until the next "
                                                                                "note");
}

TEST_CASE("P6.30 the drift arc: 16 bars shaped by the drift, the current pass lit") {
    Fixture f;
    auto frame = sineFrame(98.0, -18.0);
    frame.step = 1;
    frame.count = 8 * 5 + 1;  // pass 5: the default drift moves two of the six sounding steps
    frame.root = 43;
    f.hold(frame, 3);
    const auto image = f.shot();
    juce::Image::BitmapData data(image, juce::Image::BitmapData::readOnly);
    const float bw = (292.0f - 16.0f - 45.0f) / 16.0f;
    const int expected[16] = {0, 0, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 0};
    int brightest = -1;
    for (int j = 0; j < 16; ++j) {
        const int x = juce::roundToInt(892.0f + static_cast<float>(j) * (bw + 3.0f) + bw / 2.0f);
        int height = 0;
        for (int y = 286; y > kArc.getY(); --y) {
            const auto c = data.getPixelColour(x, y);
            if (luma(c) < 0.12)
                break;
            ++height;
        }
        const float want = std::max(1.5f, 20.0f * static_cast<float>(expected[j]) / 3.0f);
        CHECK_MESSAGE(std::abs(static_cast<float>(height) - want) <= 2.5f, "bar " << j << " is " << height << " px; with drift 0.7 "
                                                                         "and six sounding steps pass " << j << " moves "
                                                                         << expected[j] << " steps: " << want << " px");
        if (roseBright(data.getPixelColour(x, 285)))
            brightest = brightest == -1 ? j : -2;
    }
    CHECK_MESSAGE(brightest == 5, "only the current pass, 5 (count 41 of an 8-step pattern), is rose_bright; got " << brightest);
    const auto drawn = f.editor->drawnTexts();
    CHECK_MESSAGE(findText(drawn, "STEPS AWAY FROM HOME") != nullptr, "the arc says what its bars are");
    const auto* pass = findText(drawn, "PASS 6/16");
    REQUIRE_MESSAGE(pass != nullptr, "while the pattern plays the arc reads PASS 6/16 (pass 5, counted from 1)");
    CHECK(std::abs(pass->bounds.getRight() - 1176) <= 2);
    f.hold(idleFrame(), 2);
    CHECK_MESSAGE(findText(f.editor->drawnTexts(), "16 PASSES") != nullptr, "at rest the arc reads 16 PASSES");
}

TEST_CASE("P6.31 the head: the mark, the clock on glass, and LIVE") {
    Fixture f;
    auto playing = playingAt(kPlayingMoment);
    f.hold(playing, 2);
    auto drawn = f.editor->drawnTexts();
    for (const char* text : {"130.0", "BPM", "HOST"})
        CHECK_MESSAGE(textIn(drawn, kClock, text) != nullptr, "the clock shows \"" << text << "\"");
    CHECK_MESSAGE(textIn(drawn, kClock, "001.3.3") != nullptr, "the clock shows the position as bar.beat.sixteenth: ppq "
                                                               << playing.ppq << " is 001.3.3");
    for (const auto& [text, x] : std::vector<std::pair<const char*, int>>{{"UNDERTOW", 24}, {"2", 188}, {"METAKOSMIA", 230}, {"LIVE", 1151}}) {
        const auto* t = textIn(drawn, {0, 0, 1200, 48}, text);
        CHECK_MESSAGE(t != nullptr, "the head shows \"" << text << "\"");
        if (t != nullptr)
            CHECK_MESSAGE(std::abs(t->bounds.getX() - x) <= 2, "\"" << text << "\" starts at x " << t->bounds.getX() << "; the "
                                                               "design puts it at " << x);
    }
    auto image = f.shot();
    CHECK_MESSAGE(brightAround(image, {1140.0f, 24.0f}, 4) >= 10, "LIVE's lamp is lit while the output sounds");
    f.hold(idleFrame(), 2);
    drawn = f.editor->drawnTexts();
    CHECK_MESSAGE(textIn(drawn, kClock, "INT") != nullptr, "without a host the badge reads INT");
    image = f.shot();
    CHECK_MESSAGE(brightAround(image, {1140.0f, 24.0f}, 4) == 0, "LIVE's lamp is dark in silence");
}

TEST_CASE("P6.32 the meter: the RMS fill, the held peak, and the clip lamp") {
    Fixture f;
    f.hold(sineFrame(98.0, -12.0), 10);  // peak -12 dBFS, RMS -15 dBFS
    auto image = f.shot();
    juce::Image::BitmapData data(image, juce::Image::BitmapData::readOnly);
    int tick = -1;
    for (int x = kMeter.getX(); x < kMeter.getRight(); ++x)
        if (roseBright(data.getPixelColour(x, 678)))
            tick = x;
    CHECK_MESSAGE(std::abs(tick - meterX(-12.0)) <= 2.0f, "the peak's tick (2 px of rose_bright) is at x " << tick << "; -12 "
                                                          "dBFS is at x " << meterX(-12.0));
    int fillEnd = -1;
    for (int x = kMeter.getX() + 2; x < kMeter.getRight() - 2; ++x)
        if (std::abs(x - tick) > 3 && luma(data.getPixelColour(x, 676)) >= 0.12)
            fillEnd = x;
    CHECK_MESSAGE(std::abs(fillEnd - meterX(-15.0)) <= 2.5f, "the fill ends at x " << fillEnd << "; RMS -15 dBFS is at x "
                                                             << meterX(-15.0) << " (-48 to 0 dBFS across 870 to 1134)");
    const auto drawn = f.editor->drawnTexts();
    CHECK_MESSAGE(findText(drawn, "PEAK -12.0 dBFS") != nullptr, "the held peak reads PEAK -12.0 dBFS above the meter");
    for (const char* label : {"-48", "-36", "-24", "-12", "-6", "0"})
        CHECK_MESSAGE(textIn(drawn, {860, 686, 290, 18}, label) != nullptr, "the meter's scale shows " << label);
    CHECK_MESSAGE(brightAround(image, {1158.0f, 678.0f}, 4) == 0, "the clip lamp is dark while the output is clean");
    auto clipping = sineFrame(98.0, -0.3);
    clipping.clipping = true;
    f.hold(clipping, 2);
    CHECK_MESSAGE(brightAround(f.shot(), {1158.0f, 678.0f}, 4) >= 10, "the clip lamp lights while the clip meter is on");
}

TEST_CASE("P6.33 the hazard stripes show only while the output clips") {
    Fixture f;
    f.hold(playingAt(kPlayingMoment), 2);
    auto image = f.shot();
    CHECK_MESSAGE(measure(image, kTopBand).bright == 0, "no stripes along the top while the output is clean");
    CHECK_MESSAGE(measure(image, kBottomBand).bright == 0, "no stripes along the bottom while the output is clean");
    auto frame = playingAt(kPlayingMoment);
    frame.clipping = true;
    f.hold(frame, 3);
    image = f.shot();
    const auto top = measure(image, kTopBand), bottom = measure(image, kBottomBand);
    CHECK_MESSAGE(top.bright >= top.pixels * 0.30, "while clipping the top band is striped rose_bright; only "
                                                   << 100.0 * top.bright / top.pixels << "% of it is");
    CHECK_MESSAGE(bottom.bright >= bottom.pixels * 0.40, "while clipping the bottom band is striped rose_bright; only "
                                                         << 100.0 * bottom.bright / bottom.pixels << "% of it is");
    CHECK_MESSAGE(findText(f.editor->drawnTexts(), "CLIP  OUTPUT HELD AT -0.3 dBFS", kTopBand.expanded(0, 2)) != nullptr,
                  "a plate in the top band says what happened: CLIP  OUTPUT HELD AT -0.3 dBFS");
    frame.clipping = false;
    f.hold(frame, 3);
    CHECK_MESSAGE(measure(f.shot(), kTopBand).bright == 0, "the stripes go when the clip meter clears");
}

TEST_CASE("P6.34 the rumble: the ghost kick's lamp flashes on its grid, and the rule breathes with the bed") {
    Fixture f;
    auto frame = idleFrame();
    frame.hostClock = true;
    frame.bpm = 130.0;
    frame.ppq = 0.95;
    f.hold(frame, 1);
    frame.ppq = 1.02;  // crosses a beat: rumble_rate is 1/4
    f.editor->advance(frame, 1.0 / 60.0);
    CHECK_MESSAGE(brightAround(f.shot(), {104.0f, 622.5f}, 3) >= 5, "the lamp after RUMBLE flashes at a ghost kick (each beat "
                                                                    "at 1/4)");
    frame.ppq = 1.30;
    f.hold(frame, 12);  // 200 ms later
    CHECK_MESSAGE(brightAround(f.shot(), {104.0f, 622.5f}, 3) == 0, "the flash lasts 120 ms");
    frame.rumble = 0.0f;
    f.hold(frame, 2);
    const double quiet = meanLuma(measure(f.shot(), {114, 621, 640, 3}));
    frame.rumble = 0.25f;
    f.hold(frame, 2);
    const double loud = meanLuma(measure(f.shot(), {114, 621, 640, 3}));
    CHECK_MESSAGE(loud >= quiet + 0.04, "the rule after RUMBLE brightens with the bed: mean luma " << quiet << " in silence, " << loud
                                        << " at -12 dBFS");
}
