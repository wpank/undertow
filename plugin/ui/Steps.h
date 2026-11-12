#pragma once

#include "Frame.h"
#include "undertow/pattern.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>

class Steps final : public juce::Component {
public:
    std::function<float(int parameterIndex)> value;
    std::function<void(int step)> onClick;

    Steps();
    void advance(const WindowFrame& frame, double seconds);
    float light(int step) const;
    juce::Rectangle<int> cellBounds(int step) const;
    int stepAt(juce::Point<int> windowPoint) const;
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void dropCache();

private:
    static constexpr int kStepCount = 8;
    static constexpr int kTraceColumns = 66;

    struct Trace {
        std::array<float, kTraceColumns> minimum{};
        std::array<float, kTraceColumns> maximum{};
        int columns = 0;
    };

    static float readValue(const std::function<float(int)>& value, int index);
    static double stepLengthSeconds(double bpm, int rateChoice);
    void updatePattern();
    void updateCurrentTrace(double seconds);
    juce::Rectangle<int> localCellBounds(int step) const;
    juce::Rectangle<int> localTraceBounds(int step) const;
    float columnOpacity(int step) const;
    float traceOpacity(int step) const;
    float borderOpacity(int step) const;
    void drawRestBorder(juce::Graphics& g, juce::Rectangle<int> cell, float alpha) const;
    void drawTrace(juce::Graphics& g, int step, juce::Rectangle<int> trace) const;
    void drawCell(juce::Graphics& g, int step) const;
    void drawText(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> box,
                  juce::Justification justification) const;

    std::array<float, undertow::kNumParams> parameterValues_{};
    std::array<float, kStepCount> lights_{};
    std::array<int, kStepCount> ages_{};
    std::array<Trace, kStepCount> traces_{};
    undertow::Pattern pattern_{};
    WindowFrame frame_{};
    int currentStep_ = -1;
    int currentLength_ = 8;
    bool notesMode_ = false;
    double currentStepElapsed_ = 0.0;
};
