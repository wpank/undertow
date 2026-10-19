// Phase 0 spec tests: the JUCE plugin skeleton (docs/specs/phase-0-foundation.md, checks P0.08 to P0.13).
// spec-preset: plugin
// spec-needs: plugin/PluginProcessor.h
// spec-needs: plugin/PluginEditor.h

#include "doctest.h"

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "spec_alloc.h"
#include "undertow/params.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace {

std::unique_ptr<juce::AudioProcessor> makeProcessor() {
    return std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
}

juce::RangedAudioParameter* ranged(juce::AudioProcessor& processor, int index) {
    return dynamic_cast<juce::RangedAudioParameter*>(processor.getParameters()[index]);
}

// The value of `key` in a plist <dict> element, or nullptr.
juce::XmlElement* plistValue(juce::XmlElement& dict, const juce::String& key) {
    for (auto* child = dict.getFirstChildElement(); child != nullptr; child = child->getNextElement())
        if (child->hasTagName("key") && child->getAllSubText() == key)
            return child->getNextElement();
    return nullptr;
}

std::unique_ptr<juce::XmlElement> readPlist(const juce::File& bundle) {
    auto xml = juce::XmlDocument::parse(bundle.getChildFile("Contents/Info.plist"));
    if (xml == nullptr)
        return nullptr;
    auto* dict = xml->getChildByName("dict");
    return dict != nullptr ? std::make_unique<juce::XmlElement>(*dict) : nullptr;
}

double luminance(juce::Colour c) {
    return (0.2126 * c.getRed() + 0.7152 * c.getGreen() + 0.0722 * c.getBlue()) / 255.0;
}

} // namespace

TEST_CASE("P0.08 the processor is the Metakosmia Undertow synth: MIDI in, one stereo output, no inputs") {
    juce::ScopedJuceInitialiser_GUI gui;
    auto processor = makeProcessor();
    REQUIRE_MESSAGE(processor != nullptr, "createPluginFilter() returned null");
    CHECK_MESSAGE(dynamic_cast<UndertowProcessor*>(processor.get()) != nullptr,
                  "createPluginFilter() must return an UndertowProcessor (plugin/PluginProcessor.h)");
    CHECK_MESSAGE(processor->getName() == "Metakosmia Undertow",
                  "getName() is \"" << processor->getName() << "\": PRODUCT_NAME must be \"Metakosmia Undertow\"");
    CHECK_MESSAGE(processor->acceptsMidi(), "the synth must accept MIDI (NEEDS_MIDI_INPUT TRUE)");
    CHECK_MESSAGE(!processor->producesMidi(), "the synth must not produce MIDI");
    CHECK_MESSAGE(!processor->isMidiEffect(), "the synth is not a MIDI effect");
    CHECK_MESSAGE(processor->getBusCount(true) == 0, "a synth has no input bus; found " << processor->getBusCount(true));
    REQUIRE_MESSAGE(processor->getBusCount(false) == 1, "the synth has exactly one output bus");
    CHECK_MESSAGE(processor->getChannelCountOfBus(false, 0) == 2, "the output bus is stereo by default");
    juce::AudioProcessor::BusesLayout stereo, mono;
    stereo.outputBuses.add(juce::AudioChannelSet::stereo());
    mono.outputBuses.add(juce::AudioChannelSet::mono());
    CHECK_MESSAGE(processor->checkBusesLayoutSupported(stereo), "isBusesLayoutSupported must accept one stereo output and no input");
    CHECK_MESSAGE(!processor->checkBusesLayoutSupported(mono),
                  "isBusesLayoutSupported must refuse a mono output: stereo is the only layout (processBlock writes two channels)");
}

TEST_CASE("P0.09 the plugin exposes the 57 parameters of the core table, in order, with the same ranges") {
    juce::ScopedJuceInitialiser_GUI gui;
    auto processor = makeProcessor();
    REQUIRE_MESSAGE(processor->getParameters().size() == undertow::kNumParams,
                    "the plugin has " << processor->getParameters().size() << " parameters; it must expose all "
                                      << undertow::kNumParams << " of undertow::paramSpec, built from that table");
    for (int i = 0; i < undertow::kNumParams; ++i) {
        const auto& spec = undertow::paramSpec(i);
        INFO("plugin parameter " << i << " must be `" << std::string(spec.id) << "`");
        auto* parameter = ranged(*processor, i);
        REQUIRE_MESSAGE(parameter != nullptr, "not a RangedAudioParameter (use the APVTS parameter classes)");
        CHECK_MESSAGE(parameter->getParameterID() == juce::String(spec.id), "its id is `" << parameter->getParameterID() << "`");
        CHECK_MESSAGE(parameter->getName(100) == juce::String(spec.name), "its name is \"" << parameter->getName(100) << "\"");
        const auto& range = parameter->getNormalisableRange();
        CHECK_MESSAGE(range.start == doctest::Approx(spec.min), "its range starts at " << range.start);
        CHECK_MESSAGE(range.end == doctest::Approx(spec.max), "its range ends at " << range.end);
        CHECK_MESSAGE(parameter->convertFrom0to1(parameter->getDefaultValue()) == doctest::Approx(spec.def).epsilon(1e-4),
                      "its default is " << parameter->convertFrom0to1(parameter->getDefaultValue()));
        switch (spec.kind) {
        case undertow::ParamKind::Float:
            CHECK_MESSAGE(dynamic_cast<juce::AudioParameterFloat*>(parameter) != nullptr, "a Float must be an AudioParameterFloat");
            if (spec.centre != 0.0f)
                CHECK_MESSAGE(range.convertFrom0to1(0.5f) == doctest::Approx(spec.centre).epsilon(0.01),
                              "half travel must land on the table's centre " << spec.centre << " (setSkewForCentre); got "
                                                                             << range.convertFrom0to1(0.5f));
            break;
        case undertow::ParamKind::Int:
            CHECK_MESSAGE(dynamic_cast<juce::AudioParameterInt*>(parameter) != nullptr, "an Int must be an AudioParameterInt");
            break;
        case undertow::ParamKind::Bool:
            CHECK_MESSAGE(dynamic_cast<juce::AudioParameterBool*>(parameter) != nullptr, "a Bool must be an AudioParameterBool");
            break;
        case undertow::ParamKind::Choice: {
            auto* choice = dynamic_cast<juce::AudioParameterChoice*>(parameter);
            REQUIRE_MESSAGE(choice != nullptr, "a Choice must be an AudioParameterChoice");
            REQUIRE(choice->choices.size() == spec.numChoices);
            for (int c = 0; c < spec.numChoices; ++c)
                CHECK(choice->choices[c] == juce::String(spec.choices[c]));
            break;
        }
        }
    }
}

TEST_CASE("P0.10 state saves and restores every parameter") {
    juce::ScopedJuceInitialiser_GUI gui;
    auto source = makeProcessor();
    std::vector<float> values;
    for (int i = 0; i < undertow::kNumParams; ++i) {
        const auto& spec = undertow::paramSpec(i);
        float value = spec.min + (spec.max - spec.min) * static_cast<float>((i * 37) % 100) / 100.0f;
        if (spec.kind != undertow::ParamKind::Float)
            value = std::round(value);
        if (spec.kind == undertow::ParamKind::Bool)
            value = static_cast<float>(i % 2);
        values.push_back(value);
        auto* parameter = ranged(*source, i);
        REQUIRE(parameter != nullptr);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    }
    juce::MemoryBlock state;
    source->getStateInformation(state);
    REQUIRE_MESSAGE(state.getSize() > 0, "getStateInformation wrote nothing");
    auto restored = makeProcessor();
    restored->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    for (int i = 0; i < undertow::kNumParams; ++i) {
        auto* parameter = ranged(*restored, i);
        REQUIRE(parameter != nullptr);
        const float got = parameter->convertFrom0to1(parameter->getValue());
        const float want = values[static_cast<std::size_t>(i)];
        CHECK_MESSAGE(got == doctest::Approx(want).epsilon(1e-4),
                      "parameter `" << std::string(undertow::paramSpec(i).id) << "` came back as " << got << ", saved as "
                                    << want << ": setStateInformation(getStateInformation()) must restore every parameter");
    }
}

TEST_CASE("P0.11 processBlock is finite, never allocates, and hands host parameter changes to the engine") {
    juce::ScopedJuceInitialiser_GUI gui;
    auto processor = makeProcessor();
    auto* undertowProcessor = dynamic_cast<UndertowProcessor*>(processor.get());
    REQUIRE(undertowProcessor != nullptr);
    processor->setPlayConfigDetails(0, 2, 48000.0, 512);
    processor->prepareToPlay(48000.0, 512);

    juce::AudioBuffer<float> buffer(2, 512);
    std::vector<juce::MidiBuffer> midi(4);
    midi[0].addEvent(juce::MidiMessage::noteOn(1, 45, static_cast<juce::uint8>(100)), 0);
    midi[1].addEvent(juce::MidiMessage::pitchWheel(1, 12000), 100);
    midi[2].addEvent(juce::MidiMessage::noteOff(1, 45), 300);
    midi[3].addEvent(juce::MidiMessage::allNotesOff(1), 511);

    const int cutoff = undertow::findParam("filter_cutoff");
    auto* cutoffParameter = ranged(*processor, cutoff);
    REQUIRE(cutoffParameter != nullptr);
    cutoffParameter->setValueNotifyingHost(0.25f);

    std::size_t allocations = 0;
    int nonFinite = 0;
    for (int block = 0; block < 200; ++block) {
        for (int ch = 0; ch < 2; ++ch)
            juce::FloatVectorOperations::fill(buffer.getWritePointer(ch), std::nanf(""), 512);
        auto& events = midi[static_cast<std::size_t>(block % 4)];
        spec::AllocationCounter counter;
        processor->processBlock(buffer, events);
        allocations += counter.count();
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < 512; ++n)
                nonFinite += std::isfinite(buffer.getSample(ch, n)) ? 0 : 1;
    }
    CHECK_MESSAGE(nonFinite == 0, nonFinite << " output samples were not finite after processBlock: write every sample "
                                               "of both channels, and never output NaN or infinity");
    CHECK_MESSAGE(allocations == 0, allocations << " heap allocations inside processBlock: preallocate the MIDI event "
                                                   "array and everything else in prepareToPlay");
    CHECK_MESSAGE(undertowProcessor->engine().getParam(cutoff) ==
                      doctest::Approx(cutoffParameter->convertFrom0to1(0.25f)).epsilon(1e-4),
                  "after processBlock the engine's filter_cutoff must equal the host value "
                      << cutoffParameter->convertFrom0to1(0.25f) << "; got " << undertowProcessor->engine().getParam(cutoff)
                      << ": processBlock must pass every parameter to engine().setParam");
    processor->releaseResources();
}

TEST_CASE("P0.12 the editor is a fixed 1200 x 760 dark ROSEDUST panel") {
    juce::ScopedJuceInitialiser_GUI gui;
    auto processor = makeProcessor();
    REQUIRE_MESSAGE(processor->hasEditor(), "hasEditor() must be true");
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditor());
    REQUIRE(editor != nullptr);
    CHECK_MESSAGE(dynamic_cast<UndertowEditor*>(editor.get()) != nullptr,
                  "createEditor() must return an UndertowEditor (plugin/PluginEditor.h)");
    CHECK_MESSAGE(editor->getWidth() == 1200, "the editor is " << editor->getWidth() << " wide; it must be 1200");
    CHECK_MESSAGE(editor->getHeight() == 760, "the editor is " << editor->getHeight() << " high; it must be 760");
    CHECK_MESSAGE(!editor->isResizable(), "the editor has a fixed size: setResizable(false, false)");
    const auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
    juce::Image::BitmapData pixels(image, juce::Image::BitmapData::readOnly);
    double sum = 0;
    int dark = 0, white = 0;
    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x) {
            const auto colour = pixels.getPixelColour(x, y);
            const double l = luminance(colour);
            sum += l;
            dark += l < 0.06 ? 1 : 0;
            white += (colour.getRed() >= 250 && colour.getGreen() >= 250 && colour.getBlue() >= 250) ? 1 : 0;
        }
    const double pixelsTotal = static_cast<double>(image.getWidth()) * image.getHeight();
    CHECK_MESSAGE(sum / pixelsTotal < 0.12, "the editor's mean luminance is " << sum / pixelsTotal
                                                                                << ": paint the bg_void ground (#060608)");
    CHECK_MESSAGE(dark / pixelsTotal >= 0.5, "only " << 100.0 * dark / pixelsTotal << "% of the editor is near-black void");
    CHECK_MESSAGE(white == 0, white << " pure white pixels: ROSEDUST never uses white (the brightest colour is rose_bright)");
    editor.reset();
}

TEST_CASE("P0.13 the build makes VST3, AU and Standalone bundles with the Metakosmia identifiers") {
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File vst3(UNDERTOW_VST3_BUNDLE), au(UNDERTOW_AU_BUNDLE), app(UNDERTOW_STANDALONE_BUNDLE);
    CHECK_MESSAGE(vst3.getFileName() == "Metakosmia Undertow.vst3", "the VST3 bundle is " << vst3.getFullPathName());
    CHECK_MESSAGE(au.getFileName() == "Metakosmia Undertow.component", "the AU bundle is " << au.getFullPathName());
    CHECK_MESSAGE(app.getFileName() == "Metakosmia Undertow.app", "the Standalone bundle is " << app.getFullPathName());
    for (const auto& bundle : {vst3, au, app}) {
        INFO("bundle " << bundle.getFullPathName());
        REQUIRE_MESSAGE(bundle.isDirectory(), "the bundle was not built");
        auto plist = readPlist(bundle);
        REQUIRE_MESSAGE(plist != nullptr, "Contents/Info.plist is missing or not an XML plist");
        auto* identifier = plistValue(*plist, "CFBundleIdentifier");
        REQUIRE(identifier != nullptr);
        CHECK_MESSAGE(identifier->getAllSubText() == "audio.metakosmia.undertow",
                      "CFBundleIdentifier is " << identifier->getAllSubText() << "; BUNDLE_ID must be audio.metakosmia.undertow");
    }
    auto plist = readPlist(au);
    REQUIRE(plist != nullptr);
    auto* components = plistValue(*plist, "AudioComponents");
    REQUIRE_MESSAGE(components != nullptr, "the AU Info.plist has no AudioComponents");
    auto* component = components->getChildByName("dict");
    REQUIRE(component != nullptr);
    auto text = [&](const char* key) {
        auto* value = plistValue(*component, key);
        return value != nullptr ? value->getAllSubText() : juce::String("<missing>");
    };
    CHECK_MESSAGE(text("type") == "aumu", "the AU type is " << text("type") << ": an instrument is aumu");
    CHECK_MESSAGE(text("subtype") == "MkUt", "the AU subtype is " << text("subtype") << ": PLUGIN_CODE MkUt");
    CHECK_MESSAGE(text("manufacturer") == "Mtks", "the AU manufacturer is " << text("manufacturer")
                                                                           << ": PLUGIN_MANUFACTURER_CODE Mtks");
}
