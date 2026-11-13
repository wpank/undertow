#include "PluginEditor.h"

#include "PluginProcessor.h"
#include "ui/Theme.h"

#include <undertow/params.h>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kStepCount = 8;
constexpr double kTimerHz = 60.0;
constexpr double kDefaultElapsedSeconds = 1.0 / kTimerHz;

float readParameterValue(juce::AudioProcessorValueTreeState& state, int index) {
    if (index < 0 || index >= undertow::kNumParams)
        return 0.0f;

    auto* parameter = state.getParameter(undertow::paramSpec(index).id);
    if (parameter == nullptr)
        return 0.0f;

    return parameter->convertFrom0to1(parameter->getValue());
}

juce::ColourGradient groundGradient() {
    juce::ColourGradient gradient(juce::Colour(0xff0a080c), 504.0f, 304.0f, juce::Colour(0xff040405), 1404.0f, 304.0f,
                                  true);
    gradient.addColour(0.65, undertow::ui::theme::bgVoid);
    return gradient;
}

bool shouldDrawNoisePixel(int x, int y) {
    const std::uint32_t hash = static_cast<std::uint32_t>((x + 37) * 73856093u) ^
                               static_cast<std::uint32_t>((y + 53) * 19349663u);
    return hash % 23u == 0u;
}

}  // namespace

UndertowEditor::UndertowEditor(UndertowProcessor& processor)
    : AudioProcessorEditor(processor),
      processor_(processor) {
    setSize(undertow::ui::theme::editorWidth, undertow::ui::theme::editorHeight);
    setResizable(false, false);

    controls_.attach(processor_.parameters(), *this);

    steps_.value = [this](int index) { return parameterValue(index); };
    steps_.onClick = [this](int step) { toggleStepRest(step); };
    steps_.setBounds(undertow::ui::theme::stepsBounds);
    addAndMakeVisible(steps_);

    console_.value = [this](int index) { return parameterValue(index); };

    for (int step = 0; step < kStepCount; ++step) {
        const auto velocity = parameterValue(undertow::param::Step1Vel + step);
        rememberedStepVelocities_[static_cast<std::size_t>(step)] = velocity > 0.0f ? velocity : 0.0f;
    }

    lastTickMs_ = juce::Time::getMillisecondCounterHiRes();
    startTimerHz(static_cast<int>(kTimerHz));
}

UndertowEditor::~UndertowEditor() {
    stopTimer();
    controls_.detach();
}

WindowFrame UndertowEditor::captureFrame() const {
    WindowFrame frame;
    const auto meters = processor_.engine().meters();

    frame.peak = meters.peak;
    frame.rms = meters.rms;
    frame.clipping = meters.clipping;
    frame.note = meters.note;
    frame.noteHz = meters.noteHz;
    frame.step = meters.step;
    frame.count = meters.count;
    frame.root = meters.root;
    frame.rumble = meters.rumble;
    frame.ppq = meters.ppq;
    frame.bpm = meters.bpm;
    frame.hostClock = meters.hostClock;
    frame.sampleRate = processor_.getSampleRate() > 0.0 ? processor_.getSampleRate() : 48000.0;

    std::fill(frame.scope.begin(), frame.scope.end(), 0.0f);
    const int copied = processor_.engine().copyScope(frame.scope.data(), static_cast<int>(frame.scope.size()));
    if (copied > 0 && copied < static_cast<int>(frame.scope.size())) {
        const auto copiedSize = static_cast<std::size_t>(copied);
        const auto padding = frame.scope.size() - copiedSize;
        std::move_backward(frame.scope.begin(), frame.scope.begin() + copied, frame.scope.end());
        std::fill_n(frame.scope.begin(), static_cast<std::ptrdiff_t>(padding), 0.0f);
    }

    return frame;
}

void UndertowEditor::freeze() {
    stopTimer();
    lastTickMs_ = 0.0;
}

void UndertowEditor::advance(const WindowFrame& frame, double seconds) {
    steps_.advance(frame, seconds);
    console_.advance(frame, seconds);

    const bool notesMode = juce::roundToInt(parameterValue(undertow::param::SeqMode)) == 2;
    const int sequenceLength = juce::jlimit(0, kStepCount, juce::roundToInt(parameterValue(undertow::param::SeqLength)));

    for (int step = 0; step < kStepCount; ++step) {
        controls_.setLight(step, notesMode ? 0.0f : steps_.light(step));
        controls_.setColumnDim(step, notesMode || step >= sequenceLength);
    }

    controls_.setHostClock(frame.hostClock);
    repaint();
}

juce::Component* UndertowEditor::controlFor(const juce::String& parameterId) const {
    return controls_.controlFor(parameterId);
}

void UndertowEditor::hover(const juce::String& parameterId) {
    controls_.setHover(parameterId);
    repaint();
}

std::vector<DrawnText> UndertowEditor::drawnTexts() {
    controls_.dropCache();
    steps_.dropCache();
    console_.dropCache();

    std::vector<DrawnText> drawn;
    texts::record() = &drawn;

    juce::Image image(juce::Image::ARGB, getWidth(), getHeight(), true);
    juce::Graphics g(image);
    paintEntireComponent(g, true);

    texts::record() = nullptr;
    return drawn;
}

juce::Rectangle<int> UndertowEditor::stepCellBounds(int step) const {
    return undertow::ui::theme::stepCellBounds(step);
}

int UndertowEditor::stepAt(juce::Point<int> point) const {
    return undertow::ui::theme::stepAt(point);
}

juce::Rectangle<int> UndertowEditor::plotBounds() const {
    return undertow::ui::theme::plotBounds();
}

void UndertowEditor::paint(juce::Graphics& g) {
    paintGround(g);
    controls_.paint(g);
    console_.paint(g);
}

void UndertowEditor::paintOverChildren(juce::Graphics& g) {
    console_.paintOver(g);
}

void UndertowEditor::timerCallback() {
    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    const double elapsed = lastTickMs_ > 0.0 ? (nowMs - lastTickMs_) / 1000.0 : kDefaultElapsedSeconds;
    lastTickMs_ = nowMs;
    advance(captureFrame(), elapsed > 0.0 ? elapsed : kDefaultElapsedSeconds);
}

float UndertowEditor::parameterValue(int index) const {
    return readParameterValue(processor_.parameters(), index);
}

void UndertowEditor::toggleStepRest(int step) {
    if (step < 0 || step >= kStepCount)
        return;

    auto& parameters = processor_.parameters();
    auto* parameter = parameters.getParameter(undertow::paramSpec(undertow::param::Step1Vel + step).id);
    if (parameter == nullptr)
        return;

    const float current = parameter->convertFrom0to1(parameter->getValue());
    const auto index = static_cast<std::size_t>(step);

    float target = 0.0f;
    if (current > 0.0f) {
        rememberedStepVelocities_[index] = current;
    } else {
        target = rememberedStepVelocities_[index] > 0.0f ? rememberedStepVelocities_[index] : 0.80f;
        rememberedStepVelocities_[index] = target;
    }

    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(parameter->convertTo0to1(target));
    parameter->endChangeGesture();
}

void UndertowEditor::paintGround(juce::Graphics& g) {
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (!groundCache_.isValid() || groundCacheScale_ != scale) {
        const int pixelWidth = std::max(1, juce::roundToInt(static_cast<float>(getWidth()) * scale));
        const int pixelHeight = std::max(1, juce::roundToInt(static_cast<float>(getHeight()) * scale));
        groundCache_ = juce::Image(juce::Image::ARGB, pixelWidth, pixelHeight, false);

        juce::Graphics cached(groundCache_);
        cached.addTransform(juce::AffineTransform::scale(scale));
        cached.fillAll(undertow::ui::theme::bgVoid);
        cached.setGradientFill(groundGradient());
        cached.fillAll();

        cached.setColour(undertow::ui::theme::roseDeep.withAlpha(0.35f));
        for (int y = 0; y < getHeight(); y += 4) {
            for (int x = 0; x < getWidth(); x += 4) {
                if (shouldDrawNoisePixel(x, y))
                    cached.fillRect(x, y, 1, 1);
            }
        }
        groundCacheScale_ = scale;
    }

    g.drawImageTransformed(groundCache_, juce::AffineTransform::scale(1.0f / scale), false);
}
