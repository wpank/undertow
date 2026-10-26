#pragma once

#include <array>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "UiFrame.h"

class Displays {
public:
    Displays();

    void advance(const UiFrame&, double);
    void paint(juce::Graphics&);

private:
    struct TunnelSample {
        float x = 0.0f;
        float y = 0.0f;
        float distance = 0.0f;
        float angle = 0.0f;
        float vignette = 0.0f;
    };

    static constexpr int kTunnelScale = 4;
    static constexpr int kPhosphorHistory = 6;

    void ensureTunnelTables();
    void renderTunnel();
    void updateScopeHistory(const UiFrame&);
    void paintTunnel(juce::Graphics&) const;
    void paintAtField(juce::Graphics&) const;
    void paintScope(juce::Graphics&) const;
    void paintPumpField(juce::Graphics&) const;
    void paintBone(juce::Graphics&) const;

    std::vector<TunnelSample> tunnelTable_;
    juce::Image tunnelImage_;
    std::array<juce::Path, kPhosphorHistory> scopeHistory_{};
    int historyCount_ = 0;
    int historyHead_ = -1;

    UiFrame frame_{};
    double tunnelPhase_ = 0.0;
    double linePhase_ = 0.0;
    float tunnelEnergy_ = 0.0f;
    float noteFade_ = 0.0f;
    float idleFade_ = 1.0f;
    int displayedNote_ = -1;
    float displayedNoteHz_ = 0.0f;
    std::array<std::array<float, 24>, 32> pumpHistory_{};
};
