#pragma once

#include "Theme.h"

#include <undertow/params.h>

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <memory>
#include <vector>

class Knob final : public juce::Slider {
public:
    Knob(bool bipolar, int detents);
    void setLight(float light);
    void setHovered(bool hovered);
    void setDimmed(bool dimmed);
    bool isHovered() const;
    void dropCache();

    void paint(juce::Graphics&) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    juce::String getTextFromValue(double) override;
    void setFormatter(std::function<juce::String(double)> formatter);

private:
    void rebuildCaches(float scale);

    std::function<juce::String(double)> formatter_;
    bool bipolar_ = false;
    int detents_ = 0;
    float light_ = 0.0f;
    bool hovered_ = false;
    bool dragging_ = false;
    bool dimmed_ = false;
    float cachedScale_ = 0.0f;
    int cachedWidth_ = 0;
    int cachedHeight_ = 0;
    juce::Image underlayCache_;
    juce::Image overlayCache_;
};

class Switch final : public juce::Component {
public:
    Switch(juce::RangedAudioParameter& parameter, std::vector<juce::String> labels, bool numberLabels);
    int segments() const;
    int selected() const;
    juce::Rectangle<int> segmentBounds(int index) const;

    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    juce::RangedAudioParameter& parameter_;
    std::vector<juce::String> labels_;
    bool numberLabels_ = false;
    std::unique_ptr<juce::ParameterAttachment> attachment_;
};

class Controls {
public:
    Controls();
    ~Controls();
    void attach(juce::AudioProcessorValueTreeState&, juce::Component& window);
    void detach();
    juce::Component* controlFor(const juce::String&) const;
    void setLight(int step, float light);
    void setColumnDim(int step, bool dim);
    void setHostClock(bool host);
    void setHover(const juce::String& parameterId);
    void paint(juce::Graphics&);
    void dropCache();

private:
    struct Entry;

    void attachKnob(juce::AudioProcessorValueTreeState&, juce::Component&, const undertow::ParamSpec&,
                    const undertow::ui::theme::ParameterPlace&);
    void attachSwitch(juce::AudioProcessorValueTreeState&, juce::Component&, const undertow::ParamSpec&,
                      const undertow::ui::theme::ParameterPlace&);
    static const undertow::ui::theme::ParameterPlace* placeFor(const juce::String& id);
    static bool isSwitchId(const juce::String& id);

    std::vector<Entry> entries_;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments_;
    float stepLights_[8]{};
    bool stepDimmed_[8]{};
    bool hostClock_ = false;
};
