#include "PluginEditor.h"

#include "PluginProcessor.h"
#include "ui/Theme.h"

#include <undertow/params.h>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kUiScopeSamples = 2048;

float mapEnergy(float rms) {
    if (!(rms > 0.0f))
        return 0.0f;

    const auto db = juce::Decibels::gainToDecibels(rms, -100.0f);
    return juce::jlimit(0.0f, 1.0f, static_cast<float>((db + 48.0) / 42.0));
}

} // namespace

UndertowEditor::UndertowEditor(UndertowProcessor& p)
    : AudioProcessorEditor(p),
      processor_(p) {
    setSize(undertow::ui::theme::editorWidth, undertow::ui::theme::editorHeight);
    setResizable(false, false);

    unitArray_.setBounds(undertow::ui::theme::gutBounds);
    unitArray_.pattern = [this] { return processor_.pattern(); };
    unitArray_.onEdit = [this](int index, StepEdit edit) { editStep(index, edit); };
    addAndMakeVisible(unitArray_);

    initialiseControls();
    look_.install(*this);
    lastTimerTickMs_ = juce::Time::getMillisecondCounter();
    startTimerHz(60);
}

UndertowEditor::~UndertowEditor() {
    stopTimer();
    look_.uninstall(*this);
}

void UndertowEditor::initialiseControls() {
    auto addSlider = [this](int paramIndex, const juce::Rectangle<int>& bounds) {
        auto slider = std::make_unique<juce::Slider>();
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 18);
        slider->setName(undertow::paramSpec(paramIndex).id);
        slider->setBounds(bounds);
        addAndMakeVisible(*slider);
        sliderAttachments_[static_cast<std::size_t>(paramIndex)] = std::make_unique<SliderAttachment>(
            processor_.parameters(), undertow::paramSpec(paramIndex).id, *slider);
        sliders_[static_cast<std::size_t>(paramIndex)] = std::move(slider);
    };

    auto addButton = [this](int paramIndex, const juce::Rectangle<int>& bounds) {
        auto button = std::make_unique<juce::ToggleButton>();
        button->setName(undertow::paramSpec(paramIndex).id);
        button->setButtonText(undertow::paramSpec(paramIndex).name);
        button->setBounds(bounds);
        addAndMakeVisible(*button);
        buttonAttachments_[static_cast<std::size_t>(paramIndex)] = std::make_unique<ButtonAttachment>(
            processor_.parameters(), undertow::paramSpec(paramIndex).id, *button);
        buttons_[static_cast<std::size_t>(paramIndex)] = std::move(button);
    };

    for (const auto& panel : undertow::ui::theme::controlPanels) {
        for (int i = 0; i < panel.parameterCount; ++i) {
            const auto paramIndex = panel.parameters[static_cast<std::size_t>(i)];
            const auto cell = panel.cells[static_cast<std::size_t>(i)];
            if (paramIndex == undertow::param::SeqOn)
                addButton(paramIndex, cell.controlBounds);
            else
                addSlider(paramIndex, cell.controlBounds);
        }
    }
}

UiFrame UndertowEditor::captureFrame() const {
    UiFrame frame;
    const auto meters = processor_.engine().meters();

    frame.peak = meters.peak;
    frame.rms = meters.rms;
    frame.energy = mapEnergy(meters.rms);
    frame.clipping = meters.clipping;
    frame.note = meters.note;
    frame.noteHz = meters.noteHz;
    frame.step = meters.step;
    frame.rumble = meters.rumble;
    frame.ppq = meters.ppq;
    frame.bpm = meters.bpm;
    frame.hostClock = meters.hostClock;
    frame.resonance = processor_.engine().getParam(undertow::param::FilterRes);
    frame.length = juce::roundToInt(processor_.engine().getParam(undertow::param::SeqLength));

    std::array<float, kUiScopeSamples> scope{};
    const int copied = processor_.engine().copyScope(scope.data(), kUiScopeSamples);
    if (copied > 0) {
        const auto start = static_cast<std::size_t>(kUiScopeSamples - copied);
        std::copy_n(scope.begin(), static_cast<std::size_t>(copied), frame.scope.begin() + static_cast<std::ptrdiff_t>(start));
    }

    return frame;
}

void UndertowEditor::freeze() {
    stopTimer();
}

void UndertowEditor::advance(const UiFrame& frame, double seconds) {
    currentFrame_ = frame;
    displays_.advance(currentFrame_, seconds);
    status_.advance(currentFrame_, seconds);
    unitArray_.advance(currentFrame_, seconds);
    repaint();
}

juce::Rectangle<int> UndertowEditor::chestBounds() const {
    return undertow::ui::theme::chestBounds;
}

juce::Rectangle<int> UndertowEditor::scopeBounds() const {
    return undertow::ui::theme::scopeBounds;
}

juce::Rectangle<int> UndertowEditor::stepBounds(int index) const {
    return unitArray_.stepBounds(index);
}

int UndertowEditor::stepAt(juce::Point<int> point) const {
    return unitArray_.stepAt(point);
}

void UndertowEditor::editStep(int index, StepEdit edit) {
    processor_.setPattern(UnitArray::edited(processor_.pattern(), index, edit));
}

juce::Component* UndertowEditor::controlFor(const juce::String& parameterId) const {
    const auto index = undertow::findParam(parameterId.toStdString());
    if (index < 0)
        return nullptr;
    if (auto* slider = sliders_[static_cast<std::size_t>(index)].get())
        return slider;
    if (auto* button = buttons_[static_cast<std::size_t>(index)].get())
        return button;
    return nullptr;
}

void UndertowEditor::paint(juce::Graphics& g) {
    g.fillAll(undertow::ui::theme::bgVoid);
    look_.paint(g);
    displays_.paint(g);
    status_.paint(g);
}

void UndertowEditor::paintOverChildren(juce::Graphics& g) {
    status_.paintOver(g);
}

void UndertowEditor::timerCallback() {
    const auto now = juce::Time::getMillisecondCounter();
    const auto elapsedMs = juce::jmax<juce::uint32>(1, now - lastTimerTickMs_);
    lastTimerTickMs_ = now;
    timerSeconds_ = static_cast<double>(elapsedMs) / 1000.0;
    advance(captureFrame(), timerSeconds_);
}
