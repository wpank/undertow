// Phase 4.2 spec tests: the editor laid out as the design has it: panels, knobs, readouts, labels and stamps
// (docs/specs/phase-4.2-layout.md, P4.2.01 to P4.2.05; docs/design/ui-spec.md and undertow-mock.svg).
// spec-preset: plugin
// spec-needs: plugin/ui/Texts.h

#include "doctest.h"

#include "spec_ui.h"
#include "undertow/params.h"

#include <cmath>
#include <regex>
#include <string>
#include <vector>

using namespace spec;

namespace {

const juce::Rectangle<int> kWindow{0, 0, 1200, 760}, kChest{312, 56, 576, 464}, kGut{16, 528, 1168, 136},
    kGround{0, 672, 1200, 88}, kPump{912, 80, 264, 96};

struct DesignControl {
    const char* id;
    const char* label;
};

// The design's panels (docs/design/undertow-mock.svg): the area, the stamp and each control with its label. The
// GROUND row holds three groups, left to right: VOICE/07, OUT/08 and the sequencer's controls.
struct DesignPanel {
    const char* stamp;
    juce::Rectangle<int> area;
    std::vector<DesignControl> controls;
};

const std::vector<DesignPanel>& designPanels() {
    static const std::vector<DesignPanel> panels{
        {"OSC/01",
         {16, 56, 280, 160},
         {{"osc_wave", "WAVE"}, {"osc_pw", "PW"}, {"osc_level", "LEVEL"}, {"osc_octave", "OCT"}, {"osc_fine", "FINE"},
          {"sub_level", "SUB"}, {"sub_octave", "SUB OCT"}}},
        {"FILTER/02",
         {16, 216, 280, 144},
         {{"filter_cutoff", "CUTOFF"}, {"filter_res", "RES"}, {"filter_drive", "DRIVE"}, {"filter_env", "ENV"},
          {"filter_track", "TRACK"}}},
        {"ENV/03",
         {16, 360, 280, 160},
         {{"amp_attack", "A"}, {"amp_decay", "D"}, {"amp_sustain", "S"}, {"amp_release", "R"}, {"filt_attack", "F A"},
          {"filt_decay", "F D"}, {"filt_sustain", "F S"}, {"filt_release", "F R"}}},
        {"RUMBLE/04",
         {904, 56, 280, 264},
         {{"rumble_level", "LEVEL"}, {"rumble_send", "SEND"}, {"rumble_kick", "KICK"}, {"rumble_rate", "RATE"},
          {"rumble_decay", "DECAY"}, {"rumble_tone", "TONE"}, {"rumble_drive", "DRIVE"}, {"rumble_duck", "DUCK"}}},
        {"MOD/05",
         {904, 320, 280, 200},
         {{"lfo1_rate", "1 RATE"}, {"lfo1_shape", "1 SHAPE"}, {"lfo1_cutoff", "1 CUT"}, {"lfo1_phase", "1 PHASE"},
          {"lfo2_rate", "2 RATE"}, {"lfo2_shape", "2 SHAPE"}, {"lfo2_dest", "2 DEST"}, {"lfo2_depth", "2 DEPTH"},
          {"lfo2_phase", "2 PHASE"}}},
        {"VOICE/07", kGround, {{"glide_ms", "GLIDE"}, {"glide_mode", "MODE"}, {"accent", "ACCENT"}, {"bend_range", "BEND"}}},
        {"OUT/08", kGround, {{"sat_drive", "SAT"}, {"width", "WIDTH"}, {"mono_freq", "MONO"}, {"master_gain", "MASTER"}}},
        {"SEQ/06",
         kGround,
         {{"seq_on", "SEQ"}, {"seq_sync", "SYNC"}, {"seq_bpm", "BPM"}, {"seq_rate", "RATE"}, {"seq_length", "LEN"},
          {"seq_gate", "GATE"}, {"seq_swing", "SWING"}, {"seq_root", "ROOT"}, {"seq_scale", "SCALE"},
          {"seq_mutate", "MUTATE"}, {"seq_evolve", "EVOLVE"}, {"seq_seed", "SEED"}}},
    };
    return panels;
}

juce::Rectangle<int> boundsOf(UndertowEditor& editor, juce::Component& control) {
    return editor.getLocalArea(&control, control.getLocalBounds());
}

std::string text(const juce::Rectangle<int>& r) { return r.toString().toStdString(); }

// Pixels of a drawn word: brighter than the panels, the noise floor and the hairlines (luminance 0.2 and up), so a
// recorded text that was never drawn is caught.
int inkIn(const juce::Image& image, juce::Rectangle<int> box) {
    juce::Image::BitmapData data(image, juce::Image::BitmapData::readOnly);
    int ink = 0;
    for (int y = std::max(0, box.getY()); y < std::min(image.getHeight(), box.getBottom()); ++y)
        for (int x = std::max(0, box.getX()); x < std::min(image.getWidth(), box.getRight()); ++x)
            ink += luminance(data.getPixelColour(x, y)) >= 0.2 ? 1 : 0;
    return ink;
}

juce::String trimmed(const juce::String& s) { return s.trim(); }

} // namespace

TEST_CASE("P4.2.01 every control sits in its own panel, clear of the CHEST, the pump field, the GUT and each other") {
    Fixture f;
    std::vector<std::pair<std::string, juce::Rectangle<int>>> placed;
    for (const auto& panel : designPanels()) {
        for (const auto& c : panel.controls) {
            INFO("`" << std::string(c.id) << "` (" << std::string(panel.stamp) << ")");
            auto* control = f.editor->controlFor(c.id);
            REQUIRE_MESSAGE(control != nullptr, "controlFor must find the control");
            const auto b = boundsOf(*f.editor, *control);
            CHECK_MESSAGE(control->isVisible(), "the control must be visible");
            CHECK_MESSAGE(panel.area.contains(b), "its bounds " << text(b) << " must lie inside its panel " << text(panel.area));
            CHECK_MESSAGE(kWindow.contains(b), "its bounds " << text(b) << " must lie inside the window");
            CHECK_MESSAGE(!b.intersects(kChest), "its bounds " << text(b) << " overlap the CHEST " << text(kChest));
            CHECK_MESSAGE(!b.intersects(kPump), "its bounds " << text(b) << " overlap the pump field " << text(kPump));
            CHECK_MESSAGE(!b.intersects(kGut), "its bounds " << text(b) << " overlap the GUT (the unit array) " << text(kGut));
            for (const auto& other : placed) {
                const std::string otherId = other.first;
                const auto otherBounds = other.second;
                CHECK_MESSAGE(!b.intersects(otherBounds), "its bounds " << text(b) << " overlap `" << otherId << "`'s " << text(otherBounds));
            }
            placed.emplace_back(c.id, b);
        }
    }
    // GROUND: VOICE/07, then OUT/08, then the sequencer's controls, left to right.
    int lastRight = 0;
    for (const char* group : {"VOICE/07", "OUT/08", "SEQ/06"}) {
        for (const auto& panel : designPanels()) {
            if (std::string(panel.stamp) != group)
                continue;
            int left = 1 << 30, right = 0;
            for (const auto& c : panel.controls) {
                if (auto* control = f.editor->controlFor(c.id)) {
                    const auto b = boundsOf(*f.editor, *control);
                    left = std::min(left, b.getX());
                    right = std::max(right, b.getRight());
                }
            }
            CHECK_MESSAGE(left >= lastRight, "in GROUND, " << std::string(group) << "'s controls must come after the group before it (they start at x "
                                                           << left << ", the group before ends at " << lastRight << ")");
            lastRight = right;
        }
    }
}

TEST_CASE("P4.2.02 knobs are 40 px dials in 64 x 64 cells, and seq_on is a lamp button") {
    Fixture f;
    for (const auto& panel : designPanels()) {
        for (const auto& c : panel.controls) {
            INFO("`" << std::string(c.id) << "`");
            auto* control = f.editor->controlFor(c.id);
            REQUIRE(control != nullptr);
            const auto b = boundsOf(*f.editor, *control);
            if (std::string(c.id) == "seq_on") {
                CHECK_MESSAGE(dynamic_cast<juce::Button*>(control) != nullptr, "seq_on is a lamp button");
                CHECK_MESSAGE((b.getWidth() <= 64 && b.getHeight() <= 40), "seq_on's button is " << text(b) << "; it fits a 64 x 40 cell");
                continue;
            }
            auto* slider = dynamic_cast<juce::Slider*>(control);
            REQUIRE_MESSAGE(slider != nullptr, "every other control is a knob (a juce::Slider)");
            CHECK_MESSAGE(slider->isRotary(), "a knob is a rotary slider");
            CHECK_MESSAGE((b.getWidth() >= 32 && b.getWidth() <= 64 && b.getHeight() >= 32 && b.getHeight() <= 64),
                          "a knob's bounds are " << b.getWidth() << " x " << b.getHeight() << "; a 40 px dial in a 64 x 64 cell "
                                                                                             "needs 32 to 64 px each way");
            CHECK_MESSAGE(std::abs(b.getWidth() - b.getHeight()) <= 4,
                          "a knob's bounds are " << b.getWidth() << " x " << b.getHeight() << ": a dial is round, so square within 4 px");
        }
    }
}

TEST_CASE("P4.2.03 no knob shows its value at rest, and its readout is formatted: a choice's name, two decimals at most, the unit") {
    Fixture f;
    for (int i = 0; i < undertow::kNumParams; ++i) {
        const auto& spec = undertow::paramSpec(i);
        if (spec.kind == undertow::ParamKind::Bool)
            continue;
        INFO("`" << std::string(spec.id) << "`");
        auto* slider = dynamic_cast<juce::Slider*>(f.editor->controlFor(spec.id));
        REQUIRE(slider != nullptr);
        bool shown = false;
        for (auto* child : slider->getChildren())
            shown = shown || (dynamic_cast<juce::Label*>(child) != nullptr && child->isVisible());
        CHECK_MESSAGE(!shown, "the knob shows a text box at rest; its value appears only on hover or drag");
        const std::string unit = spec.unit;
        std::string units;
        if (unit == "Hz")
            units = "(Hz|kHz)";
        else if (unit == "ms" || unit == "s")
            units = "(ms|s)";
        else if (unit == "bpm")
            units = "(BPM|bpm)";
        else if (!unit.empty())
            units = "(" + unit + ")";
        std::vector<double> values{spec.min, spec.def, spec.max};
        for (const double v : values) {
            const std::string shownText = slider->getTextFromValue(v).toStdString();
            if (spec.kind == undertow::ParamKind::Choice) {
                const int index = static_cast<int>(std::lround(v));
                CHECK_MESSAGE(shownText == spec.choices[index], "at " << v << " the readout is \"" << shownText << "\"; a choice reads as its name, \""
                                                                      << std::string(spec.choices[index]) << "\"");
                continue;
            }
            const std::string number = spec.kind == undertow::ParamKind::Int ? "-?\\d+" : "-?\\d+(\\.\\d{1,2})?";
            const std::regex pattern = units.empty() ? std::regex("^" + number + "%?$") : std::regex("^" + number + " ?" + units + "$");
            CHECK_MESSAGE(std::regex_match(shownText, pattern),
                          "at " << v << " the readout is \"" << shownText << "\"; it must be "
                                << (spec.kind == undertow::ParamKind::Int ? "a whole number" : "a number with at most two decimals")
                                << (units.empty() ? "" : " and its unit " + units));
        }
    }
}

TEST_CASE("P4.2.04 every control has the design's short label, drawn under it") {
    Fixture f;
    const auto image = f.shot(idleFrame());
    const auto drawn = f.editor->drawnTexts();
    for (const auto& panel : designPanels()) {
        for (const auto& c : panel.controls) {
            INFO("`" << std::string(c.id) << "`, label \"" << std::string(c.label) << "\"");
            auto* control = f.editor->controlFor(c.id);
            REQUIRE(control != nullptr);
            const auto b = boundsOf(*f.editor, *control);
            const DrawnText* found = nullptr;
            for (const auto& t : drawn)
                if (trimmed(t.text) == c.label && std::abs(t.bounds.getCentreX() - b.getCentreX()) <= 10 &&
                    t.bounds.getY() >= b.getBottom() - 4 && t.bounds.getY() <= b.getBottom() + 24 && panel.area.contains(t.bounds))
                    found = &t;
            CHECK_MESSAGE(found != nullptr, "the editor must draw \"" << std::string(c.label) << "\" under the control (" << text(b)
                                                                     << "): centred on it within 10 px, starting at most 24 px below it, inside "
                                                                     << std::string(panel.stamp) << " " << text(panel.area));
            if (found != nullptr)
                CHECK_MESSAGE(inkIn(image, found->bounds) >= 3, "\"" << std::string(c.label) << "\" is recorded at " << text(found->bounds)
                                                                     << " but nothing is drawn there");
        }
    }
}

TEST_CASE("P4.2.05 each panel carries its stamp in its top-left corner: OSC/01 to OUT/08, none overlapping") {
    Fixture f;
    const auto image = f.shot(idleFrame());
    const auto drawn = f.editor->drawnTexts();
    struct Corner {
        const char* stamp;
        juce::Rectangle<int> area;
        int x;  // where the stamp starts, within 32 px
    };
    // OSC/01 to MOD/05 and SEQ/06 sit in their panels' top-left corners; VOICE/07 and OUT/08 head their GROUND groups.
    std::vector<Corner> corners{{"OSC/01", {16, 56, 280, 160}, 16},  {"FILTER/02", {16, 216, 280, 144}, 16},
                                {"ENV/03", {16, 360, 280, 160}, 16}, {"RUMBLE/04", {904, 56, 280, 264}, 904},
                                {"MOD/05", {904, 320, 280, 200}, 904}, {"SEQ/06", kGut, 16}};
    for (const char* group : {"VOICE/07", "OUT/08"}) {
        for (const auto& panel : designPanels()) {
            if (std::string(panel.stamp) != group)
                continue;
            int left = 1 << 30;
            for (const auto& c : panel.controls)
                if (auto* control = f.editor->controlFor(c.id))
                    left = std::min(left, boundsOf(*f.editor, *control).getX());
            corners.push_back({group, kGround, left});
        }
    }
    std::vector<juce::Rectangle<int>> stamps;
    for (const auto& corner : corners) {
        INFO("stamp " << std::string(corner.stamp));
        const DrawnText* found = nullptr;
        for (const auto& t : drawn)
            if (t.text.contains(corner.stamp) && corner.area.contains(t.bounds) && std::abs(t.bounds.getX() - corner.x) <= 32 &&
                t.bounds.getY() - corner.area.getY() <= 24)
                found = &t;
        CHECK_MESSAGE(found != nullptr, "the editor must draw \"" << std::string(corner.stamp) << "\" at the top left of " << text(corner.area)
                                                                  << ", starting within 32 px of x " << corner.x);
        if (found == nullptr)
            continue;
        CHECK_MESSAGE(inkIn(image, found->bounds) >= 6, "\"" << std::string(corner.stamp) << "\" is recorded at " << text(found->bounds)
                                                             << " but nothing is drawn there");
        for (const auto& other : stamps)
            CHECK_MESSAGE(!found->bounds.intersects(other), "\"" << std::string(corner.stamp) << "\" (" << text(found->bounds)
                                                                 << ") overlaps another stamp at " << text(other));
        stamps.push_back(found->bounds);
    }
}
