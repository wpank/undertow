// Phase 4 spec tests: the pattern in the plugin state (docs/specs/phase-4-ui.md, P4.02).
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

TEST_CASE("P4.02 the pattern is part of the plugin's state, and the processor passes it to the engine") {
    juce::ScopedJuceInitialiser_GUI gui;
    std::unique_ptr<juce::AudioProcessor> source(createPluginFilter());
    auto* processor = dynamic_cast<UndertowProcessor*>(source.get());
    REQUIRE(processor != nullptr);
    undertow::Pattern pattern = undertow::defaultPattern();
    pattern.steps[0].semitones = 7;
    pattern.steps[3].gate = false;
    pattern.steps[9].accent = true;
    pattern.steps[12].probability = 0.25f;
    pattern.steps[13].slide = true;
    processor->setPattern(pattern);
    source->prepareToPlay(48000.0, 512);
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    source->processBlock(buffer, midi);
    CHECK_MESSAGE(processor->pattern() == pattern, "after setPattern and one block the processor's pattern() must be the new one");
    juce::MemoryBlock state;
    source->getStateInformation(state);
    std::unique_ptr<juce::AudioProcessor> copy(createPluginFilter());
    auto* restored = dynamic_cast<UndertowProcessor*>(copy.get());
    REQUIRE(restored != nullptr);
    copy->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    copy->prepareToPlay(48000.0, 512);
    copy->processBlock(buffer, midi);
    CHECK_MESSAGE(restored->pattern() == pattern, "the 16 steps (note, gate, accent, slide, probability) must be saved in "
                                                  "getStateInformation and restored by setStateInformation");
}

