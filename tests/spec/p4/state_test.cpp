// Phase 4 spec tests: the pattern in the plugin state (docs/specs/phase-4-ui.md, P4.02).
// spec-preset: plugin
// spec-needs: plugin/ui/UiFrame.h

#include "doctest.h"

#include "PluginProcessor.h"
#include "undertow/engine.h"
#include "undertow/sequencer.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <vector>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

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
    CHECK_MESSAGE(processor->pattern() == pattern, "after setPattern and one block the processor's pattern() must be the new one "
                                                   "(setPattern came before prepareToPlay, so prepare must keep the pattern)");
    juce::MemoryBlock state;
    source->getStateInformation(state);
    std::unique_ptr<juce::AudioProcessor> copy(createPluginFilter());
    auto* restored = dynamic_cast<UndertowProcessor*>(copy.get());
    REQUIRE(restored != nullptr);
    copy->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    copy->prepareToPlay(48000.0, 512);
    copy->processBlock(buffer, midi);
    CHECK_MESSAGE(restored->pattern() == pattern, "the 16 steps (note, gate, accent, slide, probability) must be saved in "
                                                  "getStateInformation and restored by setStateInformation (which a host "
                                                  "calls before prepareToPlay, so prepare must keep the pattern)");
}


TEST_CASE("P4.02 the engine keeps its pattern through reset and prepare") {
    // A host restores its state before prepareToPlay, and prepares again for a new sample rate or block size. Only a
    // new engine starts with defaultPattern() (P3.14).
    undertow::Pattern pattern = undertow::defaultPattern();
    pattern.steps[1].semitones = 5;
    pattern.steps[7].gate = false;
    undertow::Engine engine;
    engine.prepare(48000.0, 512);
    engine.setPattern(pattern);
    engine.reset();
    CHECK_MESSAGE(engine.pattern() == pattern, "reset() must keep the pattern set with setPattern; it went back to another");
    engine.prepare(44100.0, 256);
    CHECK_MESSAGE(engine.pattern() == pattern, "prepare() must keep the pattern (a host prepares again for a new sample rate "
                                               "or block size); it went back to another");
    std::vector<float> left(256), right(256);
    engine.process(left.data(), right.data(), 256, nullptr, 0, {});
    CHECK_MESSAGE(engine.pattern() == pattern, "after prepare and one block the engine must still be using the pattern");
    // The audio thread has taken the pattern now: a new prepare must keep it too (a sample-rate change mid-session),
    // so the default belongs in a new engine, never in reset().
    engine.prepare(48000.0, 256);
    engine.process(left.data(), right.data(), 256, nullptr, 0, {});
    CHECK_MESSAGE(engine.pattern() == pattern, "the pattern in use must survive a later prepare and play on; after prepare and a "
                                               "block the engine went back to another (is the default set in reset()?)");
}
