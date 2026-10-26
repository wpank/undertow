#include "Status.h"

#include "Theme.h"

#include <algorithm>
#include <cmath>

namespace {

float clamp01(float value) {
    return juce::jlimit(0.0f, 1.0f, value);
}

juce::Colour meterColour(float value) {
    using namespace undertow::ui::theme;
    return roseDeep.interpolatedWith(rose, clamp01(value));
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

    auto clock = juce::Rectangle<float>(36.0f, 8.0f, 136.0f, 28.0f);
    g.setColour(bgRaised.withAlpha(0.92f));
    g.fillRoundedRectangle(clock, 8.0f);
    g.setColour(border.withAlpha(0.7f));
    g.drawRoundedRectangle(clock, 8.0f, 1.0f);
    g.setColour(rose.withAlpha(0.9f));
    g.setFont(sansFont(16.0f, juce::Font::bold));
    g.drawText(frame_.hostClock ? "HEAD / HOST" : "HEAD / FREE", clock.toNearestInt(), juce::Justification::centredLeft, false);

    const auto needleBox = juce::Rectangle<float>(196.0f, 8.0f, 84.0f, 28.0f);
    g.setColour(bgRaised.withAlpha(0.92f));
    g.fillRoundedRectangle(needleBox, 8.0f);
    g.setColour(border.withAlpha(0.7f));
    g.drawRoundedRectangle(needleBox, 8.0f, 1.0f);
    g.setColour(textPrimary);
    g.setFont(monoFont(16.0f, juce::Font::bold));
    g.drawText(juce::String(frame_.bpm, 0) + " BPM", needleBox.toNearestInt(), juce::Justification::centred, false);

    const auto meter = juce::Rectangle<float>(304.0f, 8.0f, 184.0f, 28.0f);
    g.setColour(bgRaised.withAlpha(0.92f));
    g.fillRoundedRectangle(meter, 8.0f);
    g.setColour(border.withAlpha(0.7f));
    g.drawRoundedRectangle(meter, 8.0f, 1.0f);
    g.setColour(roseDim);
    g.fillRect(meter.reduced(10.0f, 10.0f));
    g.setColour(meterColour(meterNeedle_));
    g.fillRect(meter.withWidth(meter.getWidth() * meterNeedle_).reduced(10.0f, 10.0f));

    const auto pump = juce::Rectangle<float>(520.0f, 8.0f, 420.0f, 28.0f);
    g.setColour(bgRaised.withAlpha(0.92f));
    g.fillRoundedRectangle(pump, 8.0f);
    g.setColour(border.withAlpha(0.7f));
    g.drawRoundedRectangle(pump, 8.0f, 1.0f);
    for (int i = 0; i < 28; ++i) {
        const float phase = std::fmod(static_cast<float>(stripeOffset_) + i * 8.0f + static_cast<float>(frame_.ppq) * 6.0f, 40.0f) / 40.0f;
        const float x = pump.getX() + phase * pump.getWidth();
        const float amp = 0.35f + 0.65f * rumbleNeedle_;
        g.setColour(roseEmber.interpolatedWith(roseBright, amp).withAlpha(0.12f + amp * 0.34f));
        g.drawVerticalLine(juce::roundToInt(x), pump.getY(), pump.getBottom());
    }
    g.setColour(roseDim);
    g.setFont(sansFont(14.0f, juce::Font::bold));
    g.drawText("RUMBLE", pump.toNearestInt(), juce::Justification::centredLeft, false);
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
