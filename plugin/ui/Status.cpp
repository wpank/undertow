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

void drawText(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> box, juce::Justification justification) {
    texts::draw(g, text, box, justification);
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

void Status::paint(juce::Graphics& g) {
    using namespace undertow::ui::theme;
    const auto head = headBounds;

    g.setColour(bgVoid);
    g.fillRect(head);

    const auto leftMark = juce::Rectangle<int>(22, 8, 156, 22);
    g.setColour(roseDim);
    g.setFont(monoFont(11.0f));
    drawText(g, "METAKOSMIA", leftMark, juce::Justification::centredLeft);
    g.setColour(rose);
    drawText(g, "⌈ U N D E R T O W ⌋", leftMark.translated(0, 18), juce::Justification::centredLeft);

    juce::Rectangle<int> clockArea(360, 6, 360, 30);
    g.setColour(textPrimary);
    g.setFont(monoFont(13.0f, juce::Font::bold));
    auto tempoBox = clockArea.removeFromLeft(104);
    auto beatBox = clockArea.removeFromLeft(128);
    auto badgeBox = clockArea.removeFromLeft(48);
    drawText(g, formatTempo(frame_.bpm), tempoBox, juce::Justification::centredLeft);
    drawText(g, formatBeat(frame_.ppq), beatBox, juce::Justification::centred);
    g.setColour(frame_.hostClock ? rose : dream);
    drawText(g, frame_.hostClock ? "HOST" : "INT", badgeBox, juce::Justification::centredLeft);

    const auto meter = juce::Rectangle<int>(1002, 10, 10, 20);
    g.setColour(bgVoid);
    g.fillRect(meter);
    g.setColour(roseDim);
    g.fillRect(juce::Rectangle<int>(1004, 12, 2, 16));
    g.setColour(meterColour(meterNeedle_));
    g.fillRect(juce::Rectangle<int>(1004, 12, 2, juce::roundToInt(16.0f * meterNeedle_)));

    const auto liveLamp = juce::Rectangle<int>(1040, 8, 126, 22);
    g.setColour(textPrimary);
    g.setFont(monoFont(13.0f, juce::Font::bold));
    drawText(g, (frame_.energy > 0.002f ? "◉ LIVE" : "○ LIVE"), liveLamp, juce::Justification::centredLeft);
}

void Status::paintOver(juce::Graphics& g) {
    if (!frame_.clipping)
        return;

    using namespace undertow::ui::theme;
    const auto flash = 0.32f + 0.68f * static_cast<float>(borderFlash_);
    g.setColour(roseBright.withAlpha(0.32f * flash));
    g.fillRect(topHazardBounds);
    g.fillRect(bottomHazardBounds);

    const auto drawStripes = [&](const juce::Rectangle<int>& area) {
        g.saveState();
        g.reduceClipRegion(area);
        g.setColour(roseBright.withAlpha(0.92f));
        for (int x = area.getX() - 40; x < area.getRight() + 40; x += 24) {
            juce::Path stripe;
            stripe.startNewSubPath(static_cast<float>(x), static_cast<float>(area.getBottom()));
            stripe.lineTo(static_cast<float>(x + 14), static_cast<float>(area.getBottom()));
            stripe.lineTo(static_cast<float>(x + 38), static_cast<float>(area.getY()));
            stripe.lineTo(static_cast<float>(x + 24), static_cast<float>(area.getY()));
            stripe.closeSubPath();
            g.fillPath(stripe);
        }
        g.restoreState();
    };

    drawStripes(topHazardBounds.expanded(0, 8));
    drawStripes(bottomHazardBounds.expanded(0, 8));

    g.setColour(roseBright.withAlpha(0.18f + 0.18f * flash));
    g.drawRect(juce::Rectangle<int>(0, 0, headBounds.getWidth(), headBounds.getHeight()), 2);
}
