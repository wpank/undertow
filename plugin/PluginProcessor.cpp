#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <undertow/params.h>
#include <undertow/presets.h>

namespace {

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < undertow::kNumParams; ++i) {
        const auto& spec = undertow::paramSpec(i);
        juce::ParameterID pid{spec.id, 1};

        switch (spec.kind) {
        case undertow::ParamKind::Float: {
            auto range = juce::NormalisableRange<float>(spec.min, spec.max);
            if (spec.centre != 0.0f)
                range.setSkewForCentre(spec.centre);
            layout.add(std::make_unique<juce::AudioParameterFloat>(pid, spec.name, range, spec.def, spec.unit));
            break;
        }
        case undertow::ParamKind::Int:
            layout.add(std::make_unique<juce::AudioParameterInt>(pid, spec.name, static_cast<int>(spec.min), static_cast<int>(spec.max), static_cast<int>(spec.def), spec.unit));
            break;
        case undertow::ParamKind::Choice: {
            juce::StringArray choices;
            for (int c = 0; c < spec.numChoices; ++c)
                choices.add(juce::String(spec.choices[c]));
            layout.add(std::make_unique<juce::AudioParameterChoice>(pid, spec.name, choices, static_cast<int>(spec.def), spec.unit));
            break;
        }
        case undertow::ParamKind::Bool:
            layout.add(std::make_unique<juce::AudioParameterBool>(pid, spec.name, spec.def >= 0.5f, spec.unit));
            break;
        }
    }

    return layout;
}

void writePatternToXml(juce::XmlElement& root, const undertow::Pattern& pattern) {
    auto* patternXml = root.createNewChildElement("PATTERN");
    for (int i = 0; i < undertow::kSteps; ++i) {
        const auto& step = pattern.steps[static_cast<std::size_t>(i)];
        auto* stepXml = patternXml->createNewChildElement("STEP");
        stepXml->setAttribute("index", i);
        stepXml->setAttribute("semitones", step.semitones);
        stepXml->setAttribute("gate", step.gate ? 1 : 0);
        stepXml->setAttribute("accent", step.accent ? 1 : 0);
        stepXml->setAttribute("slide", step.slide ? 1 : 0);
        stepXml->setAttribute("probability", step.probability);
    }
}

bool readPatternFromXml(const juce::XmlElement& root, undertow::Pattern& pattern) {
    if (auto* patternXml = root.getChildByName("PATTERN")) {
        undertow::Pattern parsed = undertow::defaultPattern();
        forEachXmlChildElement(*patternXml, stepXml) {
            if (stepXml->hasTagName("STEP")) {
                const int index = stepXml->getIntAttribute("index", -1);
                if (index >= 0 && index < undertow::kSteps) {
                    auto& step = parsed.steps[static_cast<std::size_t>(index)];
                    step.semitones = stepXml->getIntAttribute("semitones", step.semitones);
                    step.gate = stepXml->getBoolAttribute("gate", step.gate);
                    step.accent = stepXml->getBoolAttribute("accent", step.accent);
                    step.slide = stepXml->getBoolAttribute("slide", step.slide);
                    step.probability = static_cast<float>(stepXml->getDoubleAttribute("probability", step.probability));
                }
            }
        }
        pattern = parsed;
        return true;
    }
    return false;
}

} // namespace

UndertowProcessor::UndertowProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "Parameters", createParameterLayout()) {}

undertow::Engine& UndertowProcessor::engine() { return engine_; }
juce::AudioProcessorValueTreeState& UndertowProcessor::parameters() { return apvts_; }
void UndertowProcessor::setPattern(const undertow::Pattern& pattern) { engine_.setPattern(pattern); }
undertow::Pattern UndertowProcessor::pattern() const { return engine_.pattern(); }

void UndertowProcessor::updateParametersFromState(const juce::ValueTree& state) {
    for (int i = 0; i < undertow::kNumParams; ++i) {
        const auto& spec = undertow::paramSpec(i);
        if (auto* param = apvts_.getParameter(spec.id)) {
            if (auto* property = state.getPropertyPointer(spec.id))
                param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(property->isBool() ? (bool) *property : (double) *property)));
            else if (auto* value = apvts_.getRawParameterValue(spec.id))
                param->setValueNotifyingHost(param->convertTo0to1(value->load()));
        }
    }
}
const juce::String UndertowProcessor::getName() const { return JucePlugin_Name; }
bool UndertowProcessor::acceptsMidi() const { return true; }
bool UndertowProcessor::producesMidi() const { return false; }
bool UndertowProcessor::isMidiEffect() const { return false; }
double UndertowProcessor::getTailLengthSeconds() const { return 8.0; }
int UndertowProcessor::getNumPrograms() { return undertow::numFactoryPresets(); }
int UndertowProcessor::getCurrentProgram() { return static_cast<int>(apvts_.state.getProperty("currentProgram", 0)); }
void UndertowProcessor::setCurrentProgram(int index) {
    const auto programCount = getNumPrograms();
    if (index < 0 || index >= programCount)
        return;

    float values[undertow::kNumParams]{};
    undertow::Pattern presetPattern = undertow::defaultPattern();
    if (!undertow::factoryPreset(index, values, &presetPattern))
        return;

    for (int i = 0; i < undertow::kNumParams; ++i) {
        auto* param = apvts_.getParameter(undertow::paramSpec(i).id);
        if (param != nullptr)
            param->setValueNotifyingHost(param->convertTo0to1(values[static_cast<std::size_t>(i)]));
    }
    engine_.setPattern(presetPattern);
    apvts_.state.setProperty("currentProgram", index, nullptr);
}
const juce::String UndertowProcessor::getProgramName(int index) {
    if (index < 0 || index >= getNumPrograms())
        return {};
    return juce::String(undertow::factoryPresetName(index));
}
void UndertowProcessor::changeProgramName(int, const juce::String&) {}

void UndertowProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    engine_.prepare(sampleRate, samplesPerBlock);
    midiEvents_.reserve(256);
}

void UndertowProcessor::releaseResources() {}

bool UndertowProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.inputBuses.size() == 0;
}

void UndertowProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    for (int i = 0; i < undertow::kNumParams; ++i) {
        auto* param = apvts_.getRawParameterValue(undertow::paramSpec(i).id);
        if (param != nullptr)
            engine_.setParam(i, param->load());
    }
    midiEvents_.clear();
    for (const auto metadata : midi) {
        if (midiEvents_.size() >= midiEvents_.capacity())
            break;
        const auto& msg = metadata.getMessage();
        midiEvents_.push_back({metadata.samplePosition, msg.getRawData()[0], msg.getRawData()[1], msg.getRawData()[2]});
    }
    undertow::Transport transport;
    if (auto* playhead = getPlayHead()) {
        if (auto position = playhead->getPosition()) {
            if (auto ppq = position->getPpqPosition()) {
                transport.valid = true;
                transport.ppq = *ppq;
                transport.bpm = position->getBpm().orFallback(120.0);
                transport.playing = position->getIsPlaying();
            }
        }
    }
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);
    engine_.process(left, right, buffer.getNumSamples(), midiEvents_.data(), static_cast<int>(midiEvents_.size()), transport);
}

juce::AudioProcessorEditor* UndertowProcessor::createEditor() { return new UndertowEditor(*this); }
bool UndertowProcessor::hasEditor() const { return true; }

void UndertowProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts_.copyState();
    if (auto xml = state.createXml()) {
        writePatternToXml(*xml, engine_.pattern());
        copyXmlToBinary(*xml, destData);
    }
}

void UndertowProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes)) {
        auto state = juce::ValueTree::fromXml(*xml);
        apvts_.replaceState(state);
        updateParametersFromState(apvts_.state);
        undertow::Pattern pattern = undertow::defaultPattern();
        if (!readPatternFromXml(*xml, pattern))
            pattern = undertow::defaultPattern();
        engine_.setPattern(pattern);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new UndertowProcessor(); }
