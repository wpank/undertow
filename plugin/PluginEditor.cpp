#include "PluginEditor.h"

#include "PluginProcessor.h"
#include "ui/Theme.h"

#include <undertow/params.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

constexpr int kUiScopeSamples = 2048;

float mapEnergy(float rms) {
    if (!(rms > 0.0f))
        return 0.0f;

    const auto db = juce::Decibels::gainToDecibels(rms, -100.0f);
    return juce::jlimit(0.0f, 1.0f, static_cast<float>((db + 48.0) / 42.0));
}

juce::String formatReadout(const undertow::ParamSpec& spec, double value) {
    if (spec.kind == undertow::ParamKind::Choice && spec.choices != nullptr && spec.numChoices > 0) {
        const auto index = juce::jlimit(0, spec.numChoices - 1, juce::roundToInt(value));
        return spec.choices[index];
    }

    if (spec.kind == undertow::ParamKind::Bool)
        return value >= 0.5 ? "ON" : "OFF";

    if (spec.kind == undertow::ParamKind::Int) {
        juce::String text(juce::roundToInt(value));
        if (spec.unit != nullptr && *spec.unit != '\0')
            text << ' ' << juce::String(spec.unit);
        return text;
    }

    double displayValue = value;
    juce::String unit = spec.unit;
    if (unit == "Hz" && std::abs(displayValue) >= 1000.0) {
        displayValue /= 1000.0;
        unit = "kHz";
    } else if (unit == "ms" && std::abs(displayValue) >= 1000.0) {
        displayValue /= 1000.0;
        unit = "s";
    }

    int decimals = 2;
    const auto absValue = std::abs(displayValue);
    if (absValue >= 100.0)
        decimals = 0;
    else if (absValue >= 10.0)
        decimals = 1;

    juce::String text(displayValue, decimals);
    while (text.containsChar('.') && text.endsWithChar('0'))
        text = text.dropLastCharacters(1);
    if (text.endsWithChar('.'))
        text = text.dropLastCharacters(1);

    if (unit.isNotEmpty())
        text << ' ' << unit;
    return text;
}

bool shouldShowReadout(const juce::Slider& slider) {
    return slider.isMouseOverOrDragging() || slider.isMouseButtonDown();
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
    auto addSlider = [this](int paramIndex, const undertow::ui::theme::ControlSlot& slot) {
        auto slider = std::make_unique<juce::Slider>();
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider->setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                                    juce::MathConstants<float>::pi * 2.8f,
                                    true);
        slider->setName(undertow::paramSpec(paramIndex).id);
        slider->setBounds(slot.cell.controlBounds);
        addAndMakeVisible(*slider);
        sliderAttachments_[static_cast<std::size_t>(paramIndex)] = std::make_unique<SliderAttachment>(
            processor_.parameters(), undertow::paramSpec(paramIndex).id, *slider);
        slider->textFromValueFunction = [paramIndex](double value) {
            return formatReadout(undertow::paramSpec(paramIndex), value);
        };
        slider->onDragStart = [this] { repaint(); };
        slider->onDragEnd = [this] { repaint(); };
        slider->onValueChange = [this] { repaint(); };
        sliders_[static_cast<std::size_t>(paramIndex)] = std::move(slider);
    };

    auto addButton = [this](int paramIndex, const undertow::ui::theme::ControlSlot& slot) {
        auto button = std::make_unique<juce::ToggleButton>();
        button->setName(undertow::paramSpec(paramIndex).id);
        button->setButtonText({});
        button->setBounds(slot.cell.bounds.withSizeKeepingCentre(40, 24));
        addAndMakeVisible(*button);
        buttonAttachments_[static_cast<std::size_t>(paramIndex)] = std::make_unique<ButtonAttachment>(
            processor_.parameters(), undertow::paramSpec(paramIndex).id, *button);
        buttons_[static_cast<std::size_t>(paramIndex)] = std::move(button);
    };

    for (const auto& panel : undertow::ui::theme::controlPanels) {
        for (int i = 0; i < panel.controlCount; ++i) {
            const auto& slot = panel.controls[static_cast<std::size_t>(i)];
            if (slot.isLamp)
                addButton(slot.parameter, slot);
            else
                addSlider(slot.parameter, slot);
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
    repaint(undertow::ui::theme::headBounds);
    repaint(undertow::ui::theme::chestBounds);
    repaint(undertow::ui::theme::gutBounds);
    repaint(undertow::ui::theme::groundBounds);
    repaint(undertow::ui::theme::topHazardBounds);
    repaint(undertow::ui::theme::bottomHazardBounds);
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

std::vector<DrawnText> UndertowEditor::drawnTexts() {
    std::vector<DrawnText> result;
    juce::Image image(juce::Image::ARGB, getWidth(), getHeight(), true);
    juce::Graphics g(image);
    g.fillAll(undertow::ui::theme::bgVoid);
    auto& activeRecord = texts::record();
    activeRecord = &result;
    look_.dropCache();
    look_.drawStaticLayer(g, 1.0f);
    displays_.paint(g);
    status_.paint(g);
    paintControlText(g);
    status_.paintOver(g);
    activeRecord = nullptr;
    return result;
}

void UndertowEditor::paint(juce::Graphics& g) {
    g.fillAll(undertow::ui::theme::bgVoid);
    look_.dropCache();
    look_.paint(g);
    displays_.paint(g);
    status_.paint(g);
    paintControlText(g);
}

void UndertowEditor::paintControlText(juce::Graphics& g) {
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> deferredLabels;
    for (const auto& panel : undertow::ui::theme::controlPanels) {
        for (int i = 0; i < panel.controlCount; ++i) {
            const auto& slot = panel.controls[static_cast<std::size_t>(i)];
            const auto labelBounds = slot.cell.labelBounds();
            const auto overlapsAnyControl = [labelBounds, parameter = slot.parameter](const auto& candidatePanel) {
                for (int candidateIndex = 0; candidateIndex < candidatePanel.controlCount; ++candidateIndex) {
                    const auto& candidate = candidatePanel.controls[static_cast<std::size_t>(candidateIndex)];
                    if (candidate.parameter != parameter && candidate.cell.controlBounds.intersects(labelBounds) &&
                        candidate.cell.controlBounds.getCentreX() < labelBounds.getCentreX())
                        return true;
                }
                return false;
            };

            if (overlapsAnyControl(panel)) {
                deferredLabels.emplace_back(labelBounds, slot.label);
            } else {
                g.setColour(undertow::ui::theme::textDim);
                g.setFont(undertow::ui::theme::sansFont(10.0f));
                texts::draw(g, slot.label, labelBounds, juce::Justification::centred);
            }

            if (auto* slider = sliders_[static_cast<std::size_t>(slot.parameter)].get(); slider != nullptr && shouldShowReadout(*slider)) {
                g.setColour(undertow::ui::theme::textPrimary);
                g.setFont(undertow::ui::theme::monoFont(10.0f));
                texts::draw(g, slider->getTextFromValue(slider->getValue()), slot.cell.readoutBounds(), juce::Justification::centred);
            }
        }
    }

    g.setColour(undertow::ui::theme::textDim);
    g.setFont(undertow::ui::theme::sansFont(10.0f));
    for (const auto& [bounds, label] : deferredLabels)
        texts::draw(g, label, bounds, juce::Justification::centred);
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
