// Phase 5.5 spec tests: auval clean. A float parameter a host sets to its default reads back that default exactly, as
// auval checks it; Seq Gate's didn't (docs/specs/phase-5.5-auval.md, P5.5.01 to P5.5.04). Each sets parameters as a host
// does, through the normalised value: setValueNotifyingHost and getValue.
// spec-preset: plugin

#include "doctest.h"

#include "PluginProcessor.h"
#include "undertow/params.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace {

// A float to its last place: its value and its bits.
std::string exact(float f) {
    std::uint32_t bits;
    std::memcpy(&bits, &f, sizeof bits);
    char text[48];
    std::snprintf(text, sizeof text, "%.9g (0x%08x)", static_cast<double>(f), static_cast<unsigned>(bits));
    return text;
}

UndertowProcessor& undertowOf(const std::unique_ptr<juce::AudioProcessor>& processor) {
    auto* undertow = dynamic_cast<UndertowProcessor*>(processor.get());
    REQUIRE(undertow != nullptr);
    return *undertow;
}

juce::RangedAudioParameter& parameterOf(UndertowProcessor& undertow, const char* id) {
    auto* parameter = undertow.parameters().getParameter(id);
    REQUIRE_MESSAGE(parameter != nullptr, "the plugin must keep the parameter id " << std::string(id));
    return *parameter;
}

} // namespace

TEST_CASE("P5.5.01 Seq Gate comes back as its default: set by a host to 1 and then to its default, it reads its default exactly") {
    juce::ScopedJuceInitialiser_GUI gui;
    std::unique_ptr<juce::AudioProcessor> processor(createPluginFilter());
    auto& gate = parameterOf(undertowOf(processor), "seq_gate");
    const float def = gate.getDefaultValue();
    gate.setValueNotifyingHost(1.0f);
    gate.setValueNotifyingHost(def);
    const float back = gate.getValue();
    CHECK_MESSAGE(back == def, "set to 1 and then to its default, " << exact(def) << ", Seq Gate reads back " << exact(back) << ", "
                                                                    << exact(gate.convertFrom0to1(back))
                                                                    << " in its own units. auval compares exactly and warns \"Parameter did "
                                                                       "not retain default value when set\": JUCE's 0-1 conversion takes 0.5 "
                                                                       "to 0.49999997 (a fused multiply-add). Snap Seq Gate's range to a 0.001 "
                                                                       "grid in the plugin's parameter layout");
}

TEST_CASE("P5.5.02 every float parameter comes back as its default, set by a host from either end of its range") {
    juce::ScopedJuceInitialiser_GUI gui;
    std::unique_ptr<juce::AudioProcessor> processor(createPluginFilter());
    auto& undertow = undertowOf(processor);
    int floats = 0;
    for (int i = 0; i < undertow::kNumParams; ++i) {
        const auto& spec = undertow::paramSpec(i);
        if (spec.kind != undertow::ParamKind::Float)
            continue;
        ++floats;
        const std::string id = spec.id;
        auto& parameter = parameterOf(undertow, spec.id);
        const float def = parameter.getDefaultValue();
        for (const float from : {1.0f, 0.0f}) {
            parameter.setValueNotifyingHost(from);
            parameter.setValueNotifyingHost(def);
            const float back = parameter.getValue();
            CHECK_MESSAGE(back == def, id << ", set to " << from << " and then to its default " << exact(def) << ", reads back "
                                          << exact(back) << "; a fix for one parameter must not move the problem to another");
        }
    }
    CHECK_MESSAGE(floats >= 38, "the parameter table holds 38 float parameters; found " << floats);
}

TEST_CASE("P5.5.03 every float parameter keeps its id, its range and its default, as the host sees them; only Seq Gate's range "
          "has a grid") {
    juce::ScopedJuceInitialiser_GUI gui;
    std::unique_ptr<juce::AudioProcessor> processor(createPluginFilter());
    auto& undertow = undertowOf(processor);
    for (int i = 0; i < undertow::kNumParams; ++i) {
        const auto& spec = undertow::paramSpec(i);
        if (spec.kind != undertow::ParamKind::Float)
            continue;
        const std::string id = spec.id;
        auto& parameter = parameterOf(undertow, spec.id);
        const auto& range = parameter.getNormalisableRange();
        CHECK_MESSAGE(range.start == spec.min, id << "'s range starts at " << range.start << "; the parameter table says " << spec.min);
        CHECK_MESSAGE(range.end == spec.max, id << "'s range ends at " << range.end << "; the parameter table says " << spec.max);
        if (i != undertow::param::SeqGate)
            CHECK_MESSAGE(range.interval == 0.0f, id << "'s range has a grid of " << range.interval
                                                     << ": only Seq Gate's needs one. A grid changes the steps and the text a host shows");
        const float shown = parameter.convertFrom0to1(parameter.getDefaultValue());
        CHECK_MESSAGE(std::abs(shown - spec.def) <= 1e-6f * (spec.max - spec.min),
                      id << "'s default reaches the host as " << shown << "; the parameter table says " << spec.def
                         << ". The fix keeps every default: a grid on the range must hold it");
    }
}

TEST_CASE("P5.5.04 a saved state with Seq Gate off the 0.001 grid still loads and plays: Seq Gate within 0.0005 of it, the rest "
          "as saved") {
    juce::ScopedJuceInitialiser_GUI gui;
    std::unique_ptr<juce::AudioProcessor> source(createPluginFilter());
    juce::MemoryBlock state;
    source->getStateInformation(state);
    auto xml = juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
    REQUIRE_MESSAGE(xml != nullptr, "getStateInformation must save the state as XML");
    auto* saved = xml->getChildByAttribute("id", "seq_gate");
    REQUIRE_MESSAGE(saved != nullptr, "the saved state must hold seq_gate, as <PARAM id=\"seq_gate\" value=\"...\"/>");
    // A value saved before the fix, or by a host's automation: off any 0.001 grid.
    constexpr double kSaved = 0.4567891;
    saved->setAttribute("value", kSaved);
    juce::AudioProcessor::copyXmlToBinary(*xml, state);

    std::unique_ptr<juce::AudioProcessor> copy(createPluginFilter());
    copy->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    auto& restored = undertowOf(copy);
    auto& gate = parameterOf(restored, "seq_gate");
    const float loaded = gate.convertFrom0to1(gate.getValue());
    CHECK_MESSAGE(std::abs(loaded - kSaved) <= 0.0005 + 1e-6, "a state saved with Seq Gate at 0.4567891 loads it as " << exact(loaded)
                                                                                                                     << "; it must load within 0.0005 of it "
                                                                                                                        "(a 0.001 grid loads it as 0.457)");
    auto& original = undertowOf(source);
    for (int i = 0; i < undertow::kNumParams; ++i) {
        const auto& spec = undertow::paramSpec(i);
        if (spec.kind != undertow::ParamKind::Float || i == undertow::param::SeqGate)
            continue;
        const std::string id = spec.id;
        auto& before = parameterOf(original, spec.id);
        auto& after = parameterOf(restored, spec.id);
        const float want = before.convertFrom0to1(before.getValue()), got = after.convertFrom0to1(after.getValue());
        CHECK_MESSAGE(std::abs(got - want) <= 1e-6f * (spec.max - spec.min), id << " loads as " << got << "; it was saved as " << want);
    }

    copy->prepareToPlay(48000.0, 512);
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 45, static_cast<juce::uint8>(100)), 0);
    double sum = 0.0;
    bool finite = true;
    for (int block = 0; block < 20; ++block) {
        buffer.clear();
        copy->processBlock(buffer, midi);
        midi.clear();
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < buffer.getNumSamples(); ++n) {
                const float v = buffer.getSample(ch, n);
                finite = finite && std::isfinite(v);
                sum += static_cast<double>(v) * v;
            }
    }
    const double rms = std::sqrt(sum / (20.0 * 2.0 * 512.0));
    CHECK_MESSAGE(finite, "the restored plugin must play finite samples");
    CHECK_MESSAGE(rms > 1e-3, "the restored plugin, holding A2, plays at " << 20.0 * std::log10(std::max(rms, 1e-12))
                                                                         << " dBFS RMS; it must be heard (-60 dBFS or more)");
}
