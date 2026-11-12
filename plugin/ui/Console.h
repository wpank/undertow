#pragma once

#include "Frame.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <undertow/spectrum.h>

#include <array>
#include <functional>

class Console {
public:
    std::function<float(int parameterIndex)> value;

    Console();
    void advance(const WindowFrame&, double);
    void paint(juce::Graphics&);
    void paintOver(juce::Graphics&);
    void dropCache();
    juce::Rectangle<int> plotBounds() const;

private:
    static constexpr int kCurveHistory = 5;

    using Curve = std::array<float, undertow::kSpectrumPoints>;

    float paramValue(int index) const;
    static float linearToDb(float linear, float floorDb = -96.0f);
    static float plotXForHz(double hz);
    static float plotYForDb(float db);
    static juce::String noteName(int midiNote);
    static juce::String formatFrequency(float hz);
    static juce::String formatPeakDb(float db);
    static juce::Rectangle<int> textBounds(const juce::Font&, const juce::String&, float anchorX, float baseline,
                                           juce::Justification, int paddingX = 0, int paddingY = 1);
    static void drawText(juce::Graphics&, const juce::Font&, juce::Colour, const juce::String&, float anchorX,
                         float baseline, juce::Justification, int paddingX = 0, int paddingY = 1);
    static float easeOut(float t);

    void updateSpectrum(const WindowFrame&, double);
    void pushSpectrumCurve();
    void updateHint(double seconds, bool sounding, int mode);
    void updateNumber(const WindowFrame&, double, bool sounding);
    void updateArc();
    void updateMeter(double seconds);
    void updateRumble(const WindowFrame&, double);
    void updateClip(const WindowFrame&, double);

    void drawGlass(juce::Graphics&, juce::Rectangle<float>, float radius) const;
    void drawSpectrum(juce::Graphics&);
    void drawSpectrumCurve(juce::Graphics&, const Curve&, juce::Colour, float stroke, float alpha, bool newest) const;
    void drawNumber(juce::Graphics&);
    void drawArc(juce::Graphics&);
    void drawHead(juce::Graphics&);
    void drawMeter(juce::Graphics&);
    void drawRumble(juce::Graphics&);
    void drawHazardBand(juce::Graphics&, juce::Rectangle<int>, float offset) const;
    void drawLamp(juce::Graphics&, juce::Point<float> centre, float radius, bool lit) const;

    WindowFrame frame_{};
    undertow::Spectrum spectrum_;
    double sampleRate_ = 48000.0;
    int spectrumTick_ = 0;
    double spectrumSeconds_ = 0.0;
    std::array<Curve, kCurveHistory> curves_{};
    std::array<bool, kCurveHistory> curveVisible_{};

    float hintAlpha_ = 1.0f;

    bool hasDisplayedNumber_ = false;
    float displayedHz_ = 0.0f;
    float numberStartHz_ = 0.0f;
    float numberTargetHz_ = 0.0f;
    float numberProgress_ = 1.0f;
    float numberFade_ = 0.0f;
    juce::String displayedNote_;

    float heldPeakDb_ = -96.0f;
    double peakHoldSeconds_ = 0.0;

    bool clipping_ = false;
    float clipFlash_ = 0.0f;
    double hazardOffset_ = 0.0;

    double rumbleFlashSeconds_ = 0.0;
    bool haveLastPpq_ = false;
    double lastPpq_ = 0.0;
};
