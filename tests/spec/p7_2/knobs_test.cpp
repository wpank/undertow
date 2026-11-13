// SPDX-License-Identifier: AGPL-3.0-only
// Phase 7.2 spec tests: the knobs and switches drawn as designed (docs/specs/phase-7.2-window-as-designed.md, checks
// P7.2.01 to P7.2.04; docs/design/window.md sections 3, 4.1, 5 and 8).
// spec-preset: plugin

#include "doctest.h"

#include "spec_window.h"
#include "ui/Controls.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace spec;

namespace {

constexpr float kStart = 1.25f * 3.14159265f;  // 7:30, clockwise from 12 o'clock
constexpr float kEnd = 2.75f * 3.14159265f;    // 4:30
constexpr float kTwoPi = 2.0f * 3.14159265f;

float degrees(float radians) { return radians * 180.0f / 3.14159265f; }

// The luma at a window point of a snapshot taken at `scale`.
double lumaAt(const juce::Image& image, float x, float y, float scale) {
    const int px = juce::roundToInt(x * scale), py = juce::roundToInt(y * scale);
    if (!image.getBounds().contains(px, py))
        return 0.0;
    return luma(image.getPixelAt(px, py));
}

// A point `radius` from a knob's centre at `angle`, clockwise from 12 o'clock.
juce::Point<float> around(const DesignPlace& p, float radius, float angle) {
    return {p.x + radius * std::sin(angle), p.y - radius * std::cos(angle)};
}

// The darkest and the brightest luma along a ray from the knob's centre, between two radii.
std::pair<double, double> alongRay(const juce::Image& image, const DesignPlace& p, float angle, float from, float to) {
    double lo = 1.0, hi = 0.0;
    for (float radius = from; radius <= to + 1e-3f; radius += 0.5f) {
        const auto q = around(p, radius, angle);
        const double l = lumaAt(image, q.x, q.y, 2.0f);
        lo = std::min(lo, l);
        hi = std::max(hi, l);
    }
    return {lo, hi};
}

// The brightest luma on the track's circle at `angle`, within a pixel either side of its radius.
double onTrack(const juce::Image& image, const DesignPlace& p, float radius, float angle) {
    double hi = 0.0;
    for (const float dr : {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f}) {
        const auto q = around(p, radius + dr, angle);
        hi = std::max(hi, lumaAt(image, q.x, q.y, 2.0f));
    }
    return hi;
}

juce::Slider& sliderFor(Fixture& f, const char* id) {
    auto* slider = dynamic_cast<juce::Slider*>(f.editor->controlFor(id));
    REQUIRE_MESSAGE(slider != nullptr, "controlFor(\"" << id << "\") is not a knob");
    return *slider;
}

// The ink of a knob's dial in a 2x snapshot: luma above the ground, summed over the dial's square.
double dialInk(const juce::Image& image, const DesignPlace& p) {
    double ink = 0.0;
    for (float y = p.y - p.w / 2; y < p.y + p.w / 2; y += 0.5f)
        for (float x = p.x - p.w / 2; x < p.x + p.w / 2; x += 0.5f)
            ink += std::max(0.0, lumaAt(image, x, y, 2.0f) - 0.04);
    return ink;
}

// The mean luma of a small square of window pixels in a 2x snapshot.
double squareLuma(const juce::Image& image, float x, float y, float size) {
    double sum = 0.0;
    int n = 0;
    for (float yy = y; yy < y + size; yy += 0.5f)
        for (float xx = x; xx < x + size; xx += 0.5f) {
            sum += lumaAt(image, xx, yy, 2.0f);
            ++n;
        }
    return sum / n;
}

// A knob drawn dimmed keeps its drawing at about a third, and the ground around it stays the ground.
void checkDimmed(Fixture& f, const char* id, const juce::Image& normal, const juce::Image& dimmed) {
    INFO("`" << id << "` dimmed");
    const auto b = f.boundsOf(id).toFloat();
    for (const auto corner : {b.getTopLeft(), b.getTopRight() - juce::Point<float>(3.0f, 0.0f),
                              b.getBottomLeft() - juce::Point<float>(0.0f, 3.0f), b.getBottomRight() - juce::Point<float>(3.0f, 3.0f)}) {
        const double before = squareLuma(normal, corner.x, corner.y, 3.0f), after = squareLuma(dimmed, corner.x, corner.y, 3.0f);
        CHECK_MESSAGE(std::abs(after - before) <= 0.01, "a dimmed knob is the knob at 32% opacity, never a box: the corner of its "
                      "bounds at " << corner.x << ", " << corner.y << " went from luma " << before << " to " << after);
    }
    const auto& p = designPlace(id);
    const double ratio = dialInk(dimmed, p) / std::max(1e-9, dialInk(normal, p));
    CHECK_MESSAGE(ratio >= 0.15, "a dimmed knob keeps its drawing at 32%: its dial keeps " << 100.0 * ratio << "% of its ink");
    CHECK_MESSAGE(ratio <= 0.5, "a dimmed knob is drawn at 32% opacity: its dial keeps " << 100.0 * ratio << "% of its ink");
}

}  // namespace

TEST_CASE("P7.2.01 a knob's pointer and its value arc both point at its value, clockwise from 12 o'clock; a bipolar "
          "arc grows from 12 o'clock") {
    struct Probe {
        const char* id;
        float value;
    };
    const Probe probes[] = {{"vco1_level", 0.1f},  {"vco1_level", 0.5f},  {"vco1_level", 0.9f},   {"sub_level", 1.0f},
                            {"vco2_tune", -18.0f}, {"vco2_tune", 12.0f},  {"step3_pitch", -6.0f}, {"step3_pitch", 7.0f},
                            {"vcf_cutoff", 130.0f}, {"vcf_cutoff", 1200.0f}, {"master_gain", -12.0f}};
    Fixture f;
    for (const auto& probe : probes) {
        f.set(probe.id, probe.value);
        f.hold(idleFrame(), 2);
        const auto image = f.shot(2.0f);
        auto& slider = sliderFor(f, probe.id);
        const auto& p = designPlace(probe.id);
        const float r = p.w / 2 - 2.5f, rc = 0.7f * r;  // the track's radius and the cap's (design section 3)
        const float proportion = static_cast<float>(slider.valueToProportionOfLength(slider.getValue()));
        const float angle = kStart + proportion * (kEnd - kStart);
        const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        INFO("`" << probe.id << "` at " << probe.value << ": its value sits " << degrees(std::fmod(angle, kTwoPi))
                 << " degrees clockwise from 12 o'clock (the slider's own proportion, " << proportion << ")");

        const auto pointer = alongRay(image, p, angle, 0.35f * rc, 0.75f * rc);
        CHECK_MESSAGE(pointer.first >= 0.25, "the pointer runs from the cap's centre towards the value: along that ray the "
                      "cap's darkest pixel is luma " << pointer.first << ". Every angle is clockwise from 12 o'clock, "
                      "x = cx + r sin(a), y = cy - r cos(a), as the slider's rotary range and Path::addCentredArc count it");
        for (const float off : {angle + 1.5708f, angle - 1.5708f}) {
            const auto away = alongRay(image, p, off, 0.35f * rc, 0.75f * rc);
            CHECK_MESSAGE(away.second < 0.2, "a quarter turn from the value the cap is dark, but at "
                          << degrees(std::fmod(off + kTwoPi, kTwoPi)) << " degrees it reads luma " << away.second
                          << ": is the pointer drawn with (cos a, sin a), a quarter turn off?");
        }

        const float from = bipolar ? kTwoPi : kStart;
        const float lo = std::min(from, angle), hi = std::max(from, angle);
        const float margin = 10.0f * 3.14159265f / 180.0f;
        double darkestInside = 1.0, brightestOutside = 0.0;
        for (float a = kStart; a <= kEnd + 1e-3f; a += 2.0f * 3.14159265f / 180.0f) {
            const double l = onTrack(image, p, r, a);
            if (a > lo + margin && a < hi - margin)
                darkestInside = std::min(darkestInside, l);
            else if (a < lo - margin || a > hi + margin)
                brightestOutside = std::max(brightestOutside, l);
        }
        if (hi - lo > 2.0f * margin)
            CHECK_MESSAGE(darkestInside >= 0.3, "the value arc runs on the track (radius " << r << " px) from "
                          << (bipolar ? "12 o'clock" : "7:30") << " to the value; its darkest point reads luma " << darkestInside);
        CHECK_MESSAGE(brightestOutside <= 0.22, "beyond the value the track is bare (`track`), but it reaches luma "
                      << brightestOutside << (bipolar ? ": a bipolar arc grows from 12 o'clock (2 pi), never from 9 o'clock" : ""));
    }
}

TEST_CASE("P7.2.02 a dimmed knob is the knob at 32% opacity, never a box, and its readout dims with it; readouts are "
          "`value` until their column lights") {
    Fixture f;
    f.hold(idleFrame(), 2);
    const auto normal = f.shot(2.0f);
    auto host = idleFrame();
    host.hostClock = true;
    f.hold(host, 2);
    const auto dimmed = f.shot(2.0f);
    checkDimmed(f, "seq_bpm", normal, dimmed);
    const auto& tempo = designPlace("seq_bpm");
    const auto* hostWord = findText(f.editor->drawnTexts(), "HOST",
                                    {static_cast<int>(tempo.x) - 30, static_cast<int>(tempo.y), 60, 50});
    CHECK_MESSAGE(hostWord != nullptr, "TEMPO under a host's clock reads HOST");
    if (hostWord != nullptr) {
        const auto box = hostWord->bounds.toFloat();
        double brightest = 0.0;
        for (float y = box.getY(); y < box.getBottom(); y += 0.5f)
            for (float x = box.getX(); x < box.getRight(); x += 0.5f)
                brightest = std::max(brightest, lumaAt(dimmed, x, y, 2.0f));
        CHECK_MESSAGE(brightest <= 0.35, "HOST dims with its knob, to 45% of `value`; its brightest pixel reads luma "
                                         << brightest);
    }

    f.hold(idleFrame(), 2);
    const auto full = f.shot(2.0f);
    f.set("seq_length", 5.0f);
    f.hold(idleFrame(), 2);
    const auto shortened = f.shot(2.0f);
    for (const char* id : {"step6_vel", "step7_pitch", "step8_vel"})
        checkDimmed(f, id, full, shortened);
    const auto& vel = designPlace("step6_vel");
    const auto* readout = findText(f.editor->drawnTexts(), "0.25", {static_cast<int>(vel.x) - 30, static_cast<int>(vel.y), 60, 40});
    CHECK_MESSAGE(readout != nullptr, "a column past the length keeps its readouts, at 30% (design section 4.1): step 6's "
                                      "velocity still reads 0.25");
    if (readout != nullptr) {
        const auto box = readout->bounds.toFloat();
        double brightest = 0.0;
        for (float y = box.getY(); y < box.getBottom(); y += 0.5f)
            for (float x = box.getX(); x < box.getRight(); x += 0.5f)
                brightest = std::max(brightest, lumaAt(shortened, x, y, 2.0f));
        CHECK_MESSAGE(brightest <= 0.3, "a dimmed column's readout is at 30%; its brightest pixel reads luma " << brightest);
    }

    f.set("seq_length", 8.0f);
    f.hold(idleFrame(), 2);
    const auto rest = f.shot(2.0f);
    auto readoutBox = [](const char* id) {
        const auto& p = designPlace(id);
        return juce::Rectangle<int>(static_cast<int>(p.x) - 18, static_cast<int>(p.y + p.w / 2) + 5, 36, 12);
    };
    for (int s = 1; s <= 8; ++s)
        for (const char* kind : {"_pitch", "_vel"}) {
            const auto id = "step" + std::to_string(s) + kind;
            CHECK_MESSAGE(measure(rest, readoutBox(id.c_str()), 2.0f).bright == 0, "`" << id << "`'s readout is `value` "
                          "(#988090) at rest, never rose_bright");
        }
    WindowFrame frame = sineFrame(98.0, -10.0);
    frame.step = 3;
    frame.count = 43;
    frame.root = 43;
    frame.hostClock = true;
    f.hold(frame, 2);
    const auto lit = f.shot(2.0f);
    CHECK_MESSAGE(measure(lit, readoutBox("step4_vel"), 2.0f).bright >= 6, "the playing column's readouts turn rose_bright "
                                                                           "with its light");
    CHECK_MESSAGE(measure(lit, readoutBox("step2_vel"), 2.0f).bright == 0, "a column that isn't lit keeps `value`");
}

TEST_CASE("P7.2.03 switches: the chosen segment lit in rose_ember under a rose_bright word, the others unfilled with "
          "text_dim words; the waves drawn as 13 x 7 wave glyphs") {
    Fixture f;
    f.hold(idleFrame(), 2);
    const auto image = f.shot(2.0f);
    // The median luma of a segment's inside (its fill), and the mean of its 24 brightest pixels (its word).
    auto segmentLumas = [&](juce::Rectangle<int> seg) {
        std::vector<double> all;
        for (float y = seg.getY() + 3.0f; y < seg.getBottom() - 3.0f; y += 0.5f)
            for (float x = seg.getX() + 3.0f; x < seg.getRight() - 3.0f; x += 0.5f)
                all.push_back(lumaAt(image, x, y, 2.0f));
        std::sort(all.begin(), all.end());
        double word = 0.0;
        for (std::size_t i = all.size() - 24; i < all.size(); ++i)
            word += all[i] / 24.0;
        return std::pair<double, double>(all[all.size() / 2], word);
    };
    for (const char* id : {"seq_mode", "vca_attack", "sub_octave"}) {
        auto* sw = dynamic_cast<Switch*>(f.editor->controlFor(id));
        REQUIRE_MESSAGE(sw != nullptr, "`" << id << "` is a Switch");
        for (int i = 0; i < sw->segments(); ++i) {
            INFO("`" << id << "` segment " << i << (i == sw->selected() ? " (chosen)" : ""));
            const auto [fill, word] = segmentLumas(sw->segmentBounds(i).translated(sw->getX(), sw->getY()));
            if (i == sw->selected()) {
                CHECK_MESSAGE(fill <= 0.2, "the chosen segment is lit in rose_ember (to rose_deep), never a bright fill: its "
                                           "fill reads luma " << fill);
                CHECK_MESSAGE((word + 0.05) / (fill + 0.05) >= 3.0, "the chosen word, rose_bright, stands out from its fill by "
                              "at least 3:1; it is " << (word + 0.05) / (fill + 0.05) << ":1");
            } else {
                CHECK_MESSAGE(fill <= 0.06, "an unchosen segment is the switch's glass, unfilled: it reads luma " << fill);
                CHECK_MESSAGE(word <= 0.38, "an unchosen word is text_dim; its brightest pixels read luma " << word);
            }
        }
    }
    for (const char* id : {"vco1_wave", "vco2_wave"}) {
        auto* sw = dynamic_cast<Switch*>(f.editor->controlFor(id));
        REQUIRE_MESSAGE(sw != nullptr, "`" << id << "` is a Switch");
        for (int i = 0; i < sw->segments(); ++i) {
            INFO("`" << id << "` segment " << i << (i == sw->selected() ? " (chosen)" : ""));
            const auto seg = sw->segmentBounds(i).translated(sw->getX(), sw->getY());
            float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
            for (float y = seg.getY() + 3.0f; y < seg.getBottom() - 3.0f; y += 0.5f)
                for (float x = seg.getX() + 3.0f; x < seg.getRight() - 3.0f; x += 0.5f) {
                    const auto c = image.getPixelAt(juce::roundToInt(x * 2.0f), juce::roundToInt(y * 2.0f));
                    const bool glyph = i == sw->selected() ? roseBright(c) : luma(c) >= 0.2;
                    if (glyph) {
                        x0 = std::min(x0, x), y0 = std::min(y0, y), x1 = std::max(x1, x), y1 = std::max(y1, y);
                    }
                }
            const float w = x1 - x0 + 0.5f, h = y1 - y0 + 0.5f;
            CHECK_MESSAGE(x1 >= x0, "the segment draws its wave glyph");
            CHECK_MESSAGE((w >= 10.0f && w <= 15.0f && h >= 5.0f && h <= 9.0f), "a wave glyph is a 13 x 7 px triangle or "
                          "square wave at 1.4 px; this one's ink spans " << w << " x " << h << " px");
        }
    }
}

TEST_CASE("P7.2.04 detent dots: one per position on RATE, LENGTH and the rumble's RATE, the current one lit; none on "
          "a knob of more positions") {
    Fixture f;
    f.hold(idleFrame(), 2);
    const auto image = f.shot(2.0f);
    auto dotRuns = [&](const DesignPlace& p, bool skipTop) {
        const float radius = p.w / 2 + 2.1f;  // the track's radius plus 4.6 px
        int runs = 0;
        bool inside = false;
        for (float a = kStart - 0.09f; a <= kEnd + 0.09f; a += 0.25f * 3.14159265f / 180.0f) {
            if (skipTop && std::abs(a - kTwoPi) < 0.21f) {
                inside = false;
                continue;
            }
            const auto q = around(p, radius, a);
            const bool ink = lumaAt(image, q.x, q.y, 2.0f) >= 0.1;  // text_ghost dots; the ground's noise stays under 0.08
            runs += ink && !inside ? 1 : 0;
            inside = ink;
        }
        return runs;
    };
    for (const auto& [id, positions] : std::vector<std::pair<const char*, int>>{{"seq_rate", 8}, {"seq_length", 8}, {"rumble_rate", 4}}) {
        const auto& p = designPlace(id);
        CHECK_MESSAGE(dotRuns(p, false) == positions, "`" << id << "` has " << positions << " positions: as many detent dots "
                      "on a circle 4.6 px outside its track (radius " << p.w / 2 + 2.1f << " px); found " << dotRuns(p, false));
        auto& slider = sliderFor(f, id);
        const float a = kStart + static_cast<float>(slider.valueToProportionOfLength(slider.getValue())) * (kEnd - kStart);
        const auto q = around(p, p.w / 2 + 2.1f, a);
        int bright = 0;
        for (float dy = -1.5f; dy <= 1.5f; dy += 0.5f)
            for (float dx = -1.5f; dx <= 1.5f; dx += 0.5f)
                bright += roseBright(image.getPixelAt(juce::roundToInt((q.x + dx) * 2.0f), juce::roundToInt((q.y + dy) * 2.0f))) ? 1 : 0;
        CHECK_MESSAGE(bright >= 2, "`" << id << "`'s current position's dot is lit rose_bright");
    }
    for (const char* id : {"seq_root", "step1_pitch", "step4_pitch", "step8_pitch"})
        CHECK_MESSAGE(dotRuns(designPlace(id), true) == 0, "`" << id << "` has more than 8 positions: no detent dots or ticks "
                      "around it (a step pitch has only its 12 o'clock tick); found " << dotRuns(designPlace(id), true));
}
