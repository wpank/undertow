#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <undertow/engine.h>

class UndertowProcessor final : public juce::AudioProcessor {
public:
    UndertowProcessor();

    undertow::Engine& engine();
    juce::AudioProcessorValueTreeState& parameters();
    void setPattern(const undertow::Pattern& pattern);
    undertow::Pattern pattern() const;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

private:
    juce::AudioProcessorValueTreeState apvts_;
    undertow::Engine engine_;
    std::vector<undertow::MidiEvent> midiEvents_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UndertowProcessor)
};
