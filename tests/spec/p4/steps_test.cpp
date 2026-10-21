// Phase 4 spec tests: the unit array (docs/specs/phase-4-ui.md, P4.10).
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

TEST_CASE("P4.10 the unit array: 16 cells, hit-testing, the playing step lit, and edits reach the pattern") {
    Fixture f;
    juce::Rectangle<int> previous;
    for (int i = 0; i < undertow::kSteps; ++i) {
        const auto cell = f.editor->stepBounds(i);
        INFO("step " << i << " at " << cell.toString());
        CHECK_MESSAGE(cell.getY() >= 520, "the cells sit in the GUT (y 520 to 680)");
        CHECK_MESSAGE(cell.getBottom() <= 680, "the cells sit in the GUT (y 520 to 680)");
        CHECK_MESSAGE(cell.getWidth() >= 40, "a cell must be at least 40 px wide");
        if (i > 0)
            CHECK_MESSAGE(cell.getX() >= previous.getRight(), "cells run left to right without overlapping");
        CHECK_MESSAGE(f.editor->stepAt(cell.getCentre()) == i, "stepAt(the cell's centre) must be its index");
        previous = cell;
    }
    CHECK_MESSAGE(f.editor->stepAt({600, 100}) == -1, "stepAt outside the array is -1");

    UiFrame frame = playingFrame();
    frame.step = 5;
    const auto image = f.shot(frame);
    int brightest = -1;
    double best = -1;
    for (int i = 0; i < undertow::kSteps; ++i) {
        const double m = mean(measure(image, f.editor->stepBounds(i)));
        if (m > best) {
            best = m;
            brightest = i;
        }
    }
    CHECK_MESSAGE(brightest == 5, "the playing step (5) must be the brightest cell; cell " << brightest << " is");

    auto* processor = dynamic_cast<UndertowProcessor*>(f.processor.get());
    const auto before = processor->pattern();
    f.editor->editStep(2, StepEdit::ToggleGate);
    f.editor->editStep(4, StepEdit::ToggleAccent);
    f.editor->editStep(6, StepEdit::ToggleSlide);
    f.editor->editStep(8, StepEdit::Up);
    f.editor->editStep(9, StepEdit::Down);
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    f.processor->prepareToPlay(48000.0, 512);
    f.processor->processBlock(buffer, midi);
    const auto after = processor->pattern();
    INFO("the edits came before prepareToPlay, as an editor's can, so prepare must keep the pattern");
    CHECK_MESSAGE(after.steps[2].gate != before.steps[2].gate, "a click (ToggleGate) must toggle the step's gate");
    CHECK_MESSAGE(after.steps[4].accent != before.steps[4].accent, "a shift-click (ToggleAccent) must toggle the accent");
    CHECK_MESSAGE(after.steps[6].slide != before.steps[6].slide, "an alt-click (ToggleSlide) must toggle the slide");
    CHECK_MESSAGE(after.steps[8].semitones == before.steps[8].semitones + 1, "dragging up (Up) adds a semitone");
    CHECK_MESSAGE(after.steps[9].semitones == before.steps[9].semitones - 1, "dragging down (Down) takes a semitone away");
}

