// Phase 4 spec tests: the editor skeleton: frames and controls (docs/specs/phase-4-ui.md, P4.03 and P4.04).
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

TEST_CASE("P4.03 each frame the editor reads the processor: meters, pitch, step, clock and scope") {
    juce::ScopedJuceInitialiser_GUI gui;
    std::unique_ptr<juce::AudioProcessor> processor(createPluginFilter());
    processor->prepareToPlay(48000.0, 512);
    juce::AudioBuffer<float> buffer(2, 512);
    for (int block = 0; block < 47; ++block) {
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent(juce::MidiMessage::noteOn(1, 45, static_cast<juce::uint8>(90)), 0);
        processor->processBlock(buffer, midi);
    }
    std::unique_ptr<UndertowEditor> editor(dynamic_cast<UndertowEditor*>(processor->createEditor()));
    REQUIRE(editor != nullptr);
    const UiFrame frame = editor->captureFrame();
    CHECK_MESSAGE(frame.note == 45, "the frame's note is " << frame.note << " while note 45 plays");
    CHECK_MESSAGE(std::fabs(frame.noteHz - 110.0f) <= 0.5f, "the frame's pitch is " << frame.noteHz << " Hz; note 45 is 110 Hz");
    CHECK_MESSAGE(frame.rms > 0.01f, "the frame's RMS must follow the output; it is " << frame.rms);
    CHECK_MESSAGE(frame.energy > 0.0f, "the frame's energy must follow the output");
    float loudest = 0.0f;
    for (float v : frame.scope)
        loudest = std::max(loudest, std::fabs(v));
    CHECK_MESSAGE(loudest > 0.01f, "the frame's scope must hold the latest output samples (from Engine::copyScope)");
    CHECK_MESSAGE(!frame.hostClock, "with no host play head the clock is the internal one");
    CHECK_MESSAGE(std::fabs(frame.bpm - 128.0) <= 0.01, "the internal clock runs at seq_bpm (128); the frame says " << frame.bpm);
    CHECK_MESSAGE(frame.length == 16, "the frame's length is seq_length (16)");
    editor.reset();
}


TEST_CASE("P4.04 every parameter has its control on the editor, attached to it") {
    Fixture f;
    for (int i = 0; i < undertow::kNumParams; ++i) {
        const auto& spec = undertow::paramSpec(i);
        INFO("parameter `" << std::string(spec.id) << "`");
        auto* control = f.editor->controlFor(spec.id);
        REQUIRE_MESSAGE(control != nullptr, "controlFor must find the control of every parameter");
        auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(f.processor->getParameters()[i]);
        REQUIRE(parameter != nullptr);
        if (spec.kind == undertow::ParamKind::Bool) {
            auto* button = dynamic_cast<juce::Button*>(control);
            REQUIRE_MESSAGE(button != nullptr, "a Bool parameter's control must be a juce::Button");
            button->setToggleState(true, juce::sendNotificationSync);
            CHECK_MESSAGE(parameter->getValue() == doctest::Approx(1.0f), "switching the button on must set the parameter");
            button->setToggleState(false, juce::sendNotificationSync);
        } else {
            auto* slider = dynamic_cast<juce::Slider*>(control);
            REQUIRE_MESSAGE(slider != nullptr, "the control must be a juce::Slider (a knob) attached to the parameter");
            slider->setValue(slider->getMaximum(), juce::sendNotificationSync);
            CHECK_MESSAGE(parameter->convertFrom0to1(parameter->getValue()) == doctest::Approx(spec.max).epsilon(1e-3),
                          "turning the knob to its end must set the parameter to its maximum (" << spec.max << ")");
            slider->setValue(spec.def, juce::sendNotificationSync);
        }
    }
}

