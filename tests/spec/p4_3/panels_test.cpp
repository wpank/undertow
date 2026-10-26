// Phase 4.3 spec tests: every panel's controls fit it: rows aligned, labels clear of the other controls, margins inside
// each panel, and the limbs' knobs where the mock puts them (docs/specs/phase-4.3-finish.md, P4.3.01 to P4.3.04).
// spec-preset: plugin

#include "doctest.h"

#include "spec_ui.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace spec;

namespace {

const juce::Rectangle<int> kGround{0, 672, 1200, 88};

// The design's panels and their controls with labels (docs/design/undertow-mock.svg), as phase 4.2's checks hold them.
struct Panel {
    const char* stamp;
    juce::Rectangle<int> area;
    std::vector<std::pair<const char*, const char*>> controls;  // id, label
};

const std::vector<Panel>& panels() {
    static const std::vector<Panel> all{
        {"OSC/01", {16, 56, 280, 160},
         {{"osc_wave", "WAVE"}, {"osc_pw", "PW"}, {"osc_level", "LEVEL"}, {"osc_octave", "OCT"}, {"osc_fine", "FINE"},
          {"sub_level", "SUB"}, {"sub_octave", "SUB OCT"}}},
        {"FILTER/02", {16, 216, 280, 144},
         {{"filter_cutoff", "CUTOFF"}, {"filter_res", "RES"}, {"filter_drive", "DRIVE"}, {"filter_env", "ENV"},
          {"filter_track", "TRACK"}}},
        {"ENV/03", {16, 360, 280, 160},
         {{"amp_attack", "A"}, {"amp_decay", "D"}, {"amp_sustain", "S"}, {"amp_release", "R"}, {"filt_attack", "F A"},
          {"filt_decay", "F D"}, {"filt_sustain", "F S"}, {"filt_release", "F R"}}},
        {"RUMBLE/04", {904, 56, 280, 264},
         {{"rumble_level", "LEVEL"}, {"rumble_send", "SEND"}, {"rumble_kick", "KICK"}, {"rumble_rate", "RATE"},
          {"rumble_decay", "DECAY"}, {"rumble_tone", "TONE"}, {"rumble_drive", "DRIVE"}, {"rumble_duck", "DUCK"}}},
        {"MOD/05", {904, 320, 280, 200},
         {{"lfo1_rate", "1 RATE"}, {"lfo1_shape", "1 SHAPE"}, {"lfo1_cutoff", "1 CUT"}, {"lfo1_phase", "1 PHASE"},
          {"lfo2_rate", "2 RATE"}, {"lfo2_shape", "2 SHAPE"}, {"lfo2_dest", "2 DEST"}, {"lfo2_depth", "2 DEPTH"},
          {"lfo2_phase", "2 PHASE"}}},
        {"GROUND", kGround,
         {{"glide_ms", "GLIDE"}, {"glide_mode", "MODE"}, {"accent", "ACCENT"}, {"bend_range", "BEND"}, {"sat_drive", "SAT"},
          {"width", "WIDTH"}, {"mono_freq", "MONO"}, {"master_gain", "MASTER"}, {"seq_on", "SEQ"}, {"seq_sync", "SYNC"},
          {"seq_bpm", "BPM"}, {"seq_rate", "RATE"}, {"seq_length", "LEN"}, {"seq_gate", "GATE"}, {"seq_swing", "SWING"},
          {"seq_root", "ROOT"}, {"seq_scale", "SCALE"}, {"seq_mutate", "MUTATE"}, {"seq_evolve", "EVOLVE"},
          {"seq_seed", "SEED"}}},
    };
    return all;
}

// Where the mock centres each knob in the limbs: its dial's centre in docs/design/undertow-mock.svg.
struct MockCentre {
    const char* id;
    int x, y;
};

const std::vector<MockCentre>& mockCentres() {
    static const std::vector<MockCentre> all{
        {"osc_wave", 54, 98},        {"osc_pw", 122, 98},         {"osc_level", 190, 98},       {"osc_octave", 258, 98},
        {"osc_fine", 54, 162},       {"sub_level", 122, 162},     {"sub_octave", 190, 162},     {"filter_cutoff", 54, 258},
        {"filter_res", 122, 258},    {"filter_drive", 190, 258},  {"filter_env", 258, 258},     {"filter_track", 54, 322},
        {"amp_attack", 54, 402},     {"amp_decay", 122, 402},     {"amp_sustain", 190, 402},    {"amp_release", 258, 402},
        {"filt_attack", 54, 466},    {"filt_decay", 122, 466},    {"filt_sustain", 190, 466},   {"filt_release", 258, 466},
        {"rumble_level", 942, 210},  {"rumble_send", 1010, 210},  {"rumble_kick", 1078, 210},   {"rumble_rate", 1146, 210},
        {"rumble_decay", 942, 274},  {"rumble_tone", 1010, 274},  {"rumble_drive", 1078, 274},  {"rumble_duck", 1146, 274},
        {"lfo1_rate", 942, 360},     {"lfo1_shape", 1010, 360},   {"lfo1_cutoff", 1078, 360},   {"lfo1_phase", 1146, 360},
        {"lfo2_rate", 942, 424},     {"lfo2_shape", 1010, 424},   {"lfo2_dest", 1078, 424},     {"lfo2_depth", 1146, 424},
        {"lfo2_phase", 942, 488},
    };
    return all;
}

struct Placed {
    std::string id, label;
    juce::Rectangle<int> control, labelBox;  // labelBox empty when the label is missing (phase 4.2's P4.2.04 says so)
    const Panel* panel = nullptr;
};

// Each control's bounds and its label's box, as the editor drew them (P4.2.04's rule finds the label).
std::vector<Placed> place(UndertowEditor& editor, const std::vector<DrawnText>& drawn) {
    std::vector<Placed> out;
    for (const auto& panel : panels()) {
        for (const auto& entry : panel.controls) {
            const std::string id = entry.first, label = entry.second;
            auto* control = editor.controlFor(id);
            REQUIRE_MESSAGE(control != nullptr, "controlFor must find `" << id << "`");
            Placed p{id, label, editor.getLocalArea(control, control->getLocalBounds()), {}, &panel};
            for (const auto& t : drawn)
                if (t.text.trim().toStdString() == label && std::abs(t.bounds.getCentreX() - p.control.getCentreX()) <= 10 &&
                    t.bounds.getY() >= p.control.getBottom() - 4 && t.bounds.getY() <= p.control.getBottom() + 24)
                    p.labelBox = t.bounds;
            out.push_back(p);
        }
    }
    return out;
}

std::string text(const juce::Rectangle<int>& r) { return r.toString().toStdString(); }

} // namespace

TEST_CASE("P4.3.01 the controls in a row share their centre: no knob sits off its row") {
    Fixture f;
    f.shot(idleFrame());
    const auto placed = place(*f.editor, f.editor->drawnTexts());
    for (const auto& panel : panels()) {
        // A panel's rows: its controls by centre y, a new row where the next centre is more than 20 px lower.
        std::vector<const Placed*> inPanel;
        for (const auto& p : placed)
            if (p.panel == &panel)
                inPanel.push_back(&p);
        std::sort(inPanel.begin(), inPanel.end(),
                  [](const Placed* a, const Placed* b) { return a->control.getCentreY() < b->control.getCentreY(); });
        std::size_t start = 0;
        for (std::size_t i = 1; i <= inPanel.size(); ++i) {
            if (i < inPanel.size() && inPanel[i]->control.getCentreY() - inPanel[i - 1]->control.getCentreY() <= 20)
                continue;
            const int top = inPanel[start]->control.getCentreY(), bottom = inPanel[i - 1]->control.getCentreY();
            CHECK_MESSAGE(bottom - top <= 3, std::string(panel.stamp) << ": the row of `" << inPanel[start]->id << "` to `"
                                                                     << inPanel[i - 1]->id << "` has centres from y " << top
                                                                     << " to " << bottom << "; a row shares its centre within 3 px");
            start = i;
        }
    }
}

TEST_CASE("P4.3.02 no label meets another control: each keeps 4 px clear of every control but its own") {
    Fixture f;
    f.shot(idleFrame());
    const auto placed = place(*f.editor, f.editor->drawnTexts());
    for (const auto& p : placed) {
        if (p.labelBox.isEmpty())
            continue;  // a missing label is P4.2.04's to report
        for (const auto& other : placed) {
            if (other.id == p.id)
                continue;
            CHECK_MESSAGE(!p.labelBox.intersects(other.control.expanded(4)),
                          "the label \"" << p.label << "\" (" << text(p.labelBox) << ") of `" << p.id << "` comes within 4 px of `"
                                         << other.id << "`'s control (" << text(other.control) << ")");
        }
    }
}

TEST_CASE("P4.3.03 every control and label lies inside its panel with 4 px to spare") {
    Fixture f;
    f.shot(idleFrame());
    const auto placed = place(*f.editor, f.editor->drawnTexts());
    for (const auto& p : placed) {
        const auto inner = p.panel->area.reduced(4);
        const std::string stamp = p.panel->stamp;
        CHECK_MESSAGE(inner.contains(p.control), "`" << p.id << "`'s control (" << text(p.control) << ") must lie 4 px inside "
                                                     << stamp << " " << text(p.panel->area));
        if (!p.labelBox.isEmpty())
            CHECK_MESSAGE(inner.contains(p.labelBox), "the label \"" << p.label << "\" (" << text(p.labelBox) << ") must lie 4 px inside "
                                                                     << stamp << " " << text(p.panel->area));
    }
}

TEST_CASE("P4.3.04 each knob in the limbs sits where the mock puts it, its centre within 8 px of the mock's") {
    Fixture f;
    f.shot(idleFrame());
    for (const auto& m : mockCentres()) {
        const std::string id = m.id;
        auto* control = f.editor->controlFor(id);
        REQUIRE_MESSAGE(control != nullptr, "controlFor must find `" << id << "`");
        const auto centre = f.editor->getLocalArea(control, control->getLocalBounds()).getCentre();
        CHECK_MESSAGE((std::abs(centre.x - m.x) <= 8 && std::abs(centre.y - m.y) <= 8),
                      "`" << id << "` is centred at " << centre.x << ", " << centre.y << "; docs/design/undertow-mock.svg centres it at "
                          << m.x << ", " << m.y);
    }
}
