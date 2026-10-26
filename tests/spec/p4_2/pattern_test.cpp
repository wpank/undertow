// Phase 4.2 spec tests: a new engine and a new plugin play and keep the default pattern (docs/specs/phase-4.2-layout.md,
// P4.2.14 to P4.2.16; audio-qa's F16).
// spec-preset: plugin
// spec-needs: plugin/ui/Texts.h
// (The engine's pattern exists from phase 3; the line above only keeps this test out of tools/check.sh until 4.2 runs.)

#include "doctest.h"

#include "PluginProcessor.h"
#include "spec_analysis.h"
#include "spec_engine.h"
#include "undertow/engine.h"
#include "undertow/sequencer.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>
#include <memory>
#include <vector>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace {

constexpr double kRate = 48000.0;
constexpr int kStep = 5625;  // a 1/16 at 128 BPM, in samples
constexpr int kBlock = 375;  // 15 blocks to a step

struct Bar {
    spec::Stereo out;
    std::vector<int> notes;  // meters().note after each block
};

// One bar (16 steps) from beat 0 on a host transport at 128 BPM, with the sequencer on and the rumble off, so a rest
// is silent. `pattern` null: the engine's own pattern, as a new instance has it.
Bar playBar(const undertow::Pattern* pattern) {
    undertow::Engine engine;
    engine.prepare(kRate, 512);
    auto p = spec::defaults();
    spec::set(p, "seq_on", 1);
    spec::set(p, "rumble_level", 0.0f);
    spec::apply(engine, p);
    if (pattern != nullptr)
        engine.setPattern(*pattern);
    Bar bar;
    bar.out.left.resize(16 * kStep);
    bar.out.right.resize(16 * kStep);
    undertow::Transport transport{true, true, 128.0, 0.0};
    for (int start = 0; start < 16 * kStep; start += kBlock) {
        engine.process(bar.out.left.data() + start, bar.out.right.data() + start, kBlock, nullptr, 0, transport);
        transport.ppq += kBlock / kRate * transport.bpm / 60.0;
        bar.notes.push_back(engine.meters().note);
    }
    return bar;
}

// The note meter at `steps` into the bar (the block that ends there).
int noteAt(const Bar& bar, double steps) {
    const auto block = static_cast<std::size_t>(std::lround(steps * kStep / kBlock)) - 1;
    return bar.notes[std::min(block, bar.notes.size() - 1)];
}

} // namespace

TEST_CASE("P4.2.14 a new engine plays the default bassline: step 2 is A2, accented; step 5 rests; the bar as defaultPattern() plays it") {
    const Bar fresh = playBar(nullptr);
    const undertow::Pattern defaults = undertow::defaultPattern();
    const Bar given = playBar(&defaults);
    CHECK_MESSAGE(noteAt(fresh, 2.4) == 45, "a new engine with the sequencer on must play the default pattern: step 2 is A2 (45, root 33 "
                                            "plus 12); the note meter read "
                                                << noteAt(fresh, 2.4));
    CHECK_MESSAGE(noteAt(fresh, 5.9) == -1, "step 5 of the default pattern is a rest: the voice must be idle by its end; the note meter read "
                                                << noteAt(fresh, 5.9));
    double worst = 0.0;
    for (std::size_t i = 0; i < fresh.out.left.size(); ++i)
        worst = std::max({worst, std::fabs(static_cast<double>(fresh.out.left[i] - given.out.left[i])),
                          std::fabs(static_cast<double>(fresh.out.right[i] - given.out.right[i]))});
    CHECK_MESSAGE(worst <= 1e-6, "a new engine's bar must be the bar an engine given defaultPattern() plays (accents, slides, rests and "
                                 "the probability step included); they differ by up to "
                                     << worst);
}

TEST_CASE("P4.2.15 a new engine's pattern() stays defaultPattern() once audio runs") {
    undertow::Engine engine;
    engine.prepare(kRate, 512);
    std::vector<float> left(512), right(512);
    engine.process(left.data(), right.data(), 512, nullptr, 0, {});
    CHECK_MESSAGE(engine.pattern() == undertow::defaultPattern(), "after one block a new engine's pattern() must still be "
                                                                  "defaultPattern(): the sequencer plays it, and pattern() reads it back");
    auto p = spec::defaults();
    spec::set(p, "seq_on", 1);
    spec::apply(engine, p);
    for (int i = 0; i < 20; ++i)
        engine.process(left.data(), right.data(), 512, nullptr, 0, undertow::Transport{true, true, 128.0, i * 512 / kRate * 128.0 / 60.0});
    CHECK_MESSAGE(engine.pattern() == undertow::defaultPattern(), "with the sequencer running, pattern() must still be defaultPattern()");
}

TEST_CASE("P4.2.16 a new plugin saves the default pattern with its state") {
    juce::ScopedJuceInitialiser_GUI gui;
    std::unique_ptr<juce::AudioProcessor> fresh(createPluginFilter());
    fresh->prepareToPlay(kRate, 512);
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    fresh->processBlock(buffer, midi);
    juce::MemoryBlock state;
    fresh->getStateInformation(state);
    std::unique_ptr<juce::AudioProcessor> copy(createPluginFilter());
    auto* restored = dynamic_cast<UndertowProcessor*>(copy.get());
    REQUIRE(restored != nullptr);
    copy->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    CHECK_MESSAGE(restored->pattern() == undertow::defaultPattern(),
                  "a new plugin that has run one block saves defaultPattern() in getStateInformation: restoring its state "
                  "must give the default pattern back");
}
