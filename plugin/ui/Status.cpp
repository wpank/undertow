#include "Status.h"

#include "Theme.h"
#include "Texts.h"

#include <algorithm>
#include <cmath>

namespace {

float clamp01(float value) {
    return juce::jlimit(0.0f, 1.0f, value);
}

juce::String formatTempo(float bpm) {
    return juce::String(bpm, 1) + " BPM";
}

juce::String formatBeat(double ppq) {
    const auto beats = std::max(0.0, ppq);
    const auto bar = static_cast<int>(std::floor(beats / 4.0)) + 1;
    const auto beat = static_cast<int>(std::floor(std::fmod(beats, 4.0))) + 1;
    const auto sixteenth = static_cast<int>(std::floor(std::fmod(beats * 4.0, 4.0))) + 1;
    return juce::String::formatted("%03d.%d.%d", bar, beat, sixteenth);
}

juce::Colour meterColour(float value) {
    using namespace undertow::ui::theme;
    return roseDeep.interpolatedWith(roseBright, clamp01(value));
}

juce::String liveText(bool active) {
    return juce::String::fromUTF8(active ? u8"◉ LIVE" : u8"○ LIVE");
}

juce::String markText() {
    return juce::String::fromUTF8(u8"⌈ U N D E R T O W ⌋");
}

juce::String clipLegend() {
    return juce::String::fromUTF8(u8"⌈ CLIP ⌋  OUTPUT -0.3 dBFS");
}

} // namespace

Status::Status() = default;

void Status::advance(const UiFrame& frame, double seconds) {
    frame_ = frame;
    const auto dt = static_cast<float>(juce::jlimit(0.0, 0.1, seconds));
    clockPhase_ = std::fmod(clockPhase_ + static_cast<double>(dt) * (frame.hostClock ? frame.bpm : 120.0) / 60.0, 1.0);
    borderFlash_ = frame.clipping ? std::min(1.0, borderFlash_ + static_cast<double>(dt) * 7.5) : std::max(0.0, borderFlash_ - static_cast<double>(dt) * 5.0);
    stripeOffset_ = std::fmod(stripeOffset_ + static_cast<double>(dt) * (0.35 + frame.rumble * 1.7 + frame.ppq * 0.02), 40.0);
    meterNeedle_ += (clamp01(frame.energy) - meterNeedle_) * juce::jlimit(0.0f, 1.0f, dt * 10.0f);
    rumbleNeedle_ += (clamp01(frame.rumble) - rumbleNeedle_) * juce::jlimit(0.0f, 1.0f, dt * 8.0f);
}

void Status::paintHeadText(juce::Graphics& g) const {
    using namespace undertow::ui::theme;

    g.setFont(monoFont(11.0f));
    g.setColour(roseDim);
    texts::draw(g, juce::String::fromUTF8(u8"METAKOSMIA"), juce::Rectangle<int>(22, 17, 118, 12), juce::Justification::centredLeft);

    g.setColour(rose);
    texts::draw(g, markText(), juce::Rectangle<int>(142, 17, 192, 12), juce::Justification::centredLeft);

    g.setFont(monoFont(13.0f, juce::Font::bold));
    g.setColour(textPrimary);
    texts::draw(g, formatTempo(frame_.bpm), juce::Rectangle<int>(360, 12, 96, 18), juce::Justification::centredLeft);
    texts::draw(g, formatBeat(frame_.ppq), juce::Rectangle<int>(456, 12, 126, 18), juce::Justification::centred);

    g.setColour(frame_.hostClock ? rose : dream);
    texts::draw(g, frame_.hostClock ? "HOST" : "INT", juce::Rectangle<int>(582, 12, 56, 18), juce::Justification::centredLeft);
    texts::draw(g, liveText(frame_.energy > 0.001f), juce::Rectangle<int>(960, 12, 96, 18), juce::Justification::centredLeft);
}

void Status::paint(juce::Graphics& g) {
    using namespace undertow::ui::theme;
    const auto head = headBounds;

    g.setColour(bgVoid);
    g.fillRect(head);

    paintHeadText(g);

    const auto meter = juce::Rectangle<int>(1002, 10, 10, 20);
    g.setColour(bgVoid);
    g.fillRect(meter);
    g.setColour(roseDim);
    g.fillRect(juce::Rectangle<int>(1004, 12, 2, 16));
    g.setColour(meterColour(meterNeedle_));
    const auto meterHeight = juce::roundToInt(16.0f * meterNeedle_);
    g.fillRect(juce::Rectangle<int>(1004, 28 - meterHeight, 2, meterHeight));
}

void Status::paintOver(juce::Graphics& g) {
    if (!frame_.clipping)
        return;

    using namespace undertow::ui::theme;
    const auto flash = 0.32f + 0.68f * static_cast<float>(borderFlash_);
    const auto topBand = topHazardBounds;
    const auto bottomBand = bottomHazardBounds;

    g.setColour(roseBright.withAlpha(0.20f * flash));
    g.fillRect(topBand);
    g.fillRect(bottomBand);

    const auto drawStripes = [&](const juce::Rectangle<int>& area) {
        g.saveState();
        g.reduceClipRegion(area);
        g.setColour(roseBright.withAlpha(0.92f));
        for (int x = area.getX() - 40; x < area.getRight() + 40; x += 24) {
            juce::Path stripe;
            const auto offset = static_cast<int>(std::fmod(stripeOffset_, 24.0));
            const auto sx = x + offset;
            stripe.startNewSubPath(static_cast<float>(sx), static_cast<float>(area.getBottom()));
            stripe.lineTo(static_cast<float>(sx + 14), static_cast<float>(area.getBottom()));
            stripe.lineTo(static_cast<float>(sx + 38), static_cast<float>(area.getY()));
            stripe.lineTo(static_cast<float>(sx + 24), static_cast<float>(area.getY()));
            stripe.closeSubPath();
            g.fillPath(stripe);
        }
        g.restoreState();
    };

    drawStripes(topBand);
    drawStripes(bottomBand);

    const auto legendPlate = juce::Rectangle<int>(430, 0, 340, 16).reduced(12, 0);
    g.setColour(bgVoid);
    g.fillRect(legendPlate);
    g.setFont(monoFont(10.0f));
    g.setColour(roseBright);
    texts::draw(g, clipLegend(), juce::Rectangle<int>(430, 0, 340, 16), juce::Justification::centred);
}
