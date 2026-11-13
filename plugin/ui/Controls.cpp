#include "Controls.h"

#include "Texts.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <sstream>
#include <string_view>

#include <undertow/params.h>

namespace {

using undertow::ui::theme::ParameterPlace;
using undertow::ui::theme::parameterPlaces;
using undertow::ui::theme::legend;
using undertow::ui::theme::numberFont;
using undertow::ui::theme::rose;
using undertow::ui::theme::roseBright;
using undertow::ui::theme::roseDim;
using undertow::ui::theme::rule2;
using undertow::ui::theme::textDim;
using undertow::ui::theme::textGhost;
using undertow::ui::theme::value;
using undertow::ui::theme::legendFont;
using undertow::ui::theme::glass;
using undertow::ui::theme::roseDeep;
using undertow::ui::theme::roseEmber;

constexpr float kPi = juce::MathConstants<float>::pi;
constexpr float kKnobStart = 1.25f * kPi;
constexpr float kKnobEnd = 2.75f * kPi;
constexpr float kTop = 2.0f * kPi;

bool startsWith(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

bool endsWith(std::string_view text, std::string_view suffix) {
    return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

juce::String trimZeros(std::string text) {
    if (auto dot = text.find('.'); dot != std::string::npos) {
        while (!text.empty() && text.back() == '0')
            text.pop_back();
        if (!text.empty() && text.back() == '.')
            text.pop_back();
    }
    if (text == "-0")
        text = "0";
    return juce::String(text);
}

juce::String fixed(double value, int decimals, bool trim = true) {
    std::ostringstream stream;
    stream.setf(std::ios::fixed, std::ios::floatfield);
    stream.precision(decimals);
    stream << value;
    auto text = stream.str();
    if (trim)
        return trimZeros(text);
    return juce::String(text);
}

juce::String signedFixed(double value, int decimals, bool trim = true) {
    std::ostringstream stream;
    stream.setf(std::ios::fixed, std::ios::floatfield);
    stream.precision(decimals);
    if (value > 0.0)
        stream << '+';
    stream << value;
    auto text = stream.str();
    if (trim)
        return trimZeros(text);
    return juce::String(text);
}

juce::String noteName(int midi) {
    static constexpr std::array<const char*, 12> kNames{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    midi = std::clamp(midi, 0, 127);
    return juce::String(kNames[static_cast<std::size_t>(midi % 12)]) + juce::String(midi / 12 - 1);
}

juce::String choiceLabel(const undertow::ParamSpec& spec, int index, bool uppercase = true) {
    if (spec.choices == nullptr || spec.numChoices <= 0)
        return {};
    index = juce::jlimit(0, spec.numChoices - 1, index);
    auto text = juce::String(spec.choices[index]);
    return uppercase ? text.toUpperCase() : text;
}

juce::String readoutFor(const undertow::ParamSpec& spec, double rawValue, bool hostClock) {
    const auto id = std::string_view(spec.id);

    if (id == "seq_bpm")
        return hostClock ? juce::String("HOST") : fixed(rawValue, 1, false);
    if (endsWith(id, "_pitch") && startsWith(id, "step"))
        return signedFixed(std::round(rawValue), 0);
    if (endsWith(id, "_vel"))
        return rawValue <= 0.0 ? juce::String("REST") : fixed(rawValue, 2, false);
    if (id == "seq_root")
        return noteName(static_cast<int>(std::lround(rawValue)));
    if (id == "seq_rate" || id == "rumble_rate")
        return choiceLabel(spec, static_cast<int>(std::lround(rawValue)), false);
    if (id == "seq_length")
        return juce::String(static_cast<int>(std::lround(rawValue)));
    if (id == "vco2_tune")
        return signedFixed(rawValue, 1) + " st";
    if (id == "pitch_env")
        return fixed(rawValue, 1) + " st";
    if (endsWith(id, "_decay") && (startsWith(id, "pitch") || startsWith(id, "vcf") || startsWith(id, "vca")))
        return rawValue < 1000.0 ? fixed(rawValue, 1) + " ms" : fixed(rawValue / 1000.0, 1) + " s";
    if (id == "rumble_decay")
        return fixed(rawValue, 1) + " s";
    if (id == "vcf_cutoff" || id == "rumble_tone")
        return rawValue < 1000.0 ? fixed(rawValue, 1) + " Hz" : fixed(rawValue / 1000.0, 1) + " kHz";
    if (id == "vcf_env")
        return fixed(rawValue, 1) + " oct";
    if (id == "master_gain")
        return signedFixed(rawValue, 1, false) + " dB";
    if (spec.kind == undertow::ParamKind::Choice)
        return choiceLabel(spec, static_cast<int>(std::lround(rawValue)));
    if (spec.kind == undertow::ParamKind::Int)
        return juce::String(static_cast<int>(std::lround(rawValue)));
    if (spec.unit != nullptr && spec.unit[0] != '\0')
        return fixed(rawValue, 1) + " " + juce::String(spec.unit);
    return fixed(rawValue, 2, false);
}

void drawText(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> box, juce::Justification j,
              const juce::Colour& colour, const juce::Font& font) {
    g.setColour(colour);
    g.setFont(font);
    texts::draw(g, text, box, j);
}

juce::Rectangle<int> anchoredTextBox(float anchorX, float baseline, int width, int height, juce::Justification justification) {
    const auto y = juce::roundToInt(std::floor(baseline - 13.0f));
    int x = juce::roundToInt(std::floor(anchorX));

    if (justification.testFlags(juce::Justification::horizontallyCentred))
        x -= width / 2;
    else if (justification.testFlags(juce::Justification::right))
        x -= width;

    return {x, y, width, height};
}

void drawRule(juce::Graphics& g, float x1, float x2, float y, const juce::Colour& colour, float thickness = 1.0f) {
    g.setColour(colour);
    g.drawLine(x1, y, x2, y, thickness);
}

void drawDashedVertical(juce::Graphics& g, float x, float y1, float y2, const juce::Colour& colour) {
    g.setColour(colour);
    for (float y = y1; y < y2; y += 6.0f)
        g.drawLine(x, y, x, std::min(y + 3.0f, y2), 1.0f);
}

bool isSwitchId(const juce::String& id) {
    return id == "vco1_wave" || id == "vco2_wave" || id == "sub_octave" || id == "vca_attack" || id == "seq_mode";
}

juce::Point<float> pointAtClockAngle(juce::Point<float> centre, float radius, float angle) {
    return {centre.x + radius * std::sin(angle), centre.y - radius * std::cos(angle)};
}

void strokeArc(juce::Graphics& g, juce::Point<float> centre, float radius, float startAngle, float endAngle,
               const juce::Colour& colour, float thickness) {
    juce::Path path;
    path.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour(colour);
    g.strokePath(path, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void fillDot(juce::Graphics& g, juce::Point<float> centre, float diameter, const juce::Colour& colour) {
    g.setColour(colour);
    g.fillEllipse(centre.x - diameter * 0.5f, centre.y - diameter * 0.5f, diameter, diameter);
}

void drawWaveGlyph(juce::Graphics& g, juce::Rectangle<float> segment, const juce::Colour& colour, bool triangle) {
    const auto centre = segment.getCentre();
    const float left = centre.x - 6.5f;
    const float right = centre.x + 6.5f;
    const float top = centre.y - 3.5f;
    const float middle = centre.y;
    const float bottom = centre.y + 3.5f;
    const float quarter = left + 3.25f;
    const float threeQuarter = right - 3.25f;

    juce::Path path;
    path.startNewSubPath(left, middle);
    if (triangle) {
        path.lineTo(quarter, top);
        path.lineTo(threeQuarter, bottom);
        path.lineTo(right, middle);
    } else {
        path.lineTo(left, top);
        path.lineTo(centre.x, top);
        path.lineTo(centre.x, bottom);
        path.lineTo(right, bottom);
        path.lineTo(right, middle);
    }

    g.setColour(colour);
    g.strokePath(path, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

int detentsFor(const undertow::ParamSpec& spec) {
    const auto id = std::string_view(spec.id);
    if (id == "seq_rate" || id == "seq_length")
        return 8;
    if (id == "rumble_rate")
        return 4;
    return 0;
}

}  // namespace

struct Controls::Entry {
    const undertow::ParamSpec* spec = nullptr;
    const ParameterPlace* place = nullptr;
    Knob* knob = nullptr;
    Switch* sw = nullptr;
};

Knob::Knob(bool bipolar, int detents) : bipolar_(bipolar), detents_(detents) {
    setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setMouseDragSensitivity(250);
    setRotaryParameters(kKnobStart, kKnobEnd, true);
    setDoubleClickReturnValue(true, 0.0);
    setWantsKeyboardFocus(false);
}

void Knob::setLight(float light) {
    light_ = juce::jlimit(0.0f, 1.0f, light);
    repaint();
}

void Knob::setHovered(bool hovered) {
    hovered_ = hovered;
    repaint();
}

void Knob::setDimmed(bool dimmed) {
    dimmed_ = dimmed;
    repaint();
}

bool Knob::isHovered() const {
    return hovered_ || dragging_;
}

void Knob::dropCache() {
    cachedScale_ = 0.0f;
    cachedWidth_ = 0;
    cachedHeight_ = 0;
    underlayCache_ = {};
    overlayCache_ = {};
}

void Knob::mouseEnter(const juce::MouseEvent& e) {
    hovered_ = true;
    juce::Slider::mouseEnter(e);
    repaint();
}

void Knob::mouseExit(const juce::MouseEvent& e) {
    hovered_ = false;
    juce::Slider::mouseExit(e);
    repaint();
}

void Knob::mouseDown(const juce::MouseEvent& e) {
    dragging_ = true;
    juce::Slider::mouseDown(e);
    repaint();
}

void Knob::mouseUp(const juce::MouseEvent& e) {
    juce::Slider::mouseUp(e);
    dragging_ = false;
    repaint();
}

void Knob::setFormatter(std::function<juce::String(double)> formatter) {
    formatter_ = std::move(formatter);
}

juce::String Knob::getTextFromValue(double value) {
    if (formatter_)
        return formatter_(value);
    return juce::Slider::getTextFromValue(value);
}

void Knob::rebuildCaches(float scale) {
    const auto bounds = getLocalBounds().toFloat().reduced(6.0f);
    const auto centre = bounds.getCentre();
    const auto radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float trackRadius = radius - 2.5f;
    const float capRadius = 0.7f * trackRadius;
    const auto pixelWidth = std::max(1, juce::roundToInt(static_cast<float>(getWidth()) * scale));
    const auto pixelHeight = std::max(1, juce::roundToInt(static_cast<float>(getHeight()) * scale));

    underlayCache_ = juce::Image(juce::Image::ARGB, pixelWidth, pixelHeight, true);
    overlayCache_ = juce::Image(juce::Image::ARGB, pixelWidth, pixelHeight, true);

    {
        juce::Graphics gg(underlayCache_);
        gg.addTransform(juce::AffineTransform::scale(scale));
        strokeArc(gg, centre, trackRadius, kKnobStart, kKnobEnd, undertow::ui::theme::track, 2.0f);

        if (detents_ > 1) {
            const int total = juce::jlimit(2, 8, detents_);
            const float proportion = static_cast<float>(valueToProportionOfLength(getValue()));
            const int litIndex = juce::jlimit(0, total - 1, juce::roundToInt(proportion * static_cast<float>(total - 1)));
            for (int i = 0; i < total; ++i) {
                const float t = total == 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(total - 1);
                const float angle = kKnobStart + t * (kKnobEnd - kKnobStart);
                const auto dot = pointAtClockAngle(centre, trackRadius + 4.6f, angle);
                fillDot(gg, dot, i == litIndex ? 2.5f : 1.9f, i == litIndex ? roseBright : textGhost);
            }
        } else if (bipolar_) {
            const auto start = pointAtClockAngle(centre, trackRadius + 3.0f, kTop);
            const auto end = pointAtClockAngle(centre, trackRadius + 6.0f, kTop);
            gg.setColour(textGhost);
            gg.drawLine(start.x, start.y, end.x, end.y, 1.2f);
        }
    }

    {
        juce::Graphics gg(overlayCache_);
        gg.addTransform(juce::AffineTransform::scale(scale));
        gg.setColour(juce::Colours::black.withAlpha(0.55f));
        gg.fillEllipse(centre.x - capRadius - 2.2f, centre.y - capRadius + 1.6f - 2.2f, (capRadius + 2.2f) * 2.0f,
                       (capRadius + 2.2f) * 2.0f);

        gg.setColour(juce::Colour(0xff040305));
        gg.fillEllipse(centre.x - capRadius - 1.2f, centre.y - capRadius - 1.2f, (capRadius + 1.2f) * 2.0f,
                       (capRadius + 1.2f) * 2.0f);

        juce::ColourGradient gradient(undertow::ui::theme::capHi, centre.x - capRadius * 0.28f, centre.y - capRadius * 0.40f,
                                      undertow::ui::theme::capLo, centre.x + capRadius, centre.y + capRadius, true);
        gradient.addColour(0.55, undertow::ui::theme::capMid.withAlpha(0.55f));
        gg.setGradientFill(gradient);
        gg.fillEllipse(centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f, capRadius * 2.0f);

        gg.setColour(undertow::ui::theme::capRim);
        gg.drawEllipse(centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f, capRadius * 2.0f, 1.0f);

        strokeArc(gg, centre, capRadius - 0.5f, kTop - 62.0f * kPi / 180.0f, kTop + 62.0f * kPi / 180.0f,
                  rose.withAlpha(0.16f), 1.0f);
    }

    cachedScale_ = scale;
    cachedWidth_ = getWidth();
    cachedHeight_ = getHeight();
}

void Knob::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat().reduced(6.0f);
    const auto centre = bounds.getCentre();
    const auto radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float trackRadius = radius - 2.5f;
    const float capRadius = 0.7f * trackRadius;

    const auto light = juce::jlimit(0.0f, 1.0f, light_);
    const bool active = hovered_ || dragging_;
    const auto arcColour = (active || light > 0.0f) ? roseBright : rose;
    const auto pointerColour = (active || light > 0.5f) ? roseBright : undertow::ui::theme::pointer;
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (!underlayCache_.isValid() || !overlayCache_.isValid() || cachedScale_ != scale || cachedWidth_ != getWidth() ||
        cachedHeight_ != getHeight()) {
        rebuildCaches(scale);
    }

    const float proportion = static_cast<float>(valueToProportionOfLength(getValue()));
    const float angle = kKnobStart + proportion * (kKnobEnd - kKnobStart);
    const float pointerWidth = bounds.getWidth() >= 44.0f ? 2.0f : 1.8f;
    const auto pointerStart = pointAtClockAngle(centre, capRadius * 0.18f, angle);
    const auto pointerEnd = pointAtClockAngle(centre, capRadius * 0.86f, angle);

    auto drawLive = [&] {
        const auto imageTransform = juce::AffineTransform::scale(1.0f / scale);
        g.drawImageTransformed(underlayCache_, imageTransform, false);

        const bool glowing = active || light > 0.0f;
        const float arcStart = bipolar_ ? std::min(kTop, angle) : kKnobStart;
        const float arcEnd = bipolar_ ? std::max(kTop, angle) : angle;
        if (glowing) {
            strokeArc(g, centre, trackRadius, arcStart, arcEnd, roseBright.withAlpha(0.22f), 7.0f);
            strokeArc(g, centre, trackRadius, arcStart, arcEnd, roseBright.withAlpha(0.35f), 4.5f);
        }
        if (!bipolar_ || std::abs(angle - kTop) > 1.0e-4f)
            strokeArc(g, centre, trackRadius, arcStart, arcEnd, arcColour, 2.4f);

        g.drawImageTransformed(overlayCache_, imageTransform, false);

        g.setColour(pointerColour);
        g.drawLine(pointerStart.x, pointerStart.y, pointerEnd.x, pointerEnd.y, pointerWidth);
    };

    if (dimmed_)
        g.beginTransparencyLayer(0.32f);
    drawLive();
    if (dimmed_)
        g.endTransparencyLayer();
}

Switch::Switch(juce::RangedAudioParameter& parameter, std::vector<juce::String> labels, bool numberLabels)
    : parameter_(parameter), labels_(std::move(labels)), numberLabels_(numberLabels) {
    attachment_ = std::make_unique<juce::ParameterAttachment>(parameter_, [this](float) { repaint(); });
    attachment_->sendInitialUpdate();
    setWantsKeyboardFocus(false);
}

int Switch::segments() const {
    return static_cast<int>(labels_.size());
}

int Switch::selected() const {
    const auto chosen = static_cast<int>(std::lround(parameter_.convertFrom0to1(parameter_.getValue())));
    return juce::jlimit(0, segments() - 1, chosen);
}

juce::Rectangle<int> Switch::segmentBounds(int index) const {
    if (index < 0 || index >= segments())
        return {};

    const auto bounds = getLocalBounds();
    const int baseWidth = bounds.getWidth() / std::max(1, segments());
    const int left = bounds.getX() + index * baseWidth;
    const int right = index == segments() - 1 ? bounds.getRight() : left + baseWidth;
    return {left, bounds.getY(), right - left, bounds.getHeight()};
}

void Switch::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    const auto selectedIndex = selected();
    const auto segmentCount = segments();
    const auto segmentWidth = segmentCount > 0 ? bounds.getWidth() / static_cast<float>(segmentCount) : bounds.getWidth();

    juce::ColourGradient face(juce::Colour(0xff09070b), bounds.getCentreX(), bounds.getY(), glass, bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill(face);
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(rule2);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 3.0f, 1.0f);

    for (int i = 0; i < segmentCount; ++i) {
        const float x = bounds.getX() + segmentWidth * static_cast<float>(i);
        const float w = (i == segmentCount - 1) ? (bounds.getRight() - x) : segmentWidth;
        const auto segment = juce::Rectangle<float>(x, bounds.getY(), w, bounds.getHeight());
        const auto inner = segment.reduced(2.0f, 2.0f);
        const bool on = i == selectedIndex;
        if (on) {
            juce::ColourGradient fill(roseEmber.withAlpha(0.65f), inner.getCentreX(), inner.getY(), roseDeep.withAlpha(0.55f),
                                      inner.getCentreX(), inner.getBottom(), false);
            g.setGradientFill(fill);
            g.fillRoundedRectangle(inner, 2.0f);
            g.setColour(rose.withAlpha(0.55f));
            g.drawRoundedRectangle(inner.reduced(0.5f), 2.0f, 1.0f);
        }

        if (i > 0 && i != selectedIndex && i - 1 != selectedIndex) {
            g.setColour(rule2);
            g.drawLine(segment.getX(), segment.getY() + 6.0f, segment.getX(), segment.getBottom() - 6.0f, 1.0f);
        }

        if (labels_.size() == 2 && labels_[0] == "triangle" && labels_[1] == "square") {
            drawWaveGlyph(g, inner, on ? roseBright : textDim, i == 0);
        } else if (!labels_[static_cast<std::size_t>(i)].isEmpty()) {
            const auto font = numberLabels_ ? numberFont(9.5f, 0.2f) : legendFont(7.5f, 1.1f);
            const auto colour = on ? roseBright : textDim;
            drawText(g, labels_[static_cast<std::size_t>(i)], segment.toNearestInt().reduced(2, 0), juce::Justification::centred,
                     colour, font);
        }
    }
}

void Switch::mouseUp(const juce::MouseEvent& e) {
    const int width = getWidth();
    if (segments() <= 0 || width <= 0)
        return;

    const int segment = juce::jlimit(0, segments() - 1, (e.position.x >= 0.0f ? static_cast<int>(e.position.x) : 0) * segments() / width);
    parameter_.beginChangeGesture();
    parameter_.setValueNotifyingHost(parameter_.convertTo0to1(static_cast<float>(segment)));
    parameter_.endChangeGesture();
}

Controls::Controls() = default;
Controls::~Controls() = default;

const ParameterPlace* Controls::placeFor(const juce::String& id) {
    for (const auto& place : parameterPlaces()) {
        if (id == place.id)
            return &place;
    }
    return nullptr;
}

bool Controls::isSwitchId(const juce::String& id) {
    return ::isSwitchId(id);
}

void Controls::attach(juce::AudioProcessorValueTreeState& state, juce::Component& window) {
    detach();
    entries_.reserve(static_cast<std::size_t>(undertow::kNumParams));
    sliderAttachments_.reserve(static_cast<std::size_t>(undertow::kNumParams));

    for (int i = 0; i < undertow::kNumParams; ++i) {
        const auto& spec = undertow::paramSpec(i);
        const auto* place = placeFor(spec.id);
        if (place == nullptr)
            continue;

        if (isSwitchId(spec.id))
            attachSwitch(state, window, spec, *place);
        else
            attachKnob(state, window, spec, *place);
    }
}

void Controls::detach() {
    entries_.clear();
    sliderAttachments_.clear();
}

juce::Component* Controls::controlFor(const juce::String& parameterId) const {
    for (const auto& entry : entries_) {
        if (entry.spec != nullptr && parameterId == entry.spec->id)
            return entry.knob != nullptr ? static_cast<juce::Component*>(entry.knob) : static_cast<juce::Component*>(entry.sw);
    }
    return nullptr;
}

void Controls::setLight(int step, float light) {
    if (step < 0 || step >= 8)
        return;
    stepLights_[static_cast<std::size_t>(step)] = juce::jlimit(0.0f, 1.0f, light);
    if (auto* knob = dynamic_cast<Knob*>(controlFor(juce::String("step") + juce::String(step + 1) + "_pitch")))
        knob->setLight(stepLights_[static_cast<std::size_t>(step)]);
    if (auto* knob = dynamic_cast<Knob*>(controlFor(juce::String("step") + juce::String(step + 1) + "_vel")))
        knob->setLight(stepLights_[static_cast<std::size_t>(step)]);
}

void Controls::setColumnDim(int step, bool dim) {
    if (step < 0 || step >= 8)
        return;
    stepDimmed_[static_cast<std::size_t>(step)] = dim;
    if (auto* knob = dynamic_cast<Knob*>(controlFor(juce::String("step") + juce::String(step + 1) + "_pitch")))
        knob->setDimmed(dim);
    if (auto* knob = dynamic_cast<Knob*>(controlFor(juce::String("step") + juce::String(step + 1) + "_vel")))
        knob->setDimmed(dim);
}

void Controls::setHostClock(bool host) {
    hostClock_ = host;
    if (auto* knob = dynamic_cast<Knob*>(controlFor("seq_bpm")))
        knob->setDimmed(host);
}

void Controls::setHover(const juce::String& parameterId) {
    for (auto& entry : entries_) {
        if (entry.knob != nullptr)
            entry.knob->setHovered(parameterId == entry.spec->id);
    }
}

void Controls::paint(juce::Graphics& g) {
    // Section legends and row labels.
    drawText(g, "SEQUENCER", {24, 69, 100, 18}, juce::Justification::left, rose, legendFont(9.0f, 2.3f));
    drawRule(g, 114.0f, 1176.0f, 78.5f, rose.withAlpha(0.2f));

    drawText(g, "VCO", {24, 337, 100, 18}, juce::Justification::left, rose, legendFont(9.0f, 2.3f));
    drawRule(g, 58.0f, 368.0f, 346.5f, rose.withAlpha(0.2f));
    drawText(g, "VCF", {400, 337, 100, 18}, juce::Justification::left, rose, legendFont(9.0f, 2.3f));
    drawRule(g, 428.0f, 628.0f, 346.5f, rose.withAlpha(0.2f));
    drawText(g, "VCA", {656, 337, 80, 18}, juce::Justification::left, rose, legendFont(9.0f, 2.3f));
    drawRule(g, 684.0f, 760.0f, 346.5f, rose.withAlpha(0.2f));

    drawText(g, "RUMBLE", {24, 613, 120, 18}, juce::Justification::left, rose, legendFont(9.0f, 2.3f));
    drawRule(g, 114.0f, 760.0f, 622.0f, rose.withAlpha(0.2f));
    drawText(g, "OUTPUT", {784, 613, 120, 18}, juce::Justification::left, rose, legendFont(9.0f, 2.3f));
    drawRule(g, 872.0f, 1176.0f, 622.0f, rose.withAlpha(0.2f));

    for (const auto& [text, baseline] : std::array<std::pair<const char*, float>, 3>{{{"STEP", 127.0f}, {"PITCH", 203.0f}, {"VELOCITY", 279.0f}}})
        drawText(g, text, {24, juce::roundToInt(baseline - 13.0f), 88, 18}, juce::Justification::left, legend, legendFont(8.0f, 1.1f));

    for (const auto& [text, x] : std::array<std::pair<const char*, float>, 6>{{{"VCO 1", 56.0f}, {"VCO 2", 120.0f}, {"NOISE", 184.0f}, {"SUB", 248.0f}, {"PITCH", 336.0f}, {"ENV", 596.0f}}})
        drawText(g, text, {juce::roundToInt(x - 24.0f), 359, 48, 16}, juce::Justification::centred, roseDim, legendFont(7.0f, 1.2f));

    drawText(g, "MODE", {784, 116, 80, 16}, juce::Justification::left, rose, legendFont(8.0f, 1.1f));
    drawText(g, "ATTACK", {660, 452, 92, 16}, juce::Justification::centred, rose, legendFont(8.0f, 1.1f));

    drawDashedVertical(g, 292.0f, 362.0f, 570.0f, roseDim.withAlpha(0.8f));
    drawDashedVertical(g, 556.0f, 362.0f, 570.0f, roseDim.withAlpha(0.8f));

    // Knob legends and values.
    for (const auto& entry : entries_) {
        if (entry.knob == nullptr || entry.place == nullptr || entry.spec == nullptr)
            continue;

        const auto& spec = *entry.spec;
        const auto& place = *entry.place;
        const auto specId = juce::String(spec.id);
        const bool showAlways = !isSwitchId(specId) && (startsWith(spec.id, "step") || specId == "seq_root" || specId == "seq_rate" || specId == "seq_length" || specId == "seq_bpm" || specId == "seq_drift");
        const bool hovered = entry.knob->isHovered();
        const bool showValue = showAlways || hovered;
        int stepIndex = -1;
        if (startsWith(spec.id, "step"))
            stepIndex = juce::jlimit(0, 7, juce::String(spec.id).substring(4, 5).getIntValue() - 1);
        const bool columnDimmed = stepIndex >= 0 ? stepDimmed_[static_cast<std::size_t>(stepIndex)] : false;
        const auto legendAlpha = entry.knob->isEnabled() ? 1.0f : 0.0f;

        if (entry.knob != nullptr && place.legend[0] != '\0') {
            const float legendBaseline = place.y + place.w * 0.5f + 13.5f;
            const float legendAdjust = place.y < 300.0f ? 1.0f : 2.0f;
            drawText(g, place.legend, anchoredTextBox(place.x, legendBaseline + legendAdjust, 120, 16, juce::Justification::centred),
                     juce::Justification::centred, legend.withAlpha(legendAlpha), legendFont(8.0f, 1.1f));
        }

        if (showValue) {
            const auto readout = readoutFor(spec, entry.knob->getValue(), hostClock_ && specId == "seq_bpm");
            juce::Colour finalColour = value;
            if (startsWith(spec.id, "step")) {
                const float light = stepIndex >= 0 ? stepLights_[static_cast<std::size_t>(stepIndex)] : 0.0f;
                finalColour = (hovered || light > 0.0f) ? roseBright : value;
            } else if (hovered) {
                finalColour = roseBright;
            }

            float readoutAlpha = 1.0f;
            if (columnDimmed)
                readoutAlpha = 0.30f;
            else if (hostClock_ && specId == "seq_bpm")
                readoutAlpha = 0.45f;

            const float baseline = startsWith(spec.id, "step") && (endsWith(spec.id, "_pitch") || endsWith(spec.id, "_vel")) ? (place.y + place.w * 0.5f + 14.0f)
                                                                                                                          : (place.y + place.w * 0.5f + 26.5f);
            const int width = juce::jlimit(36, 160, juce::roundToInt(std::max(42.0f, place.w + 30.0f)));
            drawText(g, readout, anchoredTextBox(place.x, baseline, width, juce::roundToInt(numberFont(9.5f).getHeight()) + 3,
                                                 juce::Justification::centred), juce::Justification::centred,
                     finalColour.withAlpha(readoutAlpha),
                     numberFont(9.5f, 0.2f));
        }
    }

    // Switch labels, drawn in window coordinates so the recorder sees them in the right place.
    for (const auto& entry : entries_) {
        if (entry.sw == nullptr || entry.place == nullptr || entry.spec == nullptr)
            continue;

        const auto& spec = *entry.spec;
        const auto specId = juce::String(spec.id);
        const auto bounds = entry.sw->getBounds();
        const int chosen = entry.sw->selected();
        const int segmentCount = entry.sw->segments();
        if (segmentCount <= 0)
            continue;

        const float segmentWidth = static_cast<float>(bounds.getWidth()) / static_cast<float>(segmentCount);
        for (int i = 0; i < segmentCount; ++i) {
            if (startsWith(spec.id, "vco") && endsWith(spec.id, "_wave"))
                continue;

            juce::String label = spec.choices != nullptr && i < spec.numChoices ? juce::String(spec.choices[i]) : juce::String();
            if (specId == "sub_octave")
                label = i == 0 ? "0" : "-1";
            else if (specId == "vca_attack" || specId == "seq_mode")
                label = label.toUpperCase();

            if (label.isEmpty())
                continue;

            const float left = static_cast<float>(bounds.getX()) + segmentWidth * static_cast<float>(i);
            const float width = (i == segmentCount - 1) ? static_cast<float>(bounds.getRight()) - left : segmentWidth;
            const auto box = juce::Rectangle<int>(juce::roundToInt(left), bounds.getY(), juce::roundToInt(width), bounds.getHeight());
            const auto colour = i == chosen ? roseBright : textDim;
            const auto font = specId == "sub_octave" ? numberFont(9.5f, 0.2f) : legendFont(7.5f, 1.1f);
            drawText(g, label, box.reduced(2, 0), juce::Justification::centred, colour, font);
        }
    }
}

void Controls::dropCache() {
    for (auto& entry : entries_) {
        if (entry.knob != nullptr)
            entry.knob->dropCache();
    }
}

void Controls::attachKnob(juce::AudioProcessorValueTreeState& state, juce::Component& window, const undertow::ParamSpec& spec,
                          const ParameterPlace& place) {
    auto* knob = new Knob(spec.min < 0.0f && spec.max > 0.0f, detentsFor(spec));
    knob->setName(spec.id);
    knob->setTitle(spec.name);
    knob->setBounds(juce::roundToInt(place.x - (place.w + 12.0f) * 0.5f), juce::roundToInt(place.y - (place.w + 12.0f) * 0.5f),
                    juce::roundToInt(place.w + 12.0f), juce::roundToInt(place.w + 12.0f));
    knob->setRange(spec.min, spec.max, spec.kind == undertow::ParamKind::Float ? 0.0 : 1.0);
    knob->setSkewFactorFromMidPoint(spec.centre > spec.min && spec.centre < spec.max ? spec.centre : (spec.min + spec.max) * 0.5f);
    knob->setDoubleClickReturnValue(true, spec.def);

    knob->setFormatter([spec](double value) { return readoutFor(spec, value, false); });
    knob->textFromValueFunction = [spec](double value) { return readoutFor(spec, value, false); };
    knob->valueFromTextFunction = [spec](const juce::String& text) {
        const auto s = text.trim();
        if (s.isEmpty())
            return static_cast<double>(spec.def);
        return s.getDoubleValue();
    };

    auto* parameter = state.getParameter(spec.id);
    if (parameter != nullptr) {
        sliderAttachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, spec.id, *knob));
    } else {
        sliderAttachments_.push_back(nullptr);
    }

    window.addAndMakeVisible(*knob);
    entries_.push_back({&spec, &place, knob, nullptr});

    if (juce::String(spec.id).startsWith("step")) {
        const int step = juce::jlimit(0, 7, juce::String(spec.id).substring(4, 5).getIntValue() - 1);
        knob->setLight(stepLights_[static_cast<std::size_t>(step)]);
        knob->setDimmed(stepDimmed_[static_cast<std::size_t>(step)]);
    }
}

void Controls::attachSwitch(juce::AudioProcessorValueTreeState& state, juce::Component& window, const undertow::ParamSpec& spec,
                            const ParameterPlace& place) {
    auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(state.getParameter(spec.id));
    if (parameter == nullptr)
        return;

    std::vector<juce::String> labels;
    labels.reserve(static_cast<std::size_t>(spec.numChoices));
    for (int i = 0; i < spec.numChoices; ++i) {
        auto label = juce::String(spec.choices != nullptr ? spec.choices[i] : "");
        if (juce::String(spec.id) == "sub_octave")
            label = (i == 0 ? "0" : "-1");
        else if (juce::String(spec.id) == "vco1_wave" || juce::String(spec.id) == "vco2_wave")
            label = (i == 0 ? "triangle" : "square");
        else if (juce::String(spec.id) == "seq_mode" || juce::String(spec.id) == "vca_attack")
            label = label.toUpperCase();
        labels.push_back(label);
    }

    auto* sw = new Switch(*parameter, std::move(labels), juce::String(spec.id) == "sub_octave");
    sw->setName(spec.id);
    sw->setTitle(spec.name);
    sw->setBounds(juce::roundToInt(place.x), juce::roundToInt(place.y), juce::roundToInt(place.w), juce::roundToInt(place.h));

    window.addAndMakeVisible(*sw);
    entries_.push_back({&spec, &place, nullptr, sw});
}
