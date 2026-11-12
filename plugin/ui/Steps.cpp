#include "Steps.h"

#include "Texts.h"
#include "Theme.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr int kTraceRows = 34;
constexpr double kLightFadeSeconds = 0.25;
constexpr float kPastLengthAlpha = 0.30f;
constexpr float kNotesModeAlpha = 0.40f;

constexpr std::array<double, 8> kRateBeats{{4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0}};

juce::Rectangle<int> translatedCell(const juce::Rectangle<int>& cell, const juce::Point<int>& offset) {
    return cell.translated(-offset.x, -offset.y);
}

float clampTraceSample(float value) {
    return juce::jlimit(-0.4f, 0.4f, value);
}

juce::String driftText(const undertow::Pattern& pattern, const WindowFrame& frame, int step) {
    if (frame.root < 0)
        return {};

    const int length = std::max(1, pattern.length());
    const long long passStart = static_cast<long long>(std::floor(static_cast<double>(frame.count) / length)) * length;
    const auto home = pattern.home(step);
    if (home.velocity <= 0.0f)
        return {};

    const auto played = pattern.at(passStart + step, frame.root);
    if (played.pitch != home.pitch) {
        const int delta = played.pitch - home.pitch;
        return (delta > 0 ? "+" : "") + juce::String(delta);
    }

    if (played.velocity < home.velocity)
        return "GHOST";

    return {};
}

}  // namespace

Steps::Steps() = default;

float Steps::readValue(const std::function<float(int)>& value, int index) {
    return value ? value(index) : 0.0f;
}

double Steps::stepLengthSeconds(double bpm, int rateChoice) {
    const double safeBpm = bpm > 1.0 ? bpm : 120.0;
    const int choice = juce::jlimit(0, static_cast<int>(kRateBeats.size()) - 1, rateChoice);
    return 60.0 / safeBpm * kRateBeats[static_cast<std::size_t>(choice)];
}

void Steps::updatePattern() {
    for (int i = 0; i < undertow::kNumParams; ++i)
        parameterValues_[static_cast<std::size_t>(i)] = readValue(value, i);
    pattern_.setParams(parameterValues_.data());
    currentLength_ = juce::jlimit(1, kStepCount, pattern_.length());
    notesMode_ = juce::roundToInt(parameterValues_[static_cast<std::size_t>(undertow::param::SeqMode)]) == 2;
}

void Steps::updateCurrentTrace(double seconds) {
    (void)seconds;
    if (currentStep_ < 0 || currentStep_ >= kStepCount)
        return;

    const auto currentHome = pattern_.home(currentStep_);
    if (currentHome.velocity <= 0.0f) {
        traces_[static_cast<std::size_t>(currentStep_)] = {};
        return;
    }

    const double stepLength = stepLengthSeconds(parameterValues_[static_cast<std::size_t>(undertow::param::SeqBpm)],
                                               juce::roundToInt(parameterValues_[static_cast<std::size_t>(undertow::param::SeqRate)]));
    const auto& scope = frame_.scope;
    const int sampleCount = juce::jlimit(0, static_cast<int>(scope.size()), juce::roundToInt(currentStepElapsed_ * frame_.sampleRate));
    const int columns = juce::jlimit(0, kTraceColumns, juce::roundToInt(kTraceColumns * (stepLength > 0.0 ? currentStepElapsed_ / stepLength : 0.0)));

    auto& trace = traces_[static_cast<std::size_t>(currentStep_)];
    trace.columns = columns;
    if (columns <= 0 || sampleCount <= 0)
        return;

    const int start = static_cast<int>(scope.size()) - sampleCount;
    for (int col = 0; col < columns; ++col) {
        const int begin = start + (sampleCount * col) / columns;
        int end = start + (sampleCount * (col + 1)) / columns;
        if (end <= begin)
            end = std::min(start + sampleCount, begin + 1);

        float minimum = 1.0f;
        float maximum = -1.0f;
        for (int i = begin; i < end; ++i) {
            const float sample = scope[static_cast<std::size_t>(i)];
            minimum = std::min(minimum, sample);
            maximum = std::max(maximum, sample);
        }
        trace.minimum[static_cast<std::size_t>(col)] = clampTraceSample(minimum);
        trace.maximum[static_cast<std::size_t>(col)] = clampTraceSample(maximum);
    }
}

void Steps::advance(const WindowFrame& frame, double seconds) {
    frame_ = frame;
    updatePattern();

    const int nextStep = (frame.step >= 0 && frame.step < kStepCount) ? frame.step : -1;
    const bool stepChanged = nextStep != currentStep_;

    if (stepChanged) {
        currentStepElapsed_ = 0.0;
        for (int step = 0; step < kStepCount; ++step) {
            if (step != nextStep)
                ages_[static_cast<std::size_t>(step)] = std::min(ages_[static_cast<std::size_t>(step)] + 1, 7);
        }
        if (nextStep >= 0) {
            ages_[static_cast<std::size_t>(nextStep)] = 0;
            traces_[static_cast<std::size_t>(nextStep)] = {};
        }
    }

    currentStep_ = nextStep;
    if (currentStep_ >= 0)
        currentStepElapsed_ += seconds;

    const float fade = static_cast<float>(seconds / kLightFadeSeconds);
    for (int step = 0; step < kStepCount; ++step) {
        auto& light = lights_[static_cast<std::size_t>(step)];
        if (step == currentStep_ && pattern_.home(step).velocity > 0.0f)
            light = 1.0f;
        else
            light = std::max(0.0f, light - fade);
    }

    updateCurrentTrace(seconds);
}

float Steps::light(int step) const {
    if (step < 0 || step >= kStepCount)
        return 0.0f;
    return lights_[static_cast<std::size_t>(step)];
}

juce::Rectangle<int> Steps::cellBounds(int step) const {
    return undertow::ui::theme::stepCellBounds(step);
}

int Steps::stepAt(juce::Point<int> windowPoint) const {
    return undertow::ui::theme::stepAt(windowPoint);
}

juce::Rectangle<int> Steps::localCellBounds(int step) const {
    return translatedCell(cellBounds(step), getPosition());
}

juce::Rectangle<int> Steps::localTraceBounds(int step) const {
    const auto cell = localCellBounds(step);
    return {cell.getX() + 4, cell.getY() + 18, 66, kTraceRows};
}

float Steps::columnOpacity(int step) const {
    if (notesMode_)
        return kNotesModeAlpha;
    if (step >= currentLength_)
        return kPastLengthAlpha;
    return 1.0f;
}

float Steps::traceOpacity(int step) const {
    const int age = step >= 0 && step < kStepCount ? ages_[static_cast<std::size_t>(step)] : 0;
    return columnOpacity(step) * juce::jlimit(0.32f, 0.92f, 0.92f - 0.10f * static_cast<float>(age));
}

void Steps::drawText(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> box,
                     juce::Justification justification) const {
    const auto before = texts::record() ? texts::record()->size() : 0u;
    texts::draw(g, text, box, justification);
    if (auto* record = texts::record(); record != nullptr && record->size() > before)
        record->back().bounds = record->back().bounds.translated(getPosition().x, getPosition().y);
}

void Steps::drawRestBorder(juce::Graphics& g, juce::Rectangle<int> cell, float alpha) const {
    const auto outline = cell.toFloat().reduced(0.5f);
    juce::Path path;
    path.addRoundedRectangle(outline, 3.0f);
    juce::Path dashed;
    constexpr float dashPattern[] = {3.0f, 3.0f};
    juce::PathStrokeType(1.0f).createDashedStroke(dashed, path, dashPattern, 2);
    g.setColour(undertow::ui::theme::roseDim.withAlpha(alpha));
    g.strokePath(dashed, juce::PathStrokeType(1.0f));
}

void Steps::drawTrace(juce::Graphics& g, int step, juce::Rectangle<int> trace) const {
    const auto& stored = traces_[static_cast<std::size_t>(step)];
    const float alpha = traceOpacity(step);
    if (stored.columns <= 0 || alpha <= 0.0f)
        return;

    const auto traceColour = undertow::ui::theme::roseBright;
    const float halfHeight = trace.getHeight() * 0.5f;
    const float midY = trace.getCentreY();
    g.setColour(traceColour.withAlpha(alpha));
    for (int col = 0; col < stored.columns; ++col) {
        const float top = midY - stored.maximum[static_cast<std::size_t>(col)] / 0.4f * halfHeight;
        const float bottom = midY - stored.minimum[static_cast<std::size_t>(col)] / 0.4f * halfHeight;
        const int y = juce::roundToInt(std::floor(std::min(top, bottom)));
        const int h = std::max(1, juce::roundToInt(std::ceil(std::max(top, bottom))) - y + 1);
        g.fillRect(trace.getX() + col, y, 1, h);
    }

    if (stored.columns < kTraceColumns) {
        g.setColour(undertow::ui::theme::roseBright.withAlpha(alpha * 0.7f));
        g.fillRect(trace.getX() + stored.columns, trace.getY(), 1, trace.getHeight());
    }
}

void Steps::drawCell(juce::Graphics& g, int step) const {
    const auto cell = localCellBounds(step);
    const auto trace = localTraceBounds(step);
    const auto offsetAlpha = columnOpacity(step);
    const bool sounding = pattern_.home(step).velocity > 0.0f;
    const float lightAmount = juce::jlimit(0.0f, 1.0f, light(step));
    const bool active = step == currentStep_ && sounding && lightAmount > 0.0f;

    const float cellLight = juce::jlimit(0.0f, 1.0f, light(step));

    g.setColour(undertow::ui::theme::glass.withAlpha((0.32f + 0.10f * cellLight) * offsetAlpha));
    g.fillRoundedRectangle(cell.toFloat().reduced(0.5f), 3.0f);

    if (sounding) {
        g.setColour(undertow::ui::theme::roseEmber.withAlpha((0.08f + 0.08f * cellLight) * offsetAlpha));
        g.fillRoundedRectangle(cell.reduced(1).toFloat(), 3.0f);
        const float brightAlpha = juce::jlimit(0.0f, 1.0f, (0.35f + 0.65f * cellLight) * offsetAlpha);
        g.setColour(undertow::ui::theme::roseBright.withAlpha(brightAlpha));
        g.fillRect(cell.getX(), cell.getY(), cell.getWidth(), 2);
        g.fillRect(cell.getX(), cell.getBottom() - 2, cell.getWidth(), 2);
        g.fillRect(cell.getX(), cell.getY(), 2, cell.getHeight());
        g.fillRect(cell.getRight() - 2, cell.getY(), 2, cell.getHeight());
        drawTrace(g, step, trace);
        g.setColour(undertow::ui::theme::rule.withAlpha(0.45f * offsetAlpha));
        g.drawRoundedRectangle(cell.toFloat().reduced(0.5f), 3.0f, 1.0f);
    } else {
        drawRestBorder(g, cell, 0.85f * offsetAlpha);
    }

    if (active) {
        g.setColour(undertow::ui::theme::roseBright.withAlpha(0.82f * offsetAlpha));
        g.drawRoundedRectangle(cell.expanded(1).toFloat(), 4.0f, 1.2f);
    }

    const auto numberColour = active ? undertow::ui::theme::roseBright : (sounding ? undertow::ui::theme::roseDim : undertow::ui::theme::roseDim);
    g.setFont(undertow::ui::theme::numberFont(9.5f, 0.2f));
    g.setColour(numberColour.withAlpha(offsetAlpha));
    drawText(g, juce::String(step + 1), undertow::ui::theme::textBox(cell.getX() + 7.0f, cell.getY() + 13.0f, 12, 10,
                                                                       juce::Justification::left),
             juce::Justification::left);

    const auto mark = driftText(pattern_, frame_, step);
    if (mark.isNotEmpty()) {
        g.setFont(mark == "GHOST" ? undertow::ui::theme::legendFont(6.0f, 1.0f)
                                    : undertow::ui::theme::numberFont(9.0f, 0.2f));
        g.setColour(undertow::ui::theme::roseBright.withAlpha(offsetAlpha));
        drawText(g, mark, undertow::ui::theme::textBox(cell.getX() + 68.0f, cell.getY() + 13.0f, 30, 10,
                                                       juce::Justification::right),
                 juce::Justification::right);
    }

    if (!sounding) {
        g.setFont(undertow::ui::theme::legendFont(6.5f, 1.0f));
        g.setColour(undertow::ui::theme::roseDim.withAlpha(offsetAlpha));
        drawText(g, "REST", undertow::ui::theme::textBox(cell.getCentreX(), cell.getY() + 35.0f, 44, 10,
                                                          juce::Justification::centred),
                 juce::Justification::centred);
    }
}

void Steps::paint(juce::Graphics& g) {
    for (int step = 0; step < kStepCount; ++step)
        drawCell(g, step);

    if (notesMode_) {
        const auto plate = juce::Rectangle<int>(188, 20, 288, 18);
        g.setColour(undertow::ui::theme::glass.withAlpha(0.92f));
        g.fillRoundedRectangle(plate.toFloat(), 3.0f);
        g.setColour(undertow::ui::theme::rule.withAlpha(0.7f));
        g.drawRoundedRectangle(plate.toFloat(), 3.0f, 1.0f);
        g.setFont(undertow::ui::theme::legendFont(7.5f, 1.1f));
        g.setColour(undertow::ui::theme::roseDim);
        drawText(g, "NOTES MODE: EACH MIDI NOTE IS ONE HIT",
                 undertow::ui::theme::textBox(332.0f, 34.0f, 300, 12, juce::Justification::centred),
                 juce::Justification::centred);
    }
}

void Steps::mouseUp(const juce::MouseEvent& event) {
    const auto step = stepAt(event.getPosition() + getPosition());
    if (step >= 0 && onClick)
        onClick(step);
}

void Steps::dropCache() {}
