#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <undertow/params.h>
#include <undertow/presets.h>

namespace {

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < undertow::kNumParams; ++i) {
        const auto& spec = undertow::paramSpec(i);
        const juce::ParameterID pid{spec.id, 1};
        const juce::String unit{spec.unit != nullptr ? spec.unit : ""};

        switch (spec.kind) {
        case undertow::ParamKind::Float: {
            auto range = juce::NormalisableRange<float>(spec.min, spec.max);
            if (spec.centre != 0.0f)
                range.setSkewForCentre(spec.centre);
            layout.add(std::make_unique<juce::AudioParameterFloat>(pid, spec.name, range, spec.def, unit));
            break;
        }
        case undertow::ParamKind::Int:
            layout.add(std::make_unique<juce::AudioParameterInt>(pid, spec.name, static_cast<int>(spec.min),
                                                                 static_cast<int>(spec.max), static_cast<int>(spec.def), unit));
            break;
        case undertow::ParamKind::Choice: {
            juce::StringArray choices;
            for (int c = 0; c < spec.numChoices; ++c)
                choices.add(spec.choices[c]);
            layout.add(std::make_unique<juce::AudioParameterChoice>(pid, spec.name, choices, static_cast<int>(spec.def),
                                                                    unit));
            break;
        }
        case undertow::ParamKind::Bool:
            layout.add(std::make_unique<juce::AudioParameterBool>(pid, spec.name, spec.def >= 0.5f, unit));
            break;
        }
    }

    return layout;
}

} // namespace

UndertowProcessor::UndertowProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "Parameters", createParameterLayout()) {}

undertow::Engine& UndertowProcessor::engine() { return engine_; }
juce::AudioProcessorValueTreeState& UndertowProcessor::parameters() { return apvts_; }

const juce::String UndertowProcessor::getName() const { return JucePlugin_Name; }
bool UndertowProcessor::acceptsMidi() const { return true; }
bool UndertowProcessor::producesMidi() const { return false; }
bool UndertowProcessor::isMidiEffect() const { return false; }
double UndertowProcessor::getTailLengthSeconds() const { return 8.0; }

int UndertowProcessor::getNumPrograms() { return undertow::numFactoryPresets(); }
int UndertowProcessor::getCurrentProgram() { return currentProgram_; }

void UndertowProcessor::setCurrentProgram(int index) {
    if (index < 0 || index >= getNumPrograms())
        return;

    float values[undertow::kNumParams]{};
    if (!undertow::factoryPreset(index, values))
        return;

    for (int i = 0; i < undertow::kNumParams; ++i) {
        auto* param = apvts_.getParameter(undertow::paramSpec(i).id);
        if (param != nullptr)
            param->setValueNotifyingHost(param->convertTo0to1(values[static_cast<std::size_t>(i)]));
    }

    currentProgram_ = index;
}

const juce::String UndertowProcessor::getProgramName(int index) {
    if (index < 0 || index >= getNumPrograms())
        return {};
    return juce::String(undertow::factoryPresetName(index));
}

void UndertowProcessor::changeProgramName(int, const juce::String&) {}

void UndertowProcessor::updateParametersFromState(const juce::ValueTree& state) {
    for (int i = 0; i < undertow::kNumParams; ++i) {
        const auto& spec = undertow::paramSpec(i);
        if (!state.hasProperty(spec.id))
            continue;

        auto* param = apvts_.getParameter(spec.id);
        if (param == nullptr)
            continue;

        const float target = static_cast<float>(state.getProperty(spec.id));
        const float target01 = param->convertTo0to1(target);
        if (param->getValue() != target01)
            param->setValueNotifyingHost(target01);
    }
}

void UndertowProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    engine_.prepare(sampleRate, samplesPerBlock);
    midiEvents_.reserve(256);
}

void UndertowProcessor::releaseResources() {}

bool UndertowProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.inputBuses.size() == 0 && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
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
        const auto& msg = metadata.getMessage();
        const auto* raw = msg.getRawData();
        midiEvents_.push_back({metadata.samplePosition, raw[0], raw[1], raw[2]});
    }

    undertow::Transport transport;
    if (auto* playhead = getPlayHead()) {
        if (auto position = playhead->getPosition()) {
            if (auto ppq = position->getPpqPosition()) {
                transport.valid = true;
                transport.playing = position->getIsPlaying();
                transport.bpm = position->getBpm().orFallback(120.0);
                transport.ppq = *ppq;
            }
        }
    }

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);
    engine_.process(left, right, buffer.getNumSamples(), midiEvents_.data(), static_cast<int>(midiEvents_.size()),
                    transport);
}

juce::AudioProcessorEditor* UndertowProcessor::createEditor() { return new UndertowEditor(*this); }
bool UndertowProcessor::hasEditor() const { return true; }

void UndertowProcessor::getStateInformation(juce::MemoryBlock& destData) {
    if (auto xml = apvts_.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void UndertowProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes)) {
        auto state = juce::ValueTree::fromXml(*xml);
        if (!state.isValid())
            return;

        apvts_.replaceState(state);
        updateParametersFromState(apvts_.state);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new UndertowProcessor(); }
