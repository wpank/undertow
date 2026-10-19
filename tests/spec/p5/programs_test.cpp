// Phase 5 spec tests: the factory presets as the plugin's programs (docs/specs/phase-5-ship.md, P5.04).
// spec-preset: plugin
// spec-needs: core/include/undertow/presets.h

#include "doctest.h"

#include "PluginProcessor.h"
#include "undertow/params.h"
#include "undertow/presets.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <cmath>
#include <memory>
#include <string>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

TEST_CASE("P5.04 the factory presets are the plugin's programs") {
    juce::ScopedJuceInitialiser_GUI gui;
    std::unique_ptr<juce::AudioProcessor> processor(createPluginFilter());
    auto* undertowProcessor = dynamic_cast<UndertowProcessor*>(processor.get());
    REQUIRE(undertowProcessor != nullptr);
    const int count = undertow::numFactoryPresets();
    REQUIRE_MESSAGE(processor->getNumPrograms() == count, "getNumPrograms() is " << processor->getNumPrograms()
                                                                                 << "; it must be the number of factory presets (" << count << ")");
    processor->prepareToPlay(48000.0, 512);
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    for (int i = 0; i < count; ++i) {
        const std::string name = undertow::factoryPresetName(i);
        INFO("program " << i << " \"" << name << "\"");
        CHECK_MESSAGE(processor->getProgramName(i).toStdString() == name, "getProgramName must be the preset's name");
        processor->setCurrentProgram(i);
        CHECK_MESSAGE(processor->getCurrentProgram() == i, "getCurrentProgram must follow setCurrentProgram");
        std::array<float, undertow::kNumParams> values{};
        undertow::Pattern pattern;
        REQUIRE(undertow::factoryPreset(i, values.data(), &pattern));
        int wrong = 0;
        std::string first;
        for (int p = 0; p < undertow::kNumParams; ++p) {
            auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(processor->getParameters()[p]);
            REQUIRE(parameter != nullptr);
            const float got = parameter->convertFrom0to1(parameter->getValue());
            const float want = values[static_cast<std::size_t>(p)];
            if (std::fabs(got - want) > 1e-3f * std::max(1.0f, std::fabs(want))) {
                if (wrong++ == 0)
                    first = std::string(undertow::paramSpec(p).id) + " is " + std::to_string(got) + ", the preset says " + std::to_string(want);
            }
        }
        CHECK_MESSAGE(wrong == 0, "setCurrentProgram must set every parameter to the preset's value; " << wrong << " differ (" << first << ")");
        processor->processBlock(buffer, midi);
        CHECK_MESSAGE(undertowProcessor->pattern() == pattern, "setCurrentProgram must also load the preset's pattern");
    }
}
