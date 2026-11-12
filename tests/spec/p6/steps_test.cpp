// SPDX-License-Identifier: AGPL-3.0-only
// Phase 6 spec tests: the step cells, the step row lit in time (docs/specs/phase-6-window.md, checks P6.20 to P6.25;
// docs/design/window.md section 4.1).
// spec-preset: plugin
// spec-needs: plugin/ui/Steps.cpp

#include "doctest.h"

#include "spec_window.h"
#include "ui/Steps.h"
#include "undertow/pattern.h"

#include <array>
#include <cmath>
#include <string>
#include <vector>

using namespace spec;

namespace {

juce::MouseEvent clickAt(juce::Component& c, juce::Point<float> p) {
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), p, juce::ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f,
                            0.0f, &c, &c, now, p, now, 1, false);
}

WindowFrame stepFrame(int step, long long count, double hz = 98.0, double db = -10.0) {
    WindowFrame f = sineFrame(hz, db);
    f.step = step;
    f.count = count;
    f.root = 43;
    f.bpm = 130.0;
    f.hostClock = true;
    return f;
}

// The mean luma of a cell's edge, its border and the glow around it: from 4 px outside the cell to 2 px inside it.
double edgeLuma(const juce::Image& image, int step) {
    const auto outer = designCell(step).expanded(4), inner = designCell(step).reduced(2);
    double sum = 0.0;
    int pixels = 0;
    for (int y = outer.getY(); y < outer.getBottom(); ++y)
        for (int x = outer.getX(); x < outer.getRight(); ++x)
            if (!inner.contains(x, y)) {
                sum += luma(image.getPixelAt(x, y));
                ++pixels;
            }
    return sum / pixels;
}

// Pixel columns of a cell's trace area that hold ink.
int tracedColumns(const juce::Image& image, int step) {
    const auto t = designTrace(step);
    int columns = 0;
    for (int x = t.getX(); x < t.getRight(); ++x)
        columns += inkIn(image, {x, t.getY(), 1, t.getHeight()}) > 0 ? 1 : 0;
    return columns;
}

struct GestureCounter : juce::AudioProcessorListener {
    int begins = 0, ends = 0;
    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails&) override {}
    void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*, int) override { ++begins; }
    void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*, int) override { ++ends; }
};

}  // namespace

TEST_CASE("P6.20 the cells are a Steps child over the row; the playing step's cell is the brightest and fades back over "
          "250 ms") {
    Fixture f;
    f.set("seq_drift", 0.0f);  // no drift marks, so only the light draws rose_bright
    for (int s = 0; s < 8; ++s)
        CHECK_MESSAGE(dynamic_cast<Steps*>(f.editor->getComponentAt(designCell(s).getCentre())) != nullptr,
                      "the component under step " << s << "'s cell must be the Steps child (theme::stepsBounds)");
    f.hold(stepFrame(3, 43), 3);
    auto image = f.shot();
    int brightest = -1;
    double best = -1;
    for (int s = 0; s < 8; ++s) {
        const double m = meanLuma(measure(image, designCell(s)));
        if (m > best) {
            best = m;
            brightest = s;
        }
    }
    CHECK_MESSAGE(brightest == 3, "the playing step (3) must be the brightest cell; cell " << brightest << " is");
    CHECK_MESSAGE(measure(image, designCell(3)).bright >= 60, "the playing cell's border and number are rose_bright");
    f.hold(stepFrame(4, 44), 1);
    CHECK_MESSAGE(measure(f.shot(), designCell(3)).bright >= 20, "a cell fades back over 250 ms, it does not go dark at once");
    f.hold(stepFrame(4, 44), 16);
    const auto after = f.shot();
    CHECK_MESSAGE(edgeLuma(after, 3) <= edgeLuma(after, 2) + 0.01, "250 ms after its step moved on, a cell's border and glow "
                  "are as dark as a cell that never lit (the one before it): mean luma " << edgeLuma(after, 3) << " against "
                  << edgeLuma(after, 2));
}

TEST_CASE("P6.21 the playing step's trace sweeps across its cell in one step; a rest stays empty") {
    Fixture f;
    f.hold(stepFrame(1, 41), 1);
    f.hold(stepFrame(2, 42), 1);
    const int early = tracedColumns(f.shot(), 2);
    f.hold(stepFrame(2, 42), 8);  // 150 ms, longer than a 1/16 step at 130 BPM (115 ms)
    const auto image = f.shot();
    const int columns = tracedColumns(image, 2);
    CHECK_MESSAGE(early < 33, "a step's trace sweeps in as it plays; " << early << " of 66 columns were traced at its start");
    CHECK_MESSAGE(columns >= 60, "after one step a 98 Hz sine at -10 dBFS traces " << columns << " of the cell's 66 columns; "
                                 "the trace fills 66 px per step from the newest samples");
    CHECK_MESSAGE(tracedColumns(image, 0) == 0, "step 1 is a rest (velocity 0): its cell has no trace");
    const auto rest = measure(image, designCell(0));
    CHECK_MESSAGE(meanLuma(rest) < meanLuma(measure(image, designCell(2))), "a rest's cell is darker than a sounding one");
    const auto drawn = f.editor->drawnTexts();
    CHECK_MESSAGE(findText(drawn, "REST", designCell(0)) != nullptr, "a rest's cell reads REST");
    for (int s = 0; s < 8; ++s)
        CHECK_MESSAGE(findText(drawn, juce::String(s + 1), designCell(s)) != nullptr, "cell " << s << " shows its number, "
                                                                                       << s + 1);
}

TEST_CASE("P6.22 cells past the length dim") {
    Fixture f;
    f.perform(kPlayingMoment);  // a pass and a bit: every sounding cell holds a trace
    const auto full = f.shot();
    f.set("seq_length", 5.0f);
    f.hold(playingAt(kPlayingMoment), 2);
    const auto shortened = f.shot();
    for (int s = 5; s < 8; ++s) {
        INFO("step " << s + 1);
        const int before = inkIn(full, designCell(s)), after = inkIn(shortened, designCell(s));
        REQUIRE_MESSAGE(before >= 40, "a sounding cell that has played holds its trace and number: " << before << " ink pixels");
        CHECK_MESSAGE(after <= before / 4, "past the length (5) the cell dims to 30%: its ink (luma 0.2 and up) went from "
                                           << before << " to " << after << " pixels");
    }
    CHECK_MESSAGE(inkIn(shortened, designCell(3)) >= inkIn(full, designCell(3)) * 9 / 10, "a step inside the length keeps its "
                                                                                         "look");
}

TEST_CASE("P6.23 the drift's marks: each step moved on the current pass shows how, the rest none") {
    Fixture f;
    f.set("seq_drift", 1.0f);
    std::array<float, undertow::kNumParams> values{};
    for (int i = 0; i < undertow::kNumParams; ++i)
        values[static_cast<std::size_t>(i)] = f.get(undertow::paramSpec(i).id);
    undertow::Pattern pattern;
    pattern.setParams(values.data());
    // How the drift moved step s on the pass that starts at count `passStart`: Pattern::at against home, or "".
    auto expectedMark = [&](long long passStart, int s) {
        const auto home = pattern.home(s);
        const auto played = pattern.at(passStart + s, 43);
        juce::String expected;
        if (home.velocity > 0.0f && played.pitch != home.pitch)
            expected = (played.pitch > home.pitch ? "+" : "") + juce::String(played.pitch - home.pitch);
        else if (home.velocity > 0.0f && played.velocity < home.velocity)
            expected = "GHOST";
        return expected;
    };
    // Plays into step 3 of the pass that starts at `passStart`, checks every cell's mark, and counts the steps moved.
    auto checkPass = [&](long long passStart) {
        f.hold(stepFrame(1, passStart + 1), 1);
        f.hold(stepFrame(2, passStart + 2), 2);
        const auto drawn = f.editor->drawnTexts();
        int marks = 0;
        for (int s = 0; s < 8; ++s) {
            INFO("step " << s + 1 << " on the pass from count " << passStart);
            const auto expected = expectedMark(passStart, s);
            const auto cell = designCell(s);
            bool any = false;
            for (const auto& t : drawn) {
                const auto text = t.text.trim();
                if (cell.contains(t.bounds.getCentre()) && (text == "GHOST" || text.startsWithChar('+') || text.startsWithChar('-'))) {
                    any = true;
                    CHECK_MESSAGE(text == expected, "the cell shows \"" << text << "\"; Pattern::at(" << passStart + s << ", 43) "
                                                    "against home(" << s << ") gives \"" << expected << "\"");
                    CHECK_MESSAGE(std::abs(t.bounds.getRight() - (cell.getX() + 68)) <= 2, "the mark is right-aligned at x "
                                                                                           << cell.getX() + 68);
                }
            }
            if (expected.isNotEmpty()) {
                ++marks;
                CHECK_MESSAGE(any, "the drift moved this step (\"" << expected << "\") but its cell shows no mark");
            }
        }
        return marks;
    };
    const int moved = checkPass(8 * 8);  // pass 8, the arc's middle: three of the six sounding steps move
    CHECK_MESSAGE(moved == 3, "at drift 1 the middle of an arc moves three steps; the pattern moved " << moved);
    // The middle of the first arc in which the drift ghosts a step (halves its velocity) rather than moving its pitch.
    long long ghostPass = -1;
    for (long long arc = 0; arc < 16 && ghostPass < 0; ++arc)
        for (int s = 0; s < 8; ++s)
            if (expectedMark((arc * 16 + 8) * 8, s) == "GHOST")
                ghostPass = (arc * 16 + 8) * 8;
    REQUIRE_MESSAGE(ghostPass >= 0, "at drift 1 one of the first 16 arcs ghosts a step");
    checkPass(ghostPass);
}

TEST_CASE("P6.24 a click on a cell makes the step a rest, and a second click brings its velocity back, each as one gesture") {
    Fixture f;
    GestureCounter gestures;
    f.processor->addListener(&gestures);
    auto* steps = dynamic_cast<Steps*>(f.editor->getComponentAt(designCell(3).getCentre()));
    REQUIRE(steps != nullptr);
    auto click = [&](int s) {
        const auto p = designCell(s).getCentre().toFloat() - steps->getPosition().toFloat();
        steps->mouseUp(clickAt(*steps, p));
    };
    REQUIRE(f.get("step4_vel") == doctest::Approx(0.55f));
    click(3);
    CHECK_MESSAGE(f.get("step4_vel") == doctest::Approx(0.0f), "a click on step 4's cell makes it a rest; its velocity is "
                                                               << f.get("step4_vel"));
    click(3);
    CHECK_MESSAGE(f.get("step4_vel") == doctest::Approx(0.55f), "a second click brings back 0.55; it is " << f.get("step4_vel"));
    click(0);
    CHECK_MESSAGE(f.get("step1_vel") == doctest::Approx(0.8f), "a step that was always a rest comes in at 0.80; it is "
                                                               << f.get("step1_vel"));
    CHECK_MESSAGE(gestures.begins == 3, "each click is one gesture: " << gestures.begins << " began for 3 clicks");
    CHECK_MESSAGE(gestures.ends == 3, "each click is one gesture: " << gestures.ends << " ended for 3 clicks");
    f.processor->removeListener(&gestures);
}

TEST_CASE("P6.25 in Notes mode the cells dim under a stamp that says why") {
    Fixture f;
    f.perform(kPlayingMoment);
    const auto key = f.shot();
    f.set("seq_mode", 2.0f);
    f.hold(idleFrame(), 2);
    const auto notes = f.shot();
    const auto drawn = f.editor->drawnTexts();
    const auto* stamp = findText(drawn, "NOTES MODE: EACH MIDI NOTE IS ONE HIT");
    REQUIRE_MESSAGE(stamp != nullptr, "Notes mode draws the stamp \"NOTES MODE: EACH MIDI NOTE IS ONE HIT\" over the cells");
    CHECK_MESSAGE(std::abs(stamp->bounds.getCentreX() - 432) <= 3, "the stamp is centred over the cells, at x 432");
    CHECK_MESSAGE(stamp->bounds.getY() >= 96, "the stamp sits on the cells' row");
    CHECK_MESSAGE(stamp->bounds.getBottom() <= 152, "the stamp sits on the cells' row");
    const int before = inkIn(key, designCell(6)), after = inkIn(notes, designCell(6));
    REQUIRE(before >= 40);
    CHECK_MESSAGE(after <= before / 4, "the cells dim to 40% in Notes mode: cell 7's ink went from " << before << " to " << after
                                       << " pixels");
}
