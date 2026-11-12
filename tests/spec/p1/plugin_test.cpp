// SPDX-License-Identifier: AGPL-3.0-only
// Phase 1 spec tests: the JUCE plugin, re-keyed (docs/specs/phase-1-foundation.md, checks P1.10 to P1.17).
// spec-preset: plugin
// spec-needs: plugin/PluginProcessor.h
// spec-needs: plugin/PluginEditor.h

#include "doctest.h"

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "spec_alloc.h"
#include "spec_paths.h"
#include "undertow/params.h"
#include "undertow/presets.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
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

// A float to its last place: its value and its bits.
std::string exact(float f) {
    std::uint32_t bits;
    std::memcpy(&bits, &f, sizeof bits);
    char text[48];
    std::snprintf(text, sizeof text, "%.9g (0x%08x)", static_cast<double>(f), static_cast<unsigned>(bits));
    return text;
}

// A value parameter `i` can hold, spread over its range by `k`, in real units.
float sampleValue(int i, int k) {
    const auto& spec = undertow::paramSpec(i);
    float value = spec.min + (spec.max - spec.min) * static_cast<float>((i * 37 + k * 11) % 100) / 100.0f;
    if (spec.kind != undertow::ParamKind::Float)
        value = std::round(value);
    return value;
}

} // namespace

TEST_CASE("P1.10 the processor is the Metakosmia Undertow 2 synth: MIDI in, one stereo output, no inputs") {
    juce::ScopedJuceInitialiser_GUI gui;
    auto processor = makeProcessor();
    REQUIRE_MESSAGE(processor != nullptr, "createPluginFilter() returned null");
    CHECK_MESSAGE(dynamic_cast<UndertowProcessor*>(processor.get()) != nullptr,
                  "createPluginFilter() must return an UndertowProcessor (plugin/PluginProcessor.h)");
    CHECK_MESSAGE(processor->getName() == "Metakosmia Undertow 2",
                  "getName() is \"" << processor->getName() << "\": PRODUCT_NAME must be \"Metakosmia Undertow 2\"");
    CHECK_MESSAGE(processor->acceptsMidi(), "the synth must accept MIDI (NEEDS_MIDI_INPUT TRUE)");
    CHECK_MESSAGE(!processor->producesMidi(), "the synth must not produce MIDI");
    CHECK_MESSAGE(!processor->isMidiEffect(), "the synth is not a MIDI effect");
    CHECK_MESSAGE(processor->getBusCount(true) == 0, "a synth has no input bus; found " << processor->getBusCount(true));
    REQUIRE_MESSAGE(processor->getBusCount(false) == 1, "the synth has exactly one output bus");
    CHECK_MESSAGE(processor->getChannelCountOfBus(false, 0) == 2, "the output bus is stereo by default");
    juce::AudioProcessor::BusesLayout stereo, mono;
    stereo.outputBuses.add(juce::AudioChannelSet::stereo());
    mono.outputBuses.add(juce::AudioChannelSet::mono());
    CHECK_MESSAGE(processor->checkBusesLayoutSupported(stereo),
                  "isBusesLayoutSupported must accept one stereo output and no input");
    CHECK_MESSAGE(!processor->checkBusesLayoutSupported(mono),
                  "isBusesLayoutSupported must refuse a mono output: stereo is the only layout (processBlock writes "
                  "two channels)");
    CHECK_MESSAGE(processor->getTailLengthSeconds() == doctest::Approx(8.0),
                  "getTailLengthSeconds() must be 8, the rumble's longest decay; got "
                      << processor->getTailLengthSeconds());
}

TEST_CASE("P1.11 the plugin exposes the 49 parameters of the core table, in order, with the same ranges") {
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
        CHECK_MESSAGE(parameter->getParameterID() == juce::String(spec.id), "its id is `" << parameter->getParameterID()
                                                                                         << "`");
        CHECK_MESSAGE(parameter->getName(100) == juce::String(spec.name), "its name is \"" << parameter->getName(100)
                                                                                          << "\"");
        const auto& range = parameter->getNormalisableRange();
        CHECK_MESSAGE(range.start == doctest::Approx(spec.min), "its range starts at " << range.start);
        CHECK_MESSAGE(range.end == doctest::Approx(spec.max), "its range ends at " << range.end);
        CHECK_MESSAGE(parameter->convertFrom0to1(parameter->getDefaultValue()) == doctest::Approx(spec.def).epsilon(1e-4),
                      "its default is " << parameter->convertFrom0to1(parameter->getDefaultValue()));
        switch (spec.kind) {
        case undertow::ParamKind::Float:
            CHECK_MESSAGE(dynamic_cast<juce::AudioParameterFloat*>(parameter) != nullptr,
                          "a Float must be an AudioParameterFloat");
            if (spec.centre != 0.0f)
                CHECK_MESSAGE(range.convertFrom0to1(0.5f) == doctest::Approx(spec.centre).epsilon(0.01),
                              "half travel must land on the table's centre "
                                  << spec.centre << " (setSkewForCentre); got " << range.convertFrom0to1(0.5f));
            else
                CHECK_MESSAGE(range.skew == doctest::Approx(1.0f), "a centre of 0 is a linear range; its skew is "
                                                                       << range.skew);
            break;
        case undertow::ParamKind::Int:
            CHECK_MESSAGE(dynamic_cast<juce::AudioParameterInt*>(parameter) != nullptr,
                          "an Int must be an AudioParameterInt");
            break;
        case undertow::ParamKind::Bool:
            CHECK_MESSAGE(dynamic_cast<juce::AudioParameterBool*>(parameter) != nullptr,
                          "a Bool must be an AudioParameterBool");
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

TEST_CASE("P1.12 state saves and restores every parameter, into a new processor or over moved values, and holds "
          "no pattern") {
    juce::ScopedJuceInitialiser_GUI gui;
    auto source = makeProcessor();
    std::vector<float> values;
    for (int i = 0; i < undertow::kNumParams; ++i) {
        values.push_back(sampleValue(i, 0));
        auto* parameter = ranged(*source, i);
        REQUIRE(parameter != nullptr);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(values.back()));
    }
    juce::MemoryBlock state;
    source->getStateInformation(state);
    REQUIRE_MESSAGE(state.getSize() > 0, "getStateInformation wrote nothing");
    auto xml = juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
    REQUIRE_MESSAGE(xml != nullptr, "the state must be XML (copyXmlToBinary)");
    CHECK_MESSAGE(xml->getChildByName("PATTERN") == nullptr,
                  "the state still holds a PATTERN element: the steps are parameters now, saved with the rest");

    auto restored = makeProcessor();
    restored->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    for (int i = 0; i < undertow::kNumParams; ++i) {
        auto* parameter = ranged(*restored, i);
        REQUIRE(parameter != nullptr);
        const float got = parameter->convertFrom0to1(parameter->getValue());
        const float want = values[static_cast<std::size_t>(i)];
        CHECK_MESSAGE(got == doctest::Approx(want).epsilon(1e-4),
                      "parameter `" << std::string(undertow::paramSpec(i).id) << "` came back as " << got
                                    << ", saved as " << want
                                    << ": setStateInformation(getStateInformation()) must restore every parameter");
    }

    // A session's state loaded into a processor whose parameters a host has already moved: every parameter must end up
    // at the state's value, and the engine must play it from the next block.
    auto defaultsSource = makeProcessor();
    juce::MemoryBlock defaultState;
    defaultsSource->getStateInformation(defaultState);
    auto moved = makeProcessor();
    for (int i = 0; i < undertow::kNumParams; ++i)
        ranged(*moved, i)->setValueNotifyingHost(ranged(*moved, i)->convertTo0to1(sampleValue(i, 5)));
    moved->setStateInformation(defaultState.getData(), static_cast<int>(defaultState.getSize()));
    moved->setPlayConfigDetails(0, 2, 48000.0, 256);
    moved->prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    moved->processBlock(buffer, midi);
    auto* engineOwner = dynamic_cast<UndertowProcessor*>(moved.get());
    REQUIRE(engineOwner != nullptr);
    for (int i = 0; i < undertow::kNumParams; ++i) {
        const auto& spec = undertow::paramSpec(i);
        auto* parameter = ranged(*moved, i);
        CHECK_MESSAGE(parameter->convertFrom0to1(parameter->getValue()) == doctest::Approx(spec.def).epsilon(1e-4),
                      "`" << std::string(spec.id) << "` is " << parameter->convertFrom0to1(parameter->getValue())
                          << " after loading a state that holds its default " << spec.def);
        CHECK_MESSAGE(engineOwner->engine().getParam(i) == doctest::Approx(spec.def).epsilon(1e-4),
                      "after loading the state and one block, the engine's `" << std::string(spec.id) << "` is "
                                                                              << engineOwner->engine().getParam(i)
                                                                              << "; the state holds " << spec.def);
    }
    moved->releaseResources();
}

TEST_CASE("P1.13 processBlock is finite, mono, never allocates, and hands host parameter changes to the engine") {
    juce::ScopedJuceInitialiser_GUI gui;
    auto processor = makeProcessor();
    auto* undertowProcessor = dynamic_cast<UndertowProcessor*>(processor.get());
    REQUIRE(undertowProcessor != nullptr);
    processor->setPlayConfigDetails(0, 2, 48000.0, 512);
    processor->prepareToPlay(48000.0, 512);

    juce::AudioBuffer<float> buffer(2, 512);
    std::vector<juce::MidiBuffer> midi(4);
    midi[0].addEvent(juce::MidiMessage::noteOn(1, 33, static_cast<juce::uint8>(100)), 0);
    midi[1].addEvent(juce::MidiMessage::controllerEvent(1, 1, 64), 100);
    midi[2].addEvent(juce::MidiMessage::noteOff(1, 33), 300);
    midi[3].addEvent(juce::MidiMessage::allNotesOff(1), 511);

    const int cutoff = undertow::param::VcfCutoff;
    auto* cutoffParameter = ranged(*processor, cutoff);
    REQUIRE(cutoffParameter != nullptr);
    cutoffParameter->setValueNotifyingHost(0.25f);
    const int step3 = undertow::param::Step3Vel;
    auto* stepParameter = ranged(*processor, step3);
    REQUIRE(stepParameter != nullptr);
    stepParameter->setValueNotifyingHost(0.4f);

    std::size_t allocations = 0;
    int nonFinite = 0, different = 0;
    double energy = 0;
    for (int block = 0; block < 200; ++block) {
        for (int ch = 0; ch < 2; ++ch)
            juce::FloatVectorOperations::fill(buffer.getWritePointer(ch), std::nanf(""), 512);
        auto& events = midi[static_cast<std::size_t>(block % 4)];
        spec::AllocationCounter counter;
        processor->processBlock(buffer, events);
        allocations += counter.count();
        for (int n = 0; n < 512; ++n) {
            const float l = buffer.getSample(0, n), r = buffer.getSample(1, n);
            nonFinite += (std::isfinite(l) ? 0 : 1) + (std::isfinite(r) ? 0 : 1);
            different += l == r ? 0 : 1;
            energy += static_cast<double>(l) * l;
        }
    }
    CHECK_MESSAGE(nonFinite == 0, nonFinite << " output samples were not finite after processBlock: write every sample "
                                               "of both channels, and never output NaN or infinity");
    CHECK_MESSAGE(different == 0, different << " samples differ between the two channels: the instrument is mono");
    CHECK_MESSAGE(allocations == 0, allocations << " heap allocations inside processBlock: preallocate the MIDI event "
                                                   "array and everything else in prepareToPlay");
    CHECK_MESSAGE(undertowProcessor->engine().getParam(cutoff) ==
                      doctest::Approx(cutoffParameter->convertFrom0to1(0.25f)).epsilon(1e-4),
                  "after processBlock the engine's vcf_cutoff must equal the host value "
                      << cutoffParameter->convertFrom0to1(0.25f) << "; got " << undertowProcessor->engine().getParam(cutoff)
                      << ": processBlock must pass every parameter to engine().setParam");
    CHECK_MESSAGE(undertowProcessor->engine().getParam(step3) == doctest::Approx(0.4f).epsilon(1e-4),
                  "after processBlock the engine's step3_vel must equal the host value 0.4; got "
                      << undertowProcessor->engine().getParam(step3));
    CHECK_MESSAGE(energy > 0.0, "200 blocks without a host position (the engine's own clock) must carry the rumble's "
                                "ghost kicks; the output was silent");
    processor->releaseResources();
}

TEST_CASE("P1.14 program 0 is Init, which sets every parameter to its default; the programs are the factory presets") {
    juce::ScopedJuceInitialiser_GUI gui;
    auto processor = makeProcessor();
    REQUIRE_MESSAGE(processor->getNumPrograms() >= 1, "getNumPrograms() is " << processor->getNumPrograms()
                                                                             << "; program 0 is Init");
    CHECK_MESSAGE(processor->getNumPrograms() == undertow::numFactoryPresets(),
                  "getNumPrograms() is " << processor->getNumPrograms() << " but there are "
                                         << undertow::numFactoryPresets() << " factory presets: one program each");
    CHECK_MESSAGE(processor->getProgramName(0) == "Init", "program 0 is named \"" << processor->getProgramName(0)
                                                                                  << "\"; it must be \"Init\"");
    CHECK_MESSAGE(std::string(undertow::factoryPresetSlug(0)) == "init", "factory preset 0's slug must be \"init\"");
    CHECK_MESSAGE(undertow::findFactoryPreset("init") == 0, "findFactoryPreset(\"init\") must find program 0");
    CHECK_MESSAGE(undertow::findFactoryPreset("Init") == 0, "findFactoryPreset(\"Init\") must find program 0");
    CHECK_MESSAGE(undertow::findFactoryPreset("no-such-preset") == -1, "an unknown preset must give -1");
    float values[undertow::kNumParams]{};
    CHECK_MESSAGE(!undertow::factoryPreset(undertow::numFactoryPresets(), values),
                  "factoryPreset past the last preset must return false");
    CHECK_MESSAGE(!undertow::factoryPreset(-1, values), "factoryPreset(-1) must return false");
    CHECK_MESSAGE(std::string(undertow::factoryPresetName(-1)).empty(), "factoryPresetName(-1) must be \"\"");
    REQUIRE(undertow::factoryPreset(0, values));
    for (int i = 0; i < undertow::kNumParams; ++i)
        CHECK_MESSAGE(values[i] == undertow::paramSpec(i).def, "Init's `" << std::string(undertow::paramSpec(i).id)
                                                                          << "` is " << values[i]
                                                                          << "; Init is every parameter's default");
    for (int i = 0; i < undertow::kNumParams; ++i) {
        auto* parameter = ranged(*processor, i);
        REQUIRE(parameter != nullptr);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(sampleValue(i, 3)));
    }
    processor->setCurrentProgram(0);
    for (int i = 0; i < undertow::kNumParams; ++i) {
        const auto& spec = undertow::paramSpec(i);
        auto* parameter = ranged(*processor, i);
        CHECK_MESSAGE(parameter->convertFrom0to1(parameter->getValue()) == doctest::Approx(spec.def).epsilon(1e-4),
                      "after setCurrentProgram(0) `" << std::string(spec.id) << "` is "
                                                     << parameter->convertFrom0to1(parameter->getValue())
                                                     << "; Init sets it to its default " << spec.def);
    }
    CHECK_MESSAGE(processor->getCurrentProgram() == 0, "getCurrentProgram() must be 0 after setCurrentProgram(0)");
}

TEST_CASE("P1.15 the window is a fixed 1200 x 760 dark ROSEDUST panel") {
    juce::ScopedJuceInitialiser_GUI gui;
    auto processor = makeProcessor();
    REQUIRE_MESSAGE(processor->hasEditor(), "hasEditor() must be true");
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditor());
    REQUIRE(editor != nullptr);
    CHECK_MESSAGE(dynamic_cast<UndertowEditor*>(editor.get()) != nullptr,
                  "createEditor() must return an UndertowEditor (plugin/PluginEditor.h)");
    CHECK_MESSAGE(editor->getWidth() == 1200, "the window is " << editor->getWidth() << " wide; it must be 1200");
    CHECK_MESSAGE(editor->getHeight() == 760, "the window is " << editor->getHeight() << " high; it must be 760");
    CHECK_MESSAGE(!editor->isResizable(), "the window has a fixed size: setResizable(false, false)");
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
    CHECK_MESSAGE(sum / pixelsTotal < 0.12, "the window's mean luminance is " << sum / pixelsTotal
                                                                              << ": paint the bg_void ground (#060608)");
    CHECK_MESSAGE(dark / pixelsTotal >= 0.5, "only " << 100.0 * dark / pixelsTotal << "% of the window is near-black void");
    CHECK_MESSAGE(white == 0, white << " pure white pixels: ROSEDUST never uses white (the brightest colour is "
                                       "rose_bright)");
    editor.reset();
}

TEST_CASE("P1.16 the build makes VST3, AU and Standalone bundles with Undertow 2's identifiers, and installs them") {
    juce::ScopedJuceInitialiser_GUI gui;
    // This checkout's bundles (spec_paths.h).
    const juce::File vst3{juce::String(spec::built(UNDERTOW_VST3_BUNDLE_IN_BUILD, UNDERTOW_VST3_BUNDLE))},
        au{juce::String(spec::built(UNDERTOW_AU_BUNDLE_IN_BUILD, UNDERTOW_AU_BUNDLE))},
        app{juce::String(spec::built(UNDERTOW_STANDALONE_BUNDLE_IN_BUILD, UNDERTOW_STANDALONE_BUNDLE))};
    CHECK_MESSAGE(vst3.getFileName() == "Metakosmia Undertow 2.vst3", "the VST3 bundle is " << vst3.getFullPathName());
    CHECK_MESSAGE(au.getFileName() == "Metakosmia Undertow 2.component", "the AU bundle is " << au.getFullPathName());
    CHECK_MESSAGE(app.getFileName() == "Metakosmia Undertow 2.app", "the Standalone bundle is " << app.getFullPathName());
    for (const auto& bundle : {vst3, au, app}) {
        INFO("bundle " << bundle.getFullPathName());
        REQUIRE_MESSAGE(bundle.isDirectory(), "the bundle was not built");
        auto plist = readPlist(bundle);
        REQUIRE_MESSAGE(plist != nullptr, "Contents/Info.plist is missing or not an XML plist");
        auto* identifier = plistValue(*plist, "CFBundleIdentifier");
        REQUIRE(identifier != nullptr);
        CHECK_MESSAGE(identifier->getAllSubText() == "audio.metakosmia.undertow2",
                      "CFBundleIdentifier is " << identifier->getAllSubText()
                                               << "; BUNDLE_ID must be audio.metakosmia.undertow2");
        auto* version = plistValue(*plist, "CFBundleShortVersionString");
        REQUIRE(version != nullptr);
        CHECK_MESSAGE(version->getAllSubText() == "2.0.0",
                      "CFBundleShortVersionString is " << version->getAllSubText() << "; VERSION must be 2.0.0");
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
    CHECK_MESSAGE(text("subtype") == "MkU2", "the AU subtype is " << text("subtype") << ": PLUGIN_CODE MkU2");
    CHECK_MESSAGE(text("manufacturer") == "Mtks", "the AU manufacturer is " << text("manufacturer")
                                                                           << ": PLUGIN_MANUFACTURER_CODE Mtks");

    // tools/install.sh installs these bundles, under Undertow 2's names, into a test destination. A build tree can
    // still hold the first version's bundles, so the installed copies must be the ones just checked.
    const juce::File dest{juce::String(spec::sourceDir() + "/build/spec-install")};
    dest.deleteRecursively(); // an earlier run's copies must not stand in for this one's
    const std::string command = "cd '" + spec::sourceDir() + "' && tools/install.sh --dest build/spec-install 2>&1";
    FILE* pipe = popen(command.c_str(), "r");
    REQUIRE_MESSAGE(pipe != nullptr, "cannot run tools/install.sh");
    std::string output;
    char line[512];
    while (fgets(line, sizeof line, pipe) != nullptr)
        output += line;
    const int status = pclose(pipe);
    REQUIRE_MESSAGE(status == 0, "tools/install.sh --dest build/spec-install failed: " << output);
    for (const char* installed : {"VST3/Metakosmia Undertow 2.vst3", "Components/Metakosmia Undertow 2.component",
                                  "Applications/Metakosmia Undertow 2.app"}) {
        const auto bundle = dest.getChildFile(installed);
        INFO("installed bundle " << bundle.getFullPathName());
        REQUIRE_MESSAGE(bundle.isDirectory(), "tools/install.sh did not install it: it installs Undertow 2's three "
                                              "bundles, `Metakosmia Undertow 2.vst3`, `.component` and `.app`");
        auto installedPlist = readPlist(bundle);
        REQUIRE(installedPlist != nullptr);
        auto* installedId = plistValue(*installedPlist, "CFBundleIdentifier");
        REQUIRE(installedId != nullptr);
        CHECK_MESSAGE(installedId->getAllSubText() == "audio.metakosmia.undertow2",
                      "the installed bundle's CFBundleIdentifier is " << installedId->getAllSubText());
    }
}

TEST_CASE("P1.17 every float parameter comes back as its default, set by a host from either end of its range") {
    juce::ScopedJuceInitialiser_GUI gui;
    auto processor = makeProcessor();
    int floats = 0;
    for (int i = 0; i < undertow::kNumParams; ++i) {
        const auto& spec = undertow::paramSpec(i);
        if (spec.kind != undertow::ParamKind::Float)
            continue;
        ++floats;
        auto* parameter = ranged(*processor, i);
        REQUIRE(parameter != nullptr);
        const float def = parameter->getDefaultValue();
        for (const float from : {1.0f, 0.0f}) {
            parameter->setValueNotifyingHost(from);
            parameter->setValueNotifyingHost(def);
            const float back = parameter->getValue();
            CHECK_MESSAGE(back == def, std::string(spec.id)
                                           << ", set to " << from << " and then to its default " << exact(def)
                                           << ", reads back " << exact(back)
                                           << ". auval compares exactly and warns \"Parameter did not retain default "
                                              "value when set\": give this parameter's range a fine grid "
                                              "(range.interval) that its default and both ends lie on");
        }
        const float shown = parameter->convertFrom0to1(def);
        CHECK_MESSAGE(std::abs(shown - spec.def) <= 1e-6f * (spec.max - spec.min),
                      std::string(spec.id) << "'s default reaches the host as " << shown << "; the table says "
                                           << spec.def << ": a grid must keep the default");
    }
    CHECK_MESSAGE(floats >= 25, "the parameter table holds 25 or more float parameters; found " << floats);
}
