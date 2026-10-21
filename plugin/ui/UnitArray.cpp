#include "UnitArray.h"

#include "Theme.h"

#include <algorithm>
#include <array>

namespace {

constexpr int kStepCount = undertow::kSteps;
constexpr double kHighlightFadeSeconds = 0.25;
constexpr int kSemitonePixels = 8;
constexpr int kCellWidth = 64;
constexpr int kCellHeight = 104;
constexpr int kCellGap = 8;
constexpr int kArrayWidth = kStepCount * kCellWidth + (kStepCount - 1) * kCellGap;
constexpr float kBorderThickness = 1.0f;
constexpr float kBrightBorderThickness = 2.0f;

juce::Rectangle<int> arrayBounds() {
    const auto gut = undertow::ui::theme::gutBounds;
    return {gut.getX() + (gut.getWidth() - kArrayWidth) / 2, gut.getY() + (gut.getHeight() - kCellHeight) / 2, kArrayWidth, kCellHeight};
}

float noteFillAmount(int semitones) {
    return juce::jlimit(0.0f, 1.0f, (static_cast<float>(semitones) + 24.0f) / 48.0f);
}

juce::Colour dimmed(juce::Colour colour, float amount) {
    return colour.interpolatedWith(undertow::ui::theme::bgVoid, juce::jlimit(0.0f, 1.0f, amount));
}

} // namespace

void UnitArray::advance(const UiFrame& frame, double seconds) {
    const int previousStep = frame_.step;
    frame_ = frame;

    if (previousStep != frame_.step && previousStep >= 0 && previousStep < kStepCount)
        lastLitIndex_ = previousStep;

    if (frame_.step >= 0 && frame_.step < kStepCount) {
        lastLitIndex_ = frame_.step;
        highlightAgeSeconds_ = 0.0;
    } else if (lastLitIndex_ >= 0) {
        highlightAgeSeconds_ = std::min(kHighlightFadeSeconds, highlightAgeSeconds_ + std::max(0.0, seconds));
        if (highlightAgeSeconds_ >= kHighlightFadeSeconds)
            lastLitIndex_ = -1;
    }

    repaint();
}

void UnitArray::paint(juce::Graphics& g) {
    const auto pat = pattern ? pattern() : undertow::Pattern{};
    const int seqLength = juce::jlimit(0, kStepCount, frame_.length);
    const float fade = 1.0f - juce::jlimit(0.0f, 1.0f, static_cast<float>(highlightAgeSeconds_ / kHighlightFadeSeconds));

    g.fillAll(juce::Colours::transparentBlack);

    for (int i = 0; i < kStepCount; ++i) {
        auto outer = stepBounds(i);
        auto bounds = outer.reduced(2).translated(-getX(), -getY());
        const auto& step = pat.steps[static_cast<std::size_t>(i)];
        const bool beyondLength = i >= seqLength;
        const bool active = i == frame_.step;
        const bool faded = !active && i == lastLitIndex_ && fade > 0.0f;
        const float glow = active ? 1.0f : (faded ? fade : 0.0f);

        juce::Colour baseFill = step.gate ? undertow::ui::theme::roseDeep : undertow::ui::theme::bgRaised;
        if (beyondLength)
            baseFill = dimmed(baseFill, 0.65f);

        juce::Colour glowFill = undertow::ui::theme::roseEmber.withAlpha(0.30f + 0.35f * glow);
        g.setColour(glow > 0.0f ? baseFill.interpolatedWith(glowFill, glow) : baseFill);
        g.fillRect(bounds);

        auto inner = bounds.reduced(8, 8);
        const int gaugeHeight = juce::roundToInt(inner.getHeight() * noteFillAmount(step.semitones));
        auto gauge = juce::Rectangle<int>{inner.getX(), inner.getBottom() - gaugeHeight, 12, gaugeHeight};

        if (step.gate) {
            juce::Colour gaugeColour = glow > 0.0f ? undertow::ui::theme::roseBright : undertow::ui::theme::rose;
            if (beyondLength)
                gaugeColour = dimmed(gaugeColour, 0.55f);
            g.setColour(gaugeColour);
            g.fillRect(gauge);
        }

        juce::Colour borderColour = undertow::ui::theme::border;
        float borderThickness = kBorderThickness;
        if (glow > 0.0f) {
            borderColour = undertow::ui::theme::roseBright.withAlpha(0.55f + 0.45f * glow);
            borderThickness = kBrightBorderThickness;
        } else if (beyondLength) {
            borderColour = undertow::ui::theme::textGhost;
        } else if (!step.gate) {
            borderColour = undertow::ui::theme::roseDim;
        }

        g.setColour(borderColour);
        if (step.gate) {
            g.drawRect(bounds.toFloat(), borderThickness);
        } else {
            std::array<float, 4> dashLengths{6.0f, 4.0f, 6.0f, 4.0f};
            juce::Path dashed;
            dashed.addRectangle(bounds.toFloat());
            juce::PathStrokeType stroke(borderThickness);
            stroke.createDashedStroke(dashed, dashed, dashLengths.data(), static_cast<int>(dashLengths.size()));
            g.strokePath(dashed, juce::PathStrokeType(borderThickness));
        }

        auto numberArea = juce::Rectangle<int>{bounds.getX(), bounds.getBottom() - 22, bounds.getWidth(), 14};
        g.setColour(beyondLength ? undertow::ui::theme::textGhost : undertow::ui::theme::roseDim);
        g.setFont(undertow::ui::theme::monoFont(11.0f, juce::Font::bold));
        g.drawText(juce::String(i + 1).paddedLeft('0', 2), numberArea, juce::Justification::centred, false);

        if (step.accent) {
            g.setColour(beyondLength ? undertow::ui::theme::roseDim : undertow::ui::theme::roseBright);
            g.setFont(undertow::ui::theme::sansFont(12.0f, juce::Font::bold));
            g.drawText(juce::String::fromUTF8("▲"), bounds.removeFromTop(18), juce::Justification::centredTop, false);
        }

        if (step.slide) {
            auto slideArea = juce::Rectangle<float>(static_cast<float>(bounds.getRight() - 18), static_cast<float>(bounds.getY() + 16), 12.0f, 24.0f);
            juce::Path slide;
            slide.startNewSubPath(slideArea.getX(), slideArea.getBottom());
            slide.lineTo(slideArea.getRight(), slideArea.getY());
            g.setColour(beyondLength ? undertow::ui::theme::roseDim : undertow::ui::theme::roseBright);
            g.strokePath(slide, juce::PathStrokeType(1.5f));
        }

        const int litDots = juce::jlimit(0, 4, juce::roundToInt(step.probability * 4.0f));
        auto dotRow = juce::Rectangle<int>{bounds.getX() + 18, bounds.getBottom() - 36, bounds.getWidth() - 36, 8};
        for (int dot = 0; dot < 4; ++dot) {
            const auto dotBounds = juce::Rectangle<float>(static_cast<float>(dotRow.getX() + dot * 10), static_cast<float>(dotRow.getY() + 1), 5.0f, 5.0f);
            const bool lit = dot < litDots;
            juce::Colour dotColour = lit ? undertow::ui::theme::rose : undertow::ui::theme::textPhantom;
            if (beyondLength)
                dotColour = lit ? undertow::ui::theme::textDim : undertow::ui::theme::textPhantom;
            g.setColour(dotColour);
            g.fillEllipse(dotBounds);
        }
    }
}

juce::Rectangle<int> UnitArray::stepBounds(int index) const {
    if (index < 0 || index >= kStepCount)
        return {};

    const auto array = arrayBounds();
    return {array.getX() + index * (kCellWidth + kCellGap), array.getY(), kCellWidth, kCellHeight};
}

int UnitArray::stepAt(juce::Point<int> point) const {
    for (int i = 0; i < kStepCount; ++i) {
        if (stepBounds(i).contains(point))
            return i;
    }
    return -1;
}

undertow::Pattern UnitArray::edited(undertow::Pattern patternValue, int index, StepEdit edit) {
    if (index < 0 || index >= kStepCount)
        return patternValue;

    auto& step = patternValue.steps[static_cast<std::size_t>(index)];
    switch (edit) {
        case StepEdit::ToggleGate:
            step.gate = !step.gate;
            break;
        case StepEdit::ToggleAccent:
            step.accent = !step.accent;
            break;
        case StepEdit::ToggleSlide:
            step.slide = !step.slide;
            break;
        case StepEdit::Up:
            step.semitones = juce::jlimit(-24, 24, step.semitones + 1);
            break;
        case StepEdit::Down:
            step.semitones = juce::jlimit(-24, 24, step.semitones - 1);
            break;
    }
    return patternValue;
}

void UnitArray::mouseDown(const juce::MouseEvent& event) {
    hoverIndex_ = stepAt(event.getEventRelativeTo(getParentComponent()).position.roundToInt());
    dragIndex_ = stepAt(event.getEventRelativeTo(getParentComponent()).position.roundToInt());
    dragging_ = dragIndex_ >= 0;
    moved_ = false;
    dragStartY_ = event.getPosition().y;
    dragAccumulatedSteps_ = 0;

    if (dragIndex_ < 0)
        return;

    if (const auto current = pattern ? pattern() : undertow::Pattern{}; dragIndex_ < kStepCount)
        dragStartSemitones_ = current.steps[static_cast<std::size_t>(dragIndex_)].semitones;

    triggerEdit(dragIndex_, dragEditForEvent(event));
}

void UnitArray::mouseDrag(const juce::MouseEvent& event) {
    if (!dragging_ || dragIndex_ < 0)
        return;

    const int deltaPixels = dragStartY_ - event.getPosition().y;
    const int targetSteps = deltaPixels / kSemitonePixels;

    while (dragAccumulatedSteps_ < targetSteps) {
        triggerEdit(dragIndex_, StepEdit::Up);
        ++dragAccumulatedSteps_;
        moved_ = true;
    }
    while (dragAccumulatedSteps_ > targetSteps) {
        triggerEdit(dragIndex_, StepEdit::Down);
        --dragAccumulatedSteps_;
        moved_ = true;
    }
}

void UnitArray::mouseUp(const juce::MouseEvent&) {
    dragging_ = false;
    dragIndex_ = -1;
    dragAccumulatedSteps_ = 0;
    moved_ = false;
}

StepEdit UnitArray::dragEditForEvent(const juce::MouseEvent& event) const {
    if (event.mods.isShiftDown())
        return StepEdit::ToggleAccent;
    if (event.mods.isAltDown())
        return StepEdit::ToggleSlide;
    return StepEdit::ToggleGate;
}

void UnitArray::triggerEdit(int index, StepEdit edit) {
    if (onEdit)
        onEdit(index, edit);
}

void UnitArray::repaintStep(int index) {
    const auto bounds = stepBounds(index);
    repaint(bounds.translated(-getX(), -getY()));
}
