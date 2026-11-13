// SPDX-License-Identifier: AGPL-3.0-only
// Phase 7.3 spec tests: the playing column's knobs as designed (docs/specs/phase-7.3-step-row-as-designed.md, checks
// P7.3.04 to P7.3.06; docs/design/window.md sections 3 and 4.1): a lit knob turns rose_bright but never glows, and a
// knob with no arc draws none.
// spec-preset: plugin

#include "doctest.h"

#include "spec_window.h"
#include "ui/Controls.h"

#include <algorithm>
#include <cmath>
#include <string>

using namespace spec;

namespace {

constexpr float kPiF = 3.14159265f;
constexpr float kStart = 1.25f * kPiF;  // 7:30, clockwise from 12 o'clock
constexpr float kEnd = 2.75f * kPiF;    // 4:30
constexpr float kTop = 2.0f * kPiF;     // 12 o'clock

// The luma at a window point of a 2x snapshot.
double lumaAt(const juce::Image& image, float x, float y) {
    const int px = juce::roundToInt(x * 2.0f), py = juce::roundToInt(y * 2.0f);
    if (!image.getBounds().contains(px, py))
        return 0.0;
    return luma(image.getPixelAt(px, py));
}

// A point `radius` from a knob's centre at `angle`, clockwise from 12 o'clock.
juce::Point<float> around(const DesignPlace& p, float radius, float angle) {
    return {p.x + radius * std::sin(angle), p.y - radius * std::cos(angle)};
}

// The track's radius of a knob (design section 3): its dial's half less 2.5 px.
float trackRadius(const DesignPlace& p) { return p.w / 2 - 2.5f; }

// The mean luma of the two rings 2.5 px inside and outside the track, between two angles: where a glow shows and the
// 2.4 px value arc does not reach.
double halo(const juce::Image& image, const DesignPlace& p, float from, float to) {
    const float r = trackRadius(p);
    double sum = 0.0;
    int n = 0;
    for (float a = from; a <= to; a += kPiF / 180.0f)
        for (const float dr : {-2.5f, 2.5f}) {
            const auto q = around(p, r + dr, a);
            sum += lumaAt(image, q.x, q.y);
            ++n;
        }
    return n > 0 ? sum / n : 0.0;
}

// The darkest luma on the track's circle between two angles: the value arc, where it is drawn.
double arcDarkest(const juce::Image& image, const DesignPlace& p, float from, float to) {
    const float r = trackRadius(p);
    double darkest = 1.0;
    for (float a = from; a <= to; a += kPiF / 180.0f) {
        double best = 0.0;
        for (const float dr : {-0.5f, 0.0f, 0.5f}) {
            const auto q = around(p, r + dr, a);
            best = std::max(best, lumaAt(image, q.x, q.y));
        }
        darkest = std::min(darkest, best);
    }
    return darkest;
}

// The brightest luma in a small disc of `radius` px around a window point.
double brightestNear(const juce::Image& image, juce::Point<float> c, float radius) {
    double brightest = 0.0;
    for (float y = c.y - radius; y <= c.y + radius; y += 0.5f)
        for (float x = c.x - radius; x <= c.x + radius; x += 0.5f)
            if ((x - c.x) * (x - c.x) + (y - c.y) * (y - c.y) <= radius * radius)
                brightest = std::max(brightest, lumaAt(image, x, y));
    return brightest;
}

// A frame in which step `step` (0 .. 7) has just started, so its column is lit at full.
WindowFrame litFrame(int step) {
    WindowFrame frame = sineFrame(98.0, -10.0);
    frame.step = step;
    frame.count = 40 + step;
    frame.root = 43;
    frame.hostClock = true;
    return frame;
}

// The angle of a knob's value, from its slider's own proportion.
float valueAngle(Fixture& f, const char* id) {
    auto* slider = dynamic_cast<juce::Slider*>(f.editor->controlFor(id));
    REQUIRE_MESSAGE(slider != nullptr, "controlFor(\"" << id << "\") is not a knob");
    return kStart + static_cast<float>(slider->valueToProportionOfLength(slider->getValue())) * (kEnd - kStart);
}

}  // namespace

TEST_CASE("P7.3.04 a lit knob's arc turns rose_bright with its column, but only a hovered or dragged knob glows") {
    Fixture f;
    f.set("step4_vel", 0.9f);
    f.set("step4_pitch", 7.0f);
    f.hold(idleFrame(), 30);
    const auto unlit = f.shot(2.0f);
    f.hold(litFrame(3), 2);
    const auto lit = f.shot(2.0f);
    const float margin = 12.0f * kPiF / 180.0f;
    for (const char* id : {"step4_vel", "step4_pitch"}) {
        INFO("`" << id << "`, its column lit");
        const auto& p = designPlace(id);
        const float angle = valueAngle(f, id);
        const bool bipolar = std::string(id).find("pitch") != std::string::npos;
        const float from = std::min(bipolar ? kTop : kStart, angle) + margin;
        const float to = std::max(bipolar ? kTop : kStart, angle) - margin;
        REQUIRE(to > from);
        const double before = halo(unlit, p, from, to), after = halo(lit, p, from, to);
        CHECK_MESSAGE(after - before <= 0.02, "lit, the ground 2.5 px either side of the arc rises from luma "
                                                  << before << " to " << after
                                                  << ": a lit knob's arc, pointer and readout turn rose_bright, but the "
                                                     "glow under the arc is for a hovered or dragged knob only");
        const double darkest = arcDarkest(lit, p, from, to);
        CHECK_MESSAGE(darkest >= 0.3, "lit, the value arc still runs from " << (bipolar ? "12 o'clock" : "7:30")
                                                                             << " to the value: its darkest point "
                                                                                "reads luma "
                                                                             << darkest);
        int brightOnArc = 0;
        for (float a = from; a <= to; a += kPiF / 180.0f) {
            const auto q = around(p, trackRadius(p), a);
            const auto c = lit.getPixelAt(juce::roundToInt(q.x * 2.0f), juce::roundToInt(q.y * 2.0f));
            brightOnArc += roseBright(c) ? 1 : 0;
        }
        CHECK_MESSAGE(brightOnArc >= static_cast<int>(0.8f * (to - from) * 180.0f / kPiF),
                      "lit, the value arc turns rose_bright with its column: only " << brightOnArc
                                                                                    << " of its degrees are rose_bright");
    }
    // Hovered, the same knob does glow.
    f.hold(idleFrame(), 30);
    f.editor->hover("step4_vel");
    f.hold(idleFrame(), 12);
    const auto hovered = f.shot(2.0f);
    f.editor->hover("");
    const auto& vel = designPlace("step4_vel");
    const float to = valueAngle(f, "step4_vel") - margin;
    const double rest = halo(unlit, vel, kStart + margin, to), glow = halo(hovered, vel, kStart + margin, to);
    CHECK_MESSAGE(glow - rest >= 0.04, "hovered, a knob's arc glows (strokes of 7 px at 22% and 4.5 px at 35% "
                                       "rose_bright under it): 2.5 px either side of the arc the ground only goes from "
                                           << rest << " to " << glow);
}

TEST_CASE("P7.3.05 a knob at its start draws no arc: a rest's velocity shows the bare track at 7:30, lit, hovered or "
          "neither") {
    Fixture f;  // step 1 is a rest: step1_vel is 0
    REQUIRE(f.get("step1_vel") == 0.0f);
    const auto& p = designPlace("step1_vel");
    const float r = trackRadius(p);
    for (const int state : {0, 1, 2}) {
        INFO(std::string(state == 0 ? "nothing playing" : state == 1 ? "step 1 playing, its column lit" : "hovered"));
        if (state == 2)
            f.editor->hover("step1_vel");
        f.hold(state == 1 ? litFrame(0) : idleFrame(), state == 1 ? 2 : 30);
        const auto image = f.shot(2.0f);
        f.editor->hover("");
        const double atStart = brightestNear(image, around(p, r, kStart), 2.5f);
        const double bare = brightestNear(image, around(p, r, kTop + 0.5f * kPiF), 2.5f);  // the track at 3:00
        CHECK_MESSAGE(atStart <= bare + 0.03, "at 7:30 the track reads luma " << atStart << " against " << bare
                                                  << " where it is bare: a knob at its start has no value arc, so "
                                                     "draw none, not a dot of its round cap (or its glow)");
    }
}

TEST_CASE("P7.3.06 a bipolar knob at its centre, lit or hovered: the 12 o'clock tick stays text_ghost, and nothing "
          "lights there") {
    Fixture f;  // step 4's pitch is 0, its knob's centre
    REQUIRE(f.get("step4_pitch") == 0.0f);
    const auto& p = designPlace("step4_pitch");
    const float r = trackRadius(p);
    f.hold(idleFrame(), 30);
    const auto unlit = f.shot(2.0f);
    f.editor->hover("step4_pitch");
    f.hold(idleFrame(), 12);
    const auto hovered = f.shot(2.0f);
    f.editor->hover("");
    f.hold(litFrame(3), 2);
    const auto lit = f.shot(2.0f);
    for (const float radius : {r, r + 4.5f}) {  // the track, then the tick (r + 3 to r + 6)
        INFO("at 12 o'clock, " << radius << " px from the centre");
        const double before = brightestNear(unlit, around(p, radius, kTop), 2.0f);
        for (const bool isLit : {true, false}) {
            const double after = brightestNear(isLit ? lit : hovered, around(p, radius, kTop), 2.0f);
            CHECK_MESSAGE(after - before <= 0.02, (isLit ? "lit" : "hovered") << ", it brightens from luma " << before
                                                      << " to " << after
                                                      << ": at its centre a bipolar knob has no arc to draw or glow "
                                                         "under, and its tick stays text_ghost");
        }
    }
    const auto top = around(p, r + 1.0f, kTop);
    const juce::Rectangle<int> box(juce::roundToInt(top.x) - 4, juce::roundToInt(top.y) - 8, 8, 12);
    const Region region = measure(lit, box, 2.0f);
    CHECK_MESSAGE(region.bright == 0, region.bright << " rose_bright pixels at the 12 o'clock of a centred bipolar knob "
                                                       "in a lit column; nothing there is lit");
}
