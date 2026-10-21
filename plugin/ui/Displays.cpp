#include "Displays.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

float clamp01(float value) {
    return juce::jlimit(0.0f, 1.0f, value);
}

float wrapPhase(float phase) {
    while (phase >= kTwoPi)
        phase -= kTwoPi;
    while (phase < 0.0f)
        phase += kTwoPi;
    return phase;
}

float easeOut(float value) {
    const auto clamped = clamp01(value);
    return 1.0f - (1.0f - clamped) * (1.0f - clamped);
}

juce::Colour heatColour(float amount) {
    const auto clamped = clamp01(amount);
    using namespace undertow::ui::theme;
    if (clamped <= 0.33f)
        return bgVoid.interpolatedWith(roseDeep, clamped / 0.33f);
    if (clamped <= 0.66f)
        return roseDeep.interpolatedWith(roseEmber, (clamped - 0.33f) / 0.33f);
    return roseEmber.interpolatedWith(rose, (clamped - 0.66f) / 0.34f);
}

juce::String noteLabel(int midiNote) {
    if (midiNote < 0)
        return {};
    return juce::String(midiNote);
}

} // namespace

Displays::Displays() {
    ensureTunnelTables();
}

void Displays::advance(const UiFrame& frame, double seconds) {
    frame_ = frame;
    ensureTunnelTables();

    const auto dt = static_cast<float>(juce::jlimit(0.0, 0.1, seconds));
    tunnelPhase_ = wrapPhase(static_cast<float>(tunnelPhase_ + dt * (0.4f + frame.energy * 2.6f)));
    linePhase_ = wrapPhase(static_cast<float>(linePhase_ + dt * (0.6f + frame.resonance * 3.4f)));
    tunnelEnergy_ += (frame.energy - tunnelEnergy_) * juce::jlimit(0.0f, 1.0f, dt * 4.0f);

    const float noteTarget = frame.note >= 0 ? 1.0f : 0.0f;
    const float idleTarget = frame.note >= 0 ? 0.0f : 1.0f;
    noteFade_ += (noteTarget - noteFade_) * juce::jlimit(0.0f, 1.0f, dt * (frame.note >= 0 ? 10.0f : 4.0f));
    idleFade_ += (idleTarget - idleFade_) * juce::jlimit(0.0f, 1.0f, dt * 3.0f);
    if (frame.note >= 0)
        displayedNote_ = frame.note;
    else if (noteFade_ < 0.02f)
        displayedNote_ = -1;

    updateScopeHistory(frame);
    renderTunnel();
}

void Displays::paint(juce::Graphics& g) {
    const auto chest = undertow::ui::theme::chestBounds;
    g.saveState();
    g.reduceClipRegion(chest);
    paintTunnel(g);
    paintAtField(g);
    paintScope(g);
    paintBone(g);
    g.restoreState();
}

void Displays::ensureTunnelTables() {
    const auto chest = undertow::ui::theme::chestBounds;
    const int width = chest.getWidth() / kTunnelScale;
    const int height = chest.getHeight() / kTunnelScale;
    if (width <= 0 || height <= 0)
        return;

    if (tunnelImage_.isValid() && tunnelImage_.getWidth() == width && tunnelImage_.getHeight() == height && !tunnelTable_.empty())
        return;

    tunnelImage_ = juce::Image(juce::Image::ARGB, width, height, true);
    tunnelTable_.assign(static_cast<std::size_t>(width * height), {});

    const auto cx = (static_cast<float>(width) - 1.0f) * 0.5f;
    const auto cy = (static_cast<float>(height) - 1.0f) * 0.5f;
    const auto invRadius = 1.0f / std::sqrt(cx * cx + cy * cy);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            auto& sample = tunnelTable_[static_cast<std::size_t>(y * width + x)];
            const auto dx = (static_cast<float>(x) - cx);
            const auto dy = (static_cast<float>(y) - cy) * 1.1f;
            const auto radius = std::sqrt(dx * dx + dy * dy) * invRadius;
            sample.x = static_cast<float>(x);
            sample.y = static_cast<float>(y);
            sample.distance = radius;
            sample.angle = std::atan2(dy, dx);
            sample.vignette = clamp01(1.0f - radius * 0.9f);
        }
    }
}

void Displays::renderTunnel() {
    if (!tunnelImage_.isValid() || tunnelTable_.empty())
        return;

    juce::Image::BitmapData pixels(tunnelImage_, juce::Image::BitmapData::writeOnly);
    const auto width = tunnelImage_.getWidth();
    const auto height = tunnelImage_.getHeight();

    const float energy = clamp01(0.08f + tunnelEnergy_ * 0.92f);
    const float motion = static_cast<float>(tunnelPhase_);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto& sample = tunnelTable_[static_cast<std::size_t>(y * width + x)];
            const float depth = 1.0f / (0.18f + sample.distance * 1.6f);
            const float rings = 0.5f + 0.5f * std::sin(depth * 11.0f - motion * 4.0f + sample.angle * 1.7f);
            const float sweep = 0.5f + 0.5f * std::sin(sample.angle * 5.0f + motion * 3.0f - sample.distance * 8.0f);
            const float lattice = 0.5f + 0.5f * std::sin((sample.x + sample.y) * 0.35f + motion * 5.0f);
            const float intensity = clamp01((0.03f + energy * 0.45f) * sample.vignette + energy * (rings * 0.42f + sweep * 0.22f + lattice * 0.08f));
            pixels.setPixelColour(x, y, heatColour(intensity));
        }
    }
}

void Displays::updateScopeHistory(const UiFrame& frame) {
    const auto scope = undertow::ui::theme::scopeBounds.toFloat();
    juce::Path path;
    const auto width = static_cast<float>(scope.getWidth());
    const auto height = static_cast<float>(scope.getHeight());
    const auto centreY = scope.getCentreY();
    const auto halfHeight = height * 0.44f;
    bool started = false;

    for (std::size_t i = 0; i < frame.scope.size(); ++i) {
        const float x = scope.getX() + (static_cast<float>(i) / static_cast<float>(frame.scope.size() - 1)) * width;
        const float y = centreY - juce::jlimit(-1.0f, 1.0f, frame.scope[i] * 2.0f) * halfHeight;
        if (!started) {
            path.startNewSubPath(x, y);
            started = true;
        } else {
            path.lineTo(x, y);
        }
    }

    historyHead_ = (historyHead_ + 1) % kPhosphorHistory;
    scopeHistory_[static_cast<std::size_t>(historyHead_)] = path;
    historyCount_ = std::min(historyCount_ + 1, kPhosphorHistory);
}

void Displays::paintTunnel(juce::Graphics& g) const {
    const auto chest = undertow::ui::theme::chestBounds;
    if (tunnelImage_.isValid())
        g.drawImage(tunnelImage_, chest.toFloat());
    g.setColour(undertow::ui::theme::bgVoid.withAlpha(0.18f + idleFade_ * 0.18f));
    g.fillRect(chest);
}

void Displays::paintAtField(juce::Graphics& g) const {
    const auto scope = undertow::ui::theme::scopeBounds.toFloat();
    const auto centre = scope.getCentre();
    const float resonance = clamp01(frame_.resonance);
    const float alpha = 0.12f + 0.22f * resonance + 0.06f * tunnelEnergy_;

    for (int i = 0; i < 4; ++i) {
        const float t = static_cast<float>(i) / 3.0f;
        const float insetX = 22.0f + t * 38.0f;
        const float insetY = 16.0f + t * 16.0f;
        juce::Path line;
        const float left = scope.getX() - insetX;
        const float right = scope.getRight() + insetX;
        const float top = scope.getY() - insetY;
        const float bottom = scope.getBottom() + insetY;
        const float bow = 12.0f + resonance * 38.0f + t * 10.0f;
        const float wobble = std::sin(static_cast<float>(linePhase_) * (1.2f + t * 0.17f) + t * 1.9f) * (4.0f + resonance * 10.0f);

        line.startNewSubPath(left, centre.y);
        line.quadraticTo(centre.x, top - bow - wobble, right, centre.y);
        line.startNewSubPath(left, centre.y);
        line.quadraticTo(centre.x, bottom + bow + wobble, right, centre.y);
        line.startNewSubPath(centre.x, top);
        line.quadraticTo(left - bow - wobble, centre.y, centre.x, bottom);
        line.startNewSubPath(centre.x, top);
        line.quadraticTo(right + bow + wobble, centre.y, centre.x, bottom);

        const auto colour = undertow::ui::theme::roseDim.interpolatedWith(undertow::ui::theme::roseBright, 0.25f + t * 0.15f)
                                .withAlpha(alpha * (0.6f + t * 0.1f));
        g.setColour(colour);
        g.strokePath(line, juce::PathStrokeType(0.9f + t * 0.25f));
    }
}

void Displays::paintScope(juce::Graphics& g) const {
    const auto scope = undertow::ui::theme::scopeBounds;
    g.setColour(undertow::ui::theme::bgMid.withAlpha(0.22f + idleFade_ * 0.08f));
    g.fillRect(scope);

    for (int age = historyCount_ - 1; age >= 0; --age) {
        const int index = (historyHead_ - age + kPhosphorHistory) % kPhosphorHistory;
        const float freshness = 1.0f - static_cast<float>(age) / static_cast<float>(kPhosphorHistory);
        const auto colour = undertow::ui::theme::roseDeep.interpolatedWith(undertow::ui::theme::roseBright, freshness)
                                .withAlpha(0.12f + freshness * 0.68f);
        g.setColour(colour);
        g.strokePath(scopeHistory_[static_cast<std::size_t>(index)], juce::PathStrokeType(1.0f + freshness * 1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void Displays::paintBone(juce::Graphics& g) const {
    if (displayedNote_ < 0 || noteFade_ <= 0.01f)
        return;

    const auto chest = undertow::ui::theme::chestBounds;
    auto area = juce::Rectangle<float>(chest.getX() + 170.0f, chest.getY() + 54.0f, 236.0f, 92.0f);
    const float fade = easeOut(noteFade_);
    g.setColour(undertow::ui::theme::bgVoid.withAlpha(0.18f * fade));
    g.fillRoundedRectangle(area.expanded(10.0f, 8.0f), 8.0f);

    g.setColour(undertow::ui::theme::bone.withAlpha(0.96f * fade));
    g.setFont(undertow::ui::theme::monoFont(72.0f, juce::Font::bold));
    g.drawFittedText(noteLabel(displayedNote_), area.toNearestInt(), juce::Justification::centred, 1);

    if (frame_.noteHz > 0.0f) {
        g.setColour(undertow::ui::theme::boneDim.withAlpha(0.8f * fade));
        g.setFont(undertow::ui::theme::monoFont(18.0f));
        g.drawFittedText(juce::String(frame_.noteHz, 1) + " HZ", area.withTrimmedTop(58.0f).toNearestInt(), juce::Justification::centredBottom, 1);
    }
}
