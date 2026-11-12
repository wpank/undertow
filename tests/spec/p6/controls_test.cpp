// SPDX-License-Identifier: AGPL-3.0-only
// Phase 6 spec tests: one control per parameter at the design's place, its readout, its gestures, and the printed
// faceplate (docs/specs/phase-6-window.md, checks P6.13 to P6.19; docs/design/window.md sections 2, 3 and 8).
// spec-preset: plugin
// spec-needs: plugin/ui/Controls.cpp

#include "doctest.h"

#include "spec_window.h"
#include "ui/Controls.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using namespace spec;

namespace {

bool isSwitch(const char* id) {
    for (const char* s : {"vco1_wave", "vco2_wave", "sub_octave", "vca_attack", "seq_mode"})
        if (std::strcmp(s, id) == 0)
            return true;
    return false;
}

// The pixel row a line of capitals stands on: the lowest row of its box with at least 30% of the ink of its inkiest row.
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

juce::MouseEvent clickAt(juce::Component& c, juce::Point<float> p) {
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), p, juce::ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f,
                            0.0f, &c, &c, now, p, now, 1, false);
}

// The design's controls that exist as rectangles in the window: every control, and the displays they must not cover.
std::vector<juce::Rectangle<int>> displays() {
    std::vector<juce::Rectangle<int>> d{kSpectrumGlass, kArc, kMeter, kClock, {0, 0, 1200, 48}};
    for (int s = 0; s < 8; ++s)
        d.push_back(designCell(s));
    return d;
}

}  // namespace

TEST_CASE("P6.13 every parameter has one control, named by its id and titled by its name") {
    Fixture f;
    std::set<juce::Component*> seen;
    for (int i = 0; i < undertow::kNumParams; ++i) {
        const auto& spec = undertow::paramSpec(i);
        INFO("`" << spec.id << "`");
        auto* c = f.editor->controlFor(spec.id);
        REQUIRE_MESSAGE(c != nullptr, "controlFor must find its control");
        CHECK_MESSAGE(c->isVisible(), "the control must be visible");
        CHECK_MESSAGE(c->getParentComponent() == f.editor.get(), "the control must be a child of the window");
        CHECK_MESSAGE(c->getName() == juce::String(spec.id), "the control's name is \"" << c->getName() << "\"; it is the id");
        CHECK_MESSAGE(c->getTitle() == juce::String(spec.name), "the control's title is \"" << c->getTitle() << "\"; it is the "
                                                                "parameter's name, \"" << spec.name << "\"");
        CHECK_MESSAGE(seen.insert(c).second, "the control is shared with another parameter");
    }
}

TEST_CASE("P6.14 each control is the design's kind, at the design's place, inside the window and clear of the rest") {
    Fixture f;
    std::vector<std::pair<std::string, juce::Rectangle<int>>> placed;
    for (const auto& p : designPlaces()) {
        INFO("`" << p.id << "`");
        auto* c = f.editor->controlFor(p.id);
        REQUIRE(c != nullptr);
        const auto b = f.boundsOf(p.id);
        if (isSwitch(p.id)) {
            auto* sw = dynamic_cast<Switch*>(c);
            REQUIRE_MESSAGE(sw != nullptr, "a Choice parameter with " << undertow::paramSpec(undertow::findParam(p.id)).numChoices
                                           << " choices is a Switch (plugin/ui/Controls.h)");
            CHECK_MESSAGE(sw->segments() == undertow::paramSpec(undertow::findParam(p.id)).numChoices, "one segment per choice");
            const juce::Rectangle<int> expected(juce::roundToInt(p.x), juce::roundToInt(p.y), juce::roundToInt(p.w), juce::roundToInt(p.h));
            CHECK_MESSAGE(b == expected, "the switch is at " << str(b) << "; the design puts it at " << str(expected));
        } else {
            auto* knob = dynamic_cast<juce::Slider*>(c);
            REQUIRE_MESSAGE(knob != nullptr, "every other control is a knob, a juce::Slider");
            CHECK_MESSAGE(knob->isRotary(), "a knob is a rotary slider");
            CHECK_MESSAGE(std::abs(b.getWidth() - (p.w + 12)) <= 1, "the knob's bounds are " << b.getWidth() << " wide; a " << p.w
                                                                    << " px dial's component is the dial plus 12 px");
            CHECK_MESSAGE(std::abs(b.getHeight() - b.getWidth()) <= 1, "a knob's bounds are square");
            CHECK_MESSAGE(std::abs(b.toFloat().getCentreX() - p.x) <= 1.0f, "the knob's centre x is " << b.toFloat().getCentreX()
                                                                            << "; the design puts it at " << p.x);
            CHECK_MESSAGE(std::abs(b.toFloat().getCentreY() - p.y) <= 1.0f, "the knob's centre y is " << b.toFloat().getCentreY()
                                                                            << "; the design puts it at " << p.y);
        }
        CHECK_MESSAGE(juce::Rectangle<int>(0, 0, 1200, 760).contains(b), "the control " << str(b) << " leaves the window");
        for (const auto& d : displays())
            CHECK_MESSAGE(!b.intersects(d), "the control " << str(b) << " covers the display at " << str(d));
        for (const auto& [other, ob] : placed)
            CHECK_MESSAGE(!b.intersects(ob), "the control " << str(b) << " overlaps `" << other << "` at " << str(ob));
        placed.emplace_back(p.id, b);
    }
}

TEST_CASE("P6.15 every control follows its parameter both ways, and a switch's click chooses its segment") {
    Fixture f;
    for (int i = 0; i < undertow::kNumParams; ++i) {
        const auto& spec = undertow::paramSpec(i);
        INFO("`" << spec.id << "`");
        auto* c = f.editor->controlFor(spec.id);
        REQUIRE(c != nullptr);
        const float a = spec.min + (spec.max - spec.min) * 0.75f;
        const float target = spec.kind == undertow::ParamKind::Float ? a : std::round(a);
        if (auto* sw = dynamic_cast<Switch*>(c)) {
            for (int k = 0; k < sw->segments(); ++k) {
                const auto seg = sw->segmentBounds(k).toFloat();
                sw->mouseUp(clickAt(*sw, seg.getCentre()));
                CHECK_MESSAGE(f.get(spec.id) == doctest::Approx(static_cast<float>(k)), "a click on segment " << k << " must set "
                                                                                        "the parameter to " << k);
                CHECK_MESSAGE(sw->selected() == k, "the switch must show segment " << k << " chosen");
            }
            f.set(spec.id, 0.0f);
            CHECK_MESSAGE(sw->selected() == 0, "when the host sets the parameter, the switch follows");
        } else if (auto* knob = dynamic_cast<juce::Slider*>(c)) {
            f.set(spec.id, target);
            CHECK_MESSAGE(knob->getValue() == doctest::Approx(target).epsilon(1e-3), "the host set " << target << "; the knob "
                                                                                     "shows " << knob->getValue());
            knob->setValue(spec.def, juce::sendNotificationSync);
            CHECK_MESSAGE(f.get(spec.id) == doctest::Approx(spec.def).epsilon(1e-3), "turning the knob to " << spec.def << " sets "
                                                                                     "the parameter; it reads " << f.get(spec.id));
        }
    }
}

TEST_CASE("P6.16 every readout reads as the design writes it") {
    Fixture f;
    const std::vector<std::tuple<const char*, double, const char*>> cases{
        {"step3_pitch", 0, "0"},       {"step3_pitch", 7, "+7"},      {"step3_pitch", -5, "-5"},     {"step8_pitch", 12, "+12"},
        {"step2_vel", 0, "REST"},      {"step2_vel", 0.22, "0.22"},   {"step2_vel", 1, "1.00"},      {"seq_root", 43, "G2"},
        {"seq_root", 49, "C#3"},       {"seq_root", 24, "C1"},        {"seq_rate", 4, "1/16"},       {"seq_rate", 6, "1/8T"},
        {"seq_rate", 0, "1 bar"},      {"rumble_rate", 2, "1 bar"},   {"seq_length", 5, "5"},        {"seq_bpm", 130, "130.0"},
        {"seq_bpm", 92.5, "92.5"},     {"vco1_level", 0.45, "0.45"},  {"noise_level", 0.05, "0.05"}, {"seq_drift", 0.7, "0.70"},
        {"vco2_tune", -12, "-12 st"},  {"vco2_tune", 7, "+7 st"},     {"vco2_tune", 0, "0 st"},      {"vco2_tune", 0.5, "+0.5 st"},
        {"pitch_env", 7, "7 st"},      {"pitch_decay", 75, "75 ms"},  {"pitch_decay", 5, "5 ms"},    {"vcf_decay", 275, "275 ms"},
        {"vcf_decay", 1500, "1.5 s"},  {"vca_decay", 20000, "20 s"},  {"rumble_decay", 4.5, "4.5 s"}, {"rumble_decay", 0.2, "0.2 s"},
        {"vcf_cutoff", 140, "140 Hz"}, {"vcf_cutoff", 1200, "1.2 kHz"}, {"vcf_cutoff", 8000, "8 kHz"}, {"rumble_tone", 100, "100 Hz"},
        {"vcf_env", 2, "2 oct"},       {"vcf_env", 0.5, "0.5 oct"},   {"master_gain", -12, "-12.0 dB"}, {"master_gain", 0, "0.0 dB"},
        {"master_gain", 3.5, "+3.5 dB"}, {"vcf_res", 0.32, "0.32"},   {"rumble_duck", 0.88, "0.88"},
    };
    for (const auto& [id, value, expected] : cases) {
        auto* knob = dynamic_cast<juce::Slider*>(f.editor->controlFor(id));
        REQUIRE_MESSAGE(knob != nullptr, "`" << id << "` is a knob");
        const auto text = knob->getTextFromValue(value).toStdString();
        CHECK_MESSAGE(text == expected, "`" << id << "` at " << value << " reads \"" << text << "\"; the design writes \""
                                            << expected << "\" (docs/design/window.md section 3, Readouts)");
    }
}

TEST_CASE("P6.17 the knobs' gestures: 270 degrees, 250 px of drag, double-click to the default, whole steps where stepped") {
    Fixture f;
    for (int i = 0; i < undertow::kNumParams; ++i) {
        const auto& spec = undertow::paramSpec(i);
        auto* knob = dynamic_cast<juce::Slider*>(f.editor->controlFor(spec.id));
        if (knob == nullptr)
            continue;
        INFO("`" << spec.id << "`");
        const auto rotary = knob->getRotaryParameters();
        CHECK_MESSAGE(rotary.startAngleRadians == doctest::Approx(1.25 * kWindowPi), "the arc starts at 7:30 (1.25 pi)");
        CHECK_MESSAGE(rotary.endAngleRadians == doctest::Approx(2.75 * kWindowPi), "the arc ends at 4:30 (2.75 pi)");
        CHECK_MESSAGE(rotary.stopAtEnd, "a knob stops at its ends");
        CHECK_MESSAGE(knob->getSliderStyle() == juce::Slider::RotaryHorizontalVerticalDrag, "drag up or right to raise it");
        CHECK_MESSAGE(knob->getMouseDragSensitivity() == 250, "250 px of drag covers the range; it is "
                                                              << knob->getMouseDragSensitivity());
        CHECK_MESSAGE(knob->getTextBoxPosition() == juce::Slider::NoTextBox, "no text box: the window draws the value");
        CHECK_MESSAGE(knob->isDoubleClickReturnEnabled(), "a double-click returns the knob to its default");
        CHECK_MESSAGE(knob->getDoubleClickReturnValue() == doctest::Approx(spec.def).epsilon(1e-4),
                      "a double-click returns to " << knob->getDoubleClickReturnValue() << "; the default is " << spec.def);
        if (spec.kind != undertow::ParamKind::Float)
            CHECK_MESSAGE(knob->getInterval() == doctest::Approx(1.0), "a stepped knob moves in whole steps");
    }
}

TEST_CASE("P6.18 the faceplate's words: section legends, row labels, strip heads and each control's legend, where the "
          "design puts them") {
    Fixture f;
    f.hold(idleFrame(), 2);
    const auto image = f.shot();
    const auto drawn = f.editor->drawnTexts();
    auto expectAt = [&](const char* text, float x, float baseline, const char* align) {
        INFO("\"" << text << "\" at x " << x << ", baseline " << baseline << " (" << align << ")");
        const DrawnText* found = nullptr;
        for (const auto& t : drawn) {
            if (t.text.trim() != juce::String(text))
                continue;
            const auto b = t.bounds.toFloat();
            const float ax = std::string(align) == "left" ? b.getX() : (std::string(align) == "right" ? b.getRight() : b.getCentreX());
            if (std::abs(ax - x) <= 2.5f && b.getY() <= baseline - 3.0f && b.getBottom() >= baseline - 1.0f)
                found = &t;
        }
        CHECK_MESSAGE(found != nullptr, "the window must draw it there (drawnTexts records every word with its box)");
        if (found != nullptr) {
            CHECK_MESSAGE(inkIn(image, found->bounds) >= 3, "it is recorded at " << str(found->bounds) << " but nothing is drawn");
            const int row = standsOn(image, found->bounds);
            CHECK_MESSAGE(std::abs(static_cast<float>(row) - (baseline - 0.5f)) <= 1.5f, "its letters stand on pixel row " << row
                          << "; on the design's baseline they stand on row " << baseline - 0.5f << ", within 1.5 px");
        }
    };
    for (const auto& [text, x, baseline] : std::vector<std::tuple<const char*, float, float>>{
             {"SEQUENCER", 24, 82}, {"VCO", 24, 350}, {"VCF", 400, 350}, {"VCA", 656, 350}, {"RUMBLE", 24, 626}, {"OUTPUT", 784, 626}})
        expectAt(text, x, baseline, "left");
    for (const auto& [text, baseline] : std::vector<std::pair<const char*, float>>{{"STEP", 127}, {"PITCH", 203}, {"VELOCITY", 279}})
        expectAt(text, 24, baseline, "left");
    for (const auto& [text, x] : std::vector<std::pair<const char*, float>>{
             {"VCO 1", 56}, {"VCO 2", 120}, {"NOISE", 184}, {"SUB", 248}, {"PITCH", 336}, {"ENV", 596}})
        expectAt(text, x, 372, "centre");
    expectAt("MODE", 784, 127.5f, "left");
    expectAt("ATTACK", 706, 463.5f, "centre");
    for (const auto& p : designPlaces())
        if (p.knob && p.legend[0] != '\0')
            expectAt(p.legend, p.x, p.y + p.w / 2 + 13.5f, "centre");
}

TEST_CASE("P6.19 values: the top tier's always, the others on hover; TEMPO reads HOST under a host's clock") {
    Fixture f;
    f.hold(idleFrame(), 2);
    auto drawn = f.editor->drawnTexts();
    auto valueUnder = [&](const char* id) -> juce::String {
        const auto& p = designPlace(id);
        const float baseline = p.y + p.w / 2 + (p.legend[0] != '\0' ? 26.5f : 14.0f);
        for (const auto& t : drawn) {
            const auto b = t.bounds.toFloat();
            if (std::abs(b.getCentreX() - p.x) <= 3.0f && b.getY() <= baseline - 3.0f && b.getBottom() >= baseline - 1.0f)
                return t.text.trim();
        }
        return {};
    };
    for (const auto& [id, text] : std::vector<std::pair<const char*, const char*>>{
             {"step1_pitch", "0"}, {"step1_vel", "REST"}, {"step2_vel", "0.22"}, {"step3_vel", "1.00"}, {"seq_root", "G2"},
             {"seq_rate", "1/16"}, {"seq_length", "8"}, {"seq_bpm", "130.0"}, {"seq_drift", "0.70"}})
        CHECK_MESSAGE(valueUnder(id) == juce::String(text), "`" << id << "` shows \"" << valueUnder(id) << "\" at rest; the top "
                                                            "tier always shows its value: \"" << text << "\"");
    for (const char* id : {"vcf_cutoff", "vco1_level", "pitch_decay", "rumble_level", "master_gain", "vca_decay"})
        CHECK_MESSAGE(valueUnder(id).isEmpty(), "`" << id << "` shows \"" << valueUnder(id) << "\" at rest; the voice, rumble "
                                                "and output values show only on hover or drag");
    f.editor->hover("vcf_cutoff");
    drawn = f.editor->drawnTexts();
    CHECK_MESSAGE(valueUnder("vcf_cutoff") == "140 Hz", "hovered, the cutoff shows its value under its legend; it shows \""
                                                        << valueUnder("vcf_cutoff") << "\"");
    const auto image = f.shot();
    const auto knob = measure(image, f.boundsOf("vcf_cutoff"));
    CHECK_MESSAGE(knob.bright >= 30, "a hovered knob's arc glows rose_bright; found " << knob.bright << " rose_bright pixels");
    f.editor->hover("");
    auto host = idleFrame();
    host.hostClock = true;
    f.hold(host, 2);
    drawn = f.editor->drawnTexts();
    CHECK_MESSAGE(valueUnder("seq_bpm") == "HOST", "under a host's clock TEMPO reads HOST; it reads \"" << valueUnder("seq_bpm") << "\"");
}
