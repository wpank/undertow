#include "Look.h"

#include "Theme.h"

#include <algorithm>
#include <cmath>

namespace {

juce::Colour tinted(juce::Colour c, float alpha) { return c.withAlpha(alpha); }

} // namespace

Look::Look() {
    setColour(juce::Slider::rotarySliderFillColourId, undertow::ui::theme::rose);
    setColour(juce::Slider::rotarySliderOutlineColourId, undertow::ui::theme::roseDeep);
    setColour(juce::Slider::thumbColourId, undertow::ui::theme::boneDim);
    setColour(juce::Slider::textBoxTextColourId, undertow::ui::theme::textPrimary);
    setColour(juce::Slider::textBoxOutlineColourId, undertow::ui::theme::border);
    setColour(juce::Slider::textBoxBackgroundColourId, undertow::ui::theme::bgRaised);
    setColour(juce::ToggleButton::textColourId, undertow::ui::theme::textPrimary);
    setColour(juce::ToggleButton::tickColourId, undertow::ui::theme::roseBright);
    setColour(juce::TooltipWindow::textColourId, undertow::ui::theme::boneDim);
}

void Look::install(juce::Component& component) {
    component.setLookAndFeel(this);
    for (int i = 0; i < component.getNumChildComponents(); ++i) {
        if (auto* child = component.getChildComponent(i))
            child->setLookAndFeel(this);
    }
}

void Look::uninstall(juce::Component& component) {
    for (int i = 0; i < component.getNumChildComponents(); ++i) {
        if (auto* child = component.getChildComponent(i))
            child->setLookAndFeel(nullptr);
    }
    component.setLookAndFeel(nullptr);
}

void Look::paint(juce::Graphics& g) {
    ensureCache(g);
    if (cachedLayer_.isValid())
        g.drawImageAt(cachedLayer_, 0, 0);
}

void Look::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                            float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                            juce::Slider&) {
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                               static_cast<float>(width), static_cast<float>(height));
    const auto centre = bounds.getCentre();
    const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f - 4.0f;
    const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    g.setColour(undertow::ui::theme::roseDeep);
    g.drawEllipse(bounds.reduced(4.0f), 1.0f);
    g.setColour(undertow::ui::theme::rose.withAlpha(0.9f));
    juce::Path arc;
    arc.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, rotaryStartAngle, angle, true);
    g.strokePath(arc, juce::PathStrokeType(2.0f));
    g.setColour(undertow::ui::theme::boneDim);
    g.drawLine(centre.x, centre.y, centre.x + std::cos(angle) * (radius - 4.0f),
               centre.y + std::sin(angle) * (radius - 4.0f), 2.0f);
}

void Look::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                            bool, bool) {
    auto area = button.getLocalBounds().toFloat();
    auto tick = area.removeFromLeft(std::min(18.0f, area.getWidth()));
    g.setColour(undertow::ui::theme::bgRaised);
    g.fillRoundedRectangle(tick.reduced(1.0f), 4.0f);
    g.setColour(button.getToggleState() ? undertow::ui::theme::roseBright : undertow::ui::theme::roseDim);
    g.drawRoundedRectangle(tick.reduced(1.0f), 4.0f, 1.2f);
    if (button.getToggleState())
        g.fillRoundedRectangle(tick.reduced(4.5f), 2.0f);
    g.setColour(findColour(juce::ToggleButton::textColourId));
    g.setFont(undertow::ui::theme::sansFont(12.0f));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(22, 0), juce::Justification::centredLeft, 1);
}

juce::Font Look::getLabelFont(juce::Label&) { return undertow::ui::theme::sansFont(12.0f); }
juce::Font Look::getTextButtonFont(juce::TextButton&, int) { return undertow::ui::theme::sansFont(12.0f); }

void Look::ensureCache(juce::Graphics& g) {
    const auto scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    const auto clip = g.getClipBounds();
    const auto size = juce::Point<int>(clip.getWidth(), clip.getHeight());
    if (cachedLayer_.isValid() && cachedScale_ == scale && cachedSize_ == size)
        return;
    cachedScale_ = scale;
    cachedSize_ = size;
    cachedLayer_ = juce::Image(juce::Image::ARGB, juce::jmax(1, juce::roundToInt(size.x * scale)),
                               juce::jmax(1, juce::roundToInt(size.y * scale)), true);
    juce::Graphics ig(cachedLayer_);
    ig.addTransform(juce::AffineTransform::scale(scale));
    drawStaticLayer(ig, scale);
}

void Look::drawStaticLayer(juce::Graphics& g, float) {
    const auto area = juce::Rectangle<float>(0.0f, 0.0f, 1200.0f, 760.0f);
    g.fillAll(undertow::ui::theme::bgVoid);
    g.setColour(undertow::ui::theme::bgVoid);
    g.fillRect(area);
    g.setColour(undertow::ui::theme::bgVoid);
    for (const auto& panel : undertow::ui::theme::controlPanels) {
        g.setColour(undertow::ui::theme::bgRaised);
        g.fillRoundedRectangle(panel.area.toFloat().reduced(18.0f), 6.0f);
        g.setColour(undertow::ui::theme::border);
        g.drawRoundedRectangle(panel.area.toFloat().reduced(18.0f), 6.0f, 1.0f);
        g.setColour(undertow::ui::theme::roseDim);
        g.setFont(undertow::ui::theme::monoFont(11.0f, juce::Font::bold));
        g.drawText(panel.stamp, panel.area.reduced(10, 10), juce::Justification::topLeft, false);
        g.setColour(undertow::ui::theme::borderActive);
        g.drawHorizontalLine(panel.area.getBottom() - 18, panel.area.getX() + 10.0f, panel.area.getRight() - 10.0f);
    }
    g.setColour(tinted(undertow::ui::theme::roseDeep, 0.25f));
    for (int x = 0; x < 1200; x += 48)
        g.drawVerticalLine(x, 56.0f, 656.0f);
    for (int y = 56; y < 656; y += 48)
        g.drawHorizontalLine(y, 16.0f, 1184.0f);
    g.setColour(undertow::ui::theme::scanlineDark);
    for (int y = 0; y < 760; y += 4)
        g.drawHorizontalLine(y, 0.0f, 1200.0f);
    juce::Random rng(0x5EEDBEEF);
    g.setColour(undertow::ui::theme::textGhost.withAlpha(0.25f));
    for (int i = 0; i < 2200; ++i)
        g.fillRect(rng.nextInt(1200), rng.nextInt(760), 1, 1);
    g.setColour(undertow::ui::theme::roseDim.withAlpha(0.18f));
    g.drawRect(area.reduced(0.5f), 1.0f);
}
