#include "Console.h"

#include "Texts.h"
#include "Theme.h"

#include <undertow/params.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace {

namespace theme = undertow::ui::theme;

constexpr float kPi = 3.14159265358979323846f;
constexpr float kSilenceDb = -60.0f;
constexpr float kSpectrumFloorDb = -96.0f;
constexpr float kSpectrumTopDb = -18.0f;
constexpr float kHintFadeSeconds = 0.3f;
constexpr float kNumberMoveSeconds = 0.12f;
constexpr float kNumberIdleFadeSeconds = 0.6f;
constexpr float kPeakHoldSeconds = 1.5f;
constexpr float kPeakFallDbPerSecond = 20.0f;
constexpr float kClipFlashSeconds = 0.2f;
constexpr double kRumbleFlashSeconds = 0.12;

const juce::Rectangle<int>& themePlotBounds() {
    static const auto bounds = theme::plotBounds();
    return bounds;
}

float dbFromLinear(float linear, float floorDb = kSpectrumFloorDb) {
    if (linear <= 0.0f)
        return floorDb;
    return std::max(floorDb, 20.0f * std::log10(linear));
}

bool isSounding(const WindowFrame& frame) {
    return dbFromLinear(frame.rms) > kSilenceDb;
}

float meterXForDb(float db) {
    const auto inner = theme::meterBounds.reduced(2, 2);
    const float t = juce::jlimit(0.0f, 1.0f, (db + 48.0f) / 48.0f);
    return static_cast<float>(inner.getX()) + static_cast<float>(inner.getWidth()) * t;
}

int sequenceMode(float raw) {
    return juce::jlimit(0, 2, juce::roundToInt(raw));
}

float rumbleGridBeats(int rate) {
    switch (rate) {
        case 0: return 1.0f;   // 1/4
        case 1: return 2.0f;   // 1/2
        case 2: return 4.0f;   // 1 bar
        case 3: return 0.5f;   // 1/8
        default: return 1.0f;
    }
}

float rumbleRuleGlow(float rumble) {
    const float db = dbFromLinear(rumble, kSilenceDb);
    return juce::jlimit(0.0f, 1.0f, (db - kSilenceDb) / (-12.0f - kSilenceDb));
}

juce::Colour fadeColour(juce::Colour live, juce::Colour idle, float amount) {
    return idle.interpolatedWith(live, juce::jlimit(0.0f, 1.0f, amount));
}

}  // namespace

Console::Console() {
    spectrum_.setSampleRate(sampleRate_);
    for (auto& curve : curves_)
        curve.fill(kSpectrumFloorDb);
    curveVisible_.fill(false);
}

void Console::advance(const WindowFrame& frame, double seconds) {
    frame_ = frame;

    if (std::abs(frame.sampleRate - sampleRate_) > 0.5) {
        sampleRate_ = frame.sampleRate;
        spectrum_.setSampleRate(sampleRate_);
        for (auto& curve : curves_)
            curve.fill(kSpectrumFloorDb);
        curveVisible_.fill(false);
        spectrumTick_ = 0;
        spectrumSeconds_ = 0.0;
    }

    updateSpectrum(frame, seconds);

    const bool sounding = isSounding(frame);
    updateHint(seconds, sounding, sequenceMode(paramValue(undertow::param::SeqMode)));
    updateNumber(frame, seconds, sounding);
    updateMeter(seconds);
    updateClip(frame, seconds);
    updateRumble(frame, seconds);
}

void Console::paint(juce::Graphics& g) {
    drawHead(g);
    drawSpectrum(g);
    drawNumber(g);
    drawArc(g);
    drawMeter(g);
    drawRumble(g);

    if (clipping_) {
        const auto plate = juce::Rectangle<float>(442.0f, 0.0f, 316.0f, 10.0f);
        g.setColour(theme::bgVoid);
        g.fillRect(plate);

        drawText(g, theme::legendFont(6.5f, 1.0f), theme::roseBright, "CLIP  OUTPUT HELD AT -0.3 dBFS",
                 plate.getCentreX(), 11.0f, juce::Justification::centred, 0, 0);
    }
}

void Console::paintOver(juce::Graphics& g) {
    if (clipping_) {
        drawHazardBand(g, theme::topHazardBounds, static_cast<float>(hazardOffset_));
        drawHazardBand(g, theme::bottomHazardBounds, static_cast<float>(hazardOffset_ + 6.0));

        const auto plate = juce::Rectangle<float>(442.0f, 0.0f, 316.0f, 10.0f);
        g.setColour(theme::bgVoid);
        g.fillRect(plate);

        drawText(g, theme::legendFont(6.5f, 1.0f), theme::roseBright, "CLIP  OUTPUT HELD AT -0.3 dBFS",
                 plate.getCentreX(), 11.0f, juce::Justification::centred, 0, 0);
    }

    if (clipFlash_ <= 0.0f)
        return;

    const float alpha = 0.45f * clipFlash_;
    g.setColour(theme::roseBright.withAlpha(alpha));
    const juce::Rectangle<float> bezels[] = {
        theme::clockBounds.toFloat(),
        theme::spectrumGlassBounds.toFloat(),
        theme::driftArcBounds.toFloat(),
        theme::meterBounds.toFloat(),
    };
    for (const auto& bezel : bezels)
        g.drawRoundedRectangle(bezel, 4.0f, 1.2f);
}

void Console::dropCache() {}

juce::Rectangle<int> Console::plotBounds() const {
    return theme::plotBounds();
}

float Console::paramValue(int index) const {
    return value ? value(index) : undertow::paramSpec(index).def;
}

float Console::linearToDb(float linear, float floorDb) {
    return dbFromLinear(linear, floorDb);
}

float Console::plotXForHz(double hz) {
    const auto& plot = themePlotBounds();
    return static_cast<float>(plot.getX()) + static_cast<float>(plot.getWidth()) *
                                                 static_cast<float>(std::log(hz / 20.0) / std::log(100.0));
}

float Console::plotYForDb(float db) {
    const auto& plot = themePlotBounds();
    const float clamped = juce::jlimit(kSpectrumFloorDb, kSpectrumTopDb, db);
    const float t = (kSpectrumTopDb - clamped) / (kSpectrumTopDb - kSpectrumFloorDb);
    return static_cast<float>(plot.getY()) + static_cast<float>(plot.getHeight()) * t;
}

juce::String Console::noteName(int midiNote) {
    static constexpr std::array<const char*, 12> names{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const int octave = midiNote / 12 - 1;
    const int note = ((midiNote % 12) + 12) % 12;
    return juce::String(names[static_cast<std::size_t>(note)]) + juce::String(octave);
}

juce::String Console::formatFrequency(float hz) {
    return juce::String(hz, hz < 1000.0f ? 2 : 1);
}

juce::String Console::formatPeakDb(float db) {
    return juce::String(db, 1);
}

juce::Rectangle<int> Console::textBounds(const juce::Font& font, const juce::String& text, float anchorX, float baseline,
                                         juce::Justification justification, int paddingX, int paddingY) {
    const int width = juce::roundToInt(std::ceil(juce::GlyphArrangement::getStringWidth(font, text))) + 2 * paddingX;
    const int height = juce::roundToInt(std::ceil(font.getHeight())) + 2 * paddingY;
    return theme::textBox(anchorX, baseline, width, height, justification);
}

void Console::drawText(juce::Graphics& g, const juce::Font& font, juce::Colour colour, const juce::String& text,
                       float anchorX, float baseline, juce::Justification justification, int paddingX, int paddingY) {
    g.setFont(font);
    g.setColour(colour);
    texts::draw(g, text, textBounds(font, text, anchorX, baseline, justification, paddingX, paddingY), justification);
}

float Console::easeOut(float t) {
    const float clamped = juce::jlimit(0.0f, 1.0f, t);
    const float inverse = 1.0f - clamped;
    return 1.0f - inverse * inverse;
}

void Console::updateSpectrum(const WindowFrame& frame, double seconds) {
    spectrumSeconds_ += std::max(0.0, seconds);
    ++spectrumTick_;

    if ((spectrumTick_ % 2) != 0 || spectrumSeconds_ < (1.0 / 30.0))
        return;

    spectrum_.analyse(frame.scope.data(), spectrumSeconds_);
    spectrumSeconds_ = 0.0;
    pushSpectrumCurve();
}

void Console::pushSpectrumCurve() {
    for (int i = kCurveHistory - 1; i > 0; --i) {
        curves_[static_cast<std::size_t>(i)] = curves_[static_cast<std::size_t>(i - 1)];
        curveVisible_[static_cast<std::size_t>(i)] = curveVisible_[static_cast<std::size_t>(i - 1)];
    }

    bool visible = false;
    auto& newest = curves_.front();
    for (int i = 0; i < undertow::kSpectrumPoints; ++i) {
        newest[static_cast<std::size_t>(i)] = spectrum_.level(i);
        visible = visible || newest[static_cast<std::size_t>(i)] > (kSpectrumFloorDb + 0.01f);
    }
    curveVisible_.front() = visible;
}

void Console::updateHint(double seconds, bool sounding, int mode) {
    const bool wantsHint = !sounding && (mode == 0 || mode == 2);
    const float delta = kHintFadeSeconds > 0.0f ? static_cast<float>(seconds / kHintFadeSeconds) : 1.0f;
    hintAlpha_ = juce::jlimit(0.0f, 1.0f, hintAlpha_ + (wantsHint ? delta : -delta));
}

void Console::updateNumber(const WindowFrame& frame, double seconds, bool sounding) {
    const bool minusOctave = paramValue(undertow::param::SubOctave) > 0.5f;

    if (frame.note >= 0 && frame.noteHz > 0.0f) {
        const float targetHz = frame.noteHz * (minusOctave ? 0.5f : 1.0f);
        const juce::String targetNote = noteName(frame.note - (minusOctave ? 12 : 0));

        if (!hasDisplayedNumber_) {
            hasDisplayedNumber_ = true;
            displayedHz_ = targetHz;
            numberStartHz_ = targetHz;
            numberTargetHz_ = targetHz;
            numberProgress_ = 1.0f;
        } else if (std::abs(targetHz - numberTargetHz_) > 0.001f || displayedNote_ != targetNote) {
            numberStartHz_ = displayedHz_;
            numberTargetHz_ = targetHz;
            numberProgress_ = 0.0f;
        }

        displayedNote_ = targetNote;
        numberFade_ = sounding ? 1.0f : numberFade_;
    }

    if (numberProgress_ < 1.0f) {
        numberProgress_ = juce::jlimit(0.0f, 1.0f, numberProgress_ + static_cast<float>(seconds / kNumberMoveSeconds));
        displayedHz_ = juce::jmap(easeOut(numberProgress_), numberStartHz_, numberTargetHz_);
    } else {
        displayedHz_ = numberTargetHz_;
    }

    if (frame.note >= 0 && frame.noteHz > 0.0f) {
        numberFade_ = 1.0f;
    } else if (hasDisplayedNumber_) {
        numberFade_ = juce::jlimit(0.0f, 1.0f, numberFade_ - static_cast<float>(seconds / kNumberIdleFadeSeconds));
    }
}

void Console::updateArc() {}

void Console::updateMeter(double seconds) {
    const float peakDb = linearToDb(frame_.peak, kSpectrumFloorDb);
    if (peakDb >= heldPeakDb_) {
        heldPeakDb_ = peakDb;
        peakHoldSeconds_ = kPeakHoldSeconds;
        return;
    }

    if (peakHoldSeconds_ > 0.0) {
        peakHoldSeconds_ = std::max(0.0, peakHoldSeconds_ - seconds);
        return;
    }

    heldPeakDb_ = std::max(peakDb, heldPeakDb_ - kPeakFallDbPerSecond * static_cast<float>(seconds));
}

void Console::updateRumble(const WindowFrame& frame, double seconds) {
    rumbleFlashSeconds_ = std::max(0.0, rumbleFlashSeconds_ - seconds);

    const bool flashEnabled = frame.hostClock && paramValue(undertow::param::RumbleKick) > 0.0f;
    const float grid = rumbleGridBeats(juce::roundToInt(paramValue(undertow::param::RumbleRate)));
    if (flashEnabled && haveLastPpq_ && grid > 0.0f) {
        const auto before = static_cast<long long>(std::floor(lastPpq_ / grid));
        const auto after = static_cast<long long>(std::floor(frame.ppq / grid));
        if (after > before)
            rumbleFlashSeconds_ = kRumbleFlashSeconds;
    }

    lastPpq_ = frame.ppq;
    haveLastPpq_ = true;
}

void Console::updateClip(const WindowFrame& frame, double seconds) {
    hazardOffset_ += 40.0 * std::max(0.0, seconds);
    while (hazardOffset_ >= 12.0)
        hazardOffset_ -= 12.0;

    if (frame.clipping && !clipping_) {
        clipFlash_ = 1.0f;
    } else if (clipFlash_ > 0.0f) {
        clipFlash_ = std::max(0.0f, clipFlash_ - static_cast<float>(seconds / kClipFlashSeconds));
    }

    clipping_ = frame.clipping;
}

void Console::drawGlass(juce::Graphics& g, juce::Rectangle<float> bounds, float radius) const {
    g.setColour(theme::glass);
    g.fillRoundedRectangle(bounds, radius);
    g.setColour(theme::rule);
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);

    g.saveState();
    g.reduceClipRegion(bounds.toNearestInt());
    g.setColour(juce::Colours::black.withAlpha(0.24f));
    for (int y = juce::roundToInt(bounds.getY()) + 1; y < juce::roundToInt(bounds.getBottom()); y += 3)
        g.fillRect(bounds.getX(), static_cast<float>(y), bounds.getWidth(), 1.0f);
    g.restoreState();
}

void Console::drawSpectrum(juce::Graphics& g) {
    const auto& plot = themePlotBounds();

    drawGlass(g, theme::spectrumGlassBounds.toFloat(), 6.0f);

    g.saveState();
    g.reduceClipRegion(theme::spectrumGlassBounds);
    g.setColour(theme::textPhantom.withAlpha(0.85f));
    for (int x = theme::spectrumGlassBounds.getX() + 14; x < theme::spectrumGlassBounds.getRight(); x += 32)
        g.drawVerticalLine(x, static_cast<float>(theme::spectrumGlassBounds.getY() + 16),
                           static_cast<float>(theme::spectrumGlassBounds.getBottom() - 16));
    for (int y = theme::spectrumGlassBounds.getY() + 18; y < theme::spectrumGlassBounds.getBottom() - 14; y += 32)
        g.drawHorizontalLine(y, static_cast<float>(theme::spectrumGlassBounds.getX() + 12),
                             static_cast<float>(theme::spectrumGlassBounds.getRight() - 12));

    const float bandLeft = plotXForHz(30.0);
    const float bandRight = plotXForHz(150.0);
    juce::ColourGradient bandFill(theme::rose.withAlpha(0.0f), bandLeft, static_cast<float>(plot.getY()),
                                  theme::rose.withAlpha(0.09f), bandLeft, static_cast<float>(plot.getBottom()), false);
    g.setGradientFill(bandFill);
    g.fillRect(juce::Rectangle<float>(bandLeft, static_cast<float>(plot.getY()), bandRight - bandLeft,
                                      static_cast<float>(plot.getHeight())));
    g.setColour(theme::rose.withAlpha(0.3f));
    const float dash[] = {3.0f, 3.0f};
    g.drawDashedLine(juce::Line<float>(bandLeft, static_cast<float>(plot.getY()), bandLeft,
                                       static_cast<float>(plot.getBottom())), dash, 2, 1.0f);
    g.drawDashedLine(juce::Line<float>(bandRight, static_cast<float>(plot.getY()), bandRight,
                                       static_cast<float>(plot.getBottom())), dash, 2, 1.0f);

    for (const int db : {-36, -60, -84}) {
        const float y = plotYForDb(static_cast<float>(db));
        g.setColour(theme::textPhantom.withAlpha(0.95f));
        g.drawDashedLine(juce::Line<float>(static_cast<float>(plot.getX()), y, static_cast<float>(plot.getRight()), y),
                         dash, 2, 1.0f);
        drawText(g, theme::numberFont(8.0f), theme::textDim, juce::String(db), static_cast<float>(plot.getRight() - 2),
                 y - 1.0f, juce::Justification::right);
    }

    const auto clipPlot = plot.withTrimmedBottom(1);
    g.reduceClipRegion(clipPlot);
    drawSpectrumCurve(g, curves_[4], theme::roseDeep, 1.0f, curveVisible_[4] ? 0.12f : 0.0f, false);
    drawSpectrumCurve(g, curves_[3], theme::roseEmber, 1.0f, curveVisible_[3] ? 0.22f : 0.0f, false);
    drawSpectrumCurve(g, curves_[2], theme::roseDim, 1.0f, curveVisible_[2] ? 0.36f : 0.0f, false);
    drawSpectrumCurve(g, curves_[1], theme::roseDim, 1.0f, curveVisible_[1] ? 0.55f : 0.0f, false);
    drawSpectrumCurve(g, curves_[0], theme::roseBright, 1.5f, curveVisible_[0] ? 1.0f : 0.0f, true);
    g.restoreState();

    drawText(g, theme::legendFont(7.5f, 0.0f), theme::rose, "SPECTRUM", 798.0f, 360.0f, juce::Justification::left);
    drawText(g, theme::numberFont(8.5f), theme::textDim, "20 Hz - 2 kHz", 798.0f, 378.0f, juce::Justification::left);

    if (isSounding(frame_)) {
        const int share = juce::roundToInt(std::round(spectrum_.share() * 100.0f));
        drawText(g, theme::numberLightFont(22.0f), theme::rose, juce::String(share), 798.0f, 390.0f,
                 juce::Justification::left);
        drawText(g, theme::numberFont(12.0f), theme::roseDim, "%", 858.0f, 390.0f, juce::Justification::left);
        drawText(g, theme::legendFont(6.5f, 1.0f), theme::roseDim, "BELOW 150 Hz", 876.0f, 390.0f,
                 juce::Justification::left);
    }

    drawText(g, theme::legendFont(6.5f, 1.0f), theme::roseDim, "30-150 Hz", (bandLeft + bandRight) * 0.5f, 564.0f,
             juce::Justification::horizontallyCentred);

    const std::array<std::pair<double, juce::String>, 9> axis{{
        {20.0, "20"}, {30.0, "30"}, {50.0, "50"}, {100.0, "100"}, {150.0, "150"},
        {300.0, "300"}, {500.0, "500"}, {1000.0, "1k"}, {2000.0, "2k"},
    }};
    for (const auto& [hz, label] : axis) {
        const auto colour = (hz == 30.0 || hz == 150.0) ? theme::rose : theme::textDim;
        drawText(g, theme::numberFont(8.5f), colour, label, plotXForHz(hz), 582.0f,
                 juce::Justification::horizontallyCentred);
    }

    if (hintAlpha_ > 0.0f) {
        const int mode = sequenceMode(paramValue(undertow::param::SeqMode));
        const juce::String hint = mode == 2 ? "PLAY MIDI NOTES" : (mode == 0 ? "HOLD A KEY TO PLAY THE PATTERN" : juce::String());
        if (!hint.isEmpty()) {
            const auto font = theme::legendFont(7.5f, 1.0f);
            const auto box = textBounds(font, hint, static_cast<float>(plot.getCentreX()),
                                        static_cast<float>(plot.getY()) + 0.4f * static_cast<float>(plot.getHeight()),
                                        juce::Justification::horizontallyCentred, 12, 7);
            g.setColour(theme::glass.brighter(0.12f).withAlpha(0.92f * hintAlpha_));
            g.fillRoundedRectangle(box.toFloat().expanded(6.0f, 4.0f), 4.0f);
            g.setColour(theme::rule.withAlpha(0.8f * hintAlpha_));
            g.drawRoundedRectangle(box.toFloat().expanded(6.0f, 4.0f), 4.0f, 1.0f);
            g.setColour(theme::roseDim.withAlpha(hintAlpha_));
            g.setFont(font);
            texts::draw(g, hint, box, juce::Justification::horizontallyCentred);
        }
    }
}

void Console::drawSpectrumCurve(juce::Graphics& g, const Curve& curve, juce::Colour colour, float stroke, float alpha,
                                bool newest) const {
    if (alpha <= 0.0f)
        return;

    juce::Path path;
    bool started = false;
    bool anyVisible = false;
    float firstX = 0.0f;
    float lastX = 0.0f;

    for (int i = 0; i < undertow::kSpectrumPoints; ++i) {
        const float db = curve[static_cast<std::size_t>(i)];
        if (db <= kSpectrumFloorDb + 0.01f)
            continue;

        const float x = plotXForHz(undertow::Spectrum::pointHz(i));
        const float y = plotYForDb(db);
        if (!started) {
            path.startNewSubPath(x, y);
            firstX = x;
            started = true;
        } else {
            path.lineTo(x, y);
        }
        lastX = x;
        anyVisible = true;
    }

    if (!anyVisible)
        return;

    if (newest) {
        juce::Path fill(path);
        fill.lineTo(lastX, static_cast<float>(themePlotBounds().getBottom()));
        fill.lineTo(firstX, static_cast<float>(themePlotBounds().getBottom()));
        fill.closeSubPath();

        juce::ColourGradient fillGradient(theme::rose.withAlpha(0.42f), 0.0f, static_cast<float>(themePlotBounds().getY()),
                                          theme::roseDeep.withAlpha(0.05f), 0.0f,
                                          static_cast<float>(themePlotBounds().getBottom()), false);
        fillGradient.addColour(0.65, theme::roseEmber.withAlpha(0.30f));
        g.setGradientFill(fillGradient);
        g.fillPath(fill);

        g.setColour(colour.withAlpha(0.16f));
        g.strokePath(path, juce::PathStrokeType(7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(colour.withAlpha(0.30f));
        g.strokePath(path, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setColour(colour.withAlpha(alpha));
    g.strokePath(path, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void Console::drawNumber(juce::Graphics& g) {
    if (!hasDisplayedNumber_)
        return;

    const auto digitsColour = fadeColour(theme::bone, theme::textGhost, numberFade_);
    const auto labelColour = fadeColour(theme::boneDim, theme::textGhost, numberFade_);
    const auto dot = juce::String::charToString(0x00B7);

    drawText(g, theme::numberLightFont(44.0f, -0.5f), digitsColour, formatFrequency(displayedHz_), 1134.0f, 386.0f,
             juce::Justification::right);
    drawText(g, theme::numberFont(14.0f), labelColour, "Hz", 1160.0f, 386.0f, juce::Justification::right);
    drawText(g, theme::legendFont(7.0f, 1.0f), labelColour, displayedNote_ + " " + dot + " THE SUB", 1160.0f, 406.0f,
             juce::Justification::right);
}

void Console::drawArc(juce::Graphics& g) {
    drawGlass(g, theme::driftArcBounds.toFloat(), 4.0f);

    const int length = juce::jlimit(1, 8, juce::roundToInt(paramValue(undertow::param::SeqLength)));
    int soundingSteps = 0;
    for (int step = 0; step < length; ++step) {
        if (paramValue(undertow::param::Step1Vel + step) > 0.0f)
            ++soundingSteps;
    }

    const float drift = juce::jlimit(0.0f, 1.0f, paramValue(undertow::param::SeqDrift));
    const bool playing = frame_.step >= 0 && sequenceMode(paramValue(undertow::param::SeqMode)) != 2;
    const int currentPass = playing ? static_cast<int>((frame_.count / length) % 16) : -1;
    const float barWidth = (292.0f - 16.0f - 45.0f) / 16.0f;

    for (int pass = 0; pass < 16; ++pass) {
        const float x = 892.0f + static_cast<float>(pass) * (barWidth + 3.0f);
        const int moved = std::min(soundingSteps,
                                   static_cast<int>(std::floor(3.0f * drift * std::sin(kPi * static_cast<float>(pass) / 16.0f) + 0.5f)));
        const float height = std::max(1.5f, 20.0f * static_cast<float>(moved) / 3.0f);
        const auto bar = juce::Rectangle<float>(x, 287.0f - height, barWidth, height);

        juce::Colour colour = theme::roseDim.withAlpha(playing ? (pass < currentPass ? 0.55f : 0.35f) : 0.55f);
        if (pass == currentPass)
            colour = theme::roseBright;

        if (pass == currentPass) {
            g.setColour(theme::roseBright.withAlpha(0.25f));
            g.fillRect(bar.expanded(0.0f, 1.5f));
        }
        g.setColour(colour);
        g.fillRect(bar);
    }

    drawText(g, theme::legendFont(8.0f, 1.1f), theme::legend, "STEPS AWAY FROM HOME", 884.0f, 308.5f,
             juce::Justification::left);
    drawText(g, theme::numberFont(9.5f), theme::value,
             currentPass >= 0 ? "PASS " + juce::String(currentPass + 1) + "/16" : "16 PASSES", 1176.0f, 308.5f,
             juce::Justification::right);
}

void Console::drawHead(juce::Graphics& g) {
    drawText(g, theme::legendFont(13.0f, 5.4f), theme::rose, "UNDERTOW", 24.0f, 29.5f, juce::Justification::left);
    drawText(g, theme::legendFont(13.0f), theme::roseDim, "2", 188.0f, 29.5f, juce::Justification::left);
    g.setColour(theme::rule2);
    g.drawVerticalLine(216, 17.0f, 31.0f);
    drawText(g, theme::legendFont(7.0f, 2.4f), theme::roseDim, "METAKOSMIA", 230.0f, 28.5f,
             juce::Justification::left);

    drawGlass(g, theme::clockBounds.toFloat(), 4.0f);
    drawText(g, theme::numberFont(12.0f, 0.4f), theme::value, juce::String(frame_.bpm, 1), 502.0f, 29.0f,
             juce::Justification::left);
    drawText(g, theme::legendFont(6.5f, 1.0f), theme::roseDim, "BPM", 553.0f, 29.0f, juce::Justification::left);

    const auto badge = juce::Rectangle<float>(584.0f, 18.0f, 40.0f, 13.0f);
    const auto badgeColour = frame_.hostClock ? theme::roseDim : theme::dream;
    g.setColour(badgeColour.withAlpha(0.16f));
    g.fillRoundedRectangle(badge, 2.0f);
    g.setColour(badgeColour);
    g.drawRoundedRectangle(badge, 2.0f, 1.0f);
    drawText(g, theme::legendFont(6.5f, 1.0f), badgeColour, frame_.hostClock ? "HOST" : "INT", 604.0f, 28.5f,
             juce::Justification::horizontallyCentred);

    const double ppq = std::max(0.0, frame_.ppq);
    const int bar = static_cast<int>(std::floor(ppq / 4.0)) + 1;
    const int beat = static_cast<int>(std::floor(std::fmod(ppq, 4.0))) + 1;
    const int sixteenth = static_cast<int>(std::floor((ppq - std::floor(ppq)) * 4.0)) + 1;
    const auto position = juce::String(bar).paddedLeft('0', 3) + "." + juce::String(beat) + "." + juce::String(sixteenth);
    drawText(g, theme::numberFont(12.0f, 0.2f), theme::value, position, 698.0f, 29.0f, juce::Justification::right);

    const bool live = linearToDb(frame_.rms, kSpectrumFloorDb) > kSilenceDb;
    drawLamp(g, {1140.0f, 24.0f}, 3.2f, live);
    drawText(g, theme::legendFont(7.0f, 1.6f), live ? theme::rose : theme::textDim, "LIVE", 1151.0f, 27.0f,
             juce::Justification::left);
}

void Console::drawMeter(juce::Graphics& g) {
    drawGlass(g, theme::meterBounds.toFloat(), 2.0f);

    const auto inner = theme::meterBounds.reduced(2, 2);
    const float rmsDb = linearToDb(frame_.rms, kSpectrumFloorDb);
    const float fillEnd = meterXForDb(rmsDb);
    if (fillEnd > static_cast<float>(inner.getX())) {
        juce::ColourGradient fill(theme::roseDeep, static_cast<float>(inner.getX()), static_cast<float>(inner.getCentreY()),
                                  theme::rose, static_cast<float>(inner.getRight()), static_cast<float>(inner.getCentreY()), false);
        fill.addColour(0.7, theme::roseDim);
        g.setGradientFill(fill);
        g.fillRect(juce::Rectangle<float>(static_cast<float>(inner.getX()), static_cast<float>(inner.getY()),
                                          fillEnd - static_cast<float>(inner.getX()), static_cast<float>(inner.getHeight())));
    }

    g.setColour(theme::roseBright);
    g.fillRect(juce::Rectangle<float>(std::round(meterXForDb(heldPeakDb_)), static_cast<float>(inner.getY()), 2.0f,
                                      static_cast<float>(inner.getHeight())));

    drawText(g, theme::numberFont(8.5f), theme::value, "PEAK " + formatPeakDb(heldPeakDb_) + " dBFS", 1136.0f, 667.0f,
             juce::Justification::right);

    for (const int db : {-48, -36, -24, -12, -6, 0})
        drawText(g, theme::numberFont(8.0f), theme::textDim, juce::String(db), meterXForDb(static_cast<float>(db)), 695.0f,
                 juce::Justification::horizontallyCentred);

    drawLamp(g, {1158.0f, 678.0f}, 3.2f, frame_.clipping);
    drawText(g, theme::legendFont(6.0f, 1.0f), theme::textDim, "CLIP", 1146.0f, 699.0f, juce::Justification::left);
}

void Console::drawRumble(juce::Graphics& g) {
    drawLamp(g, {104.0f, 622.5f}, 2.6f, rumbleFlashSeconds_ > 0.0);

    const float glow = rumbleRuleGlow(frame_.rumble);
    g.setColour(theme::rose.withAlpha(0.20f + 0.45f * glow));
    g.drawLine(114.0f, 622.0f, 760.0f, 622.0f, 1.0f + glow);
}

void Console::drawHazardBand(juce::Graphics& g, juce::Rectangle<int> bounds, float offset) const {
    g.saveState();
    g.reduceClipRegion(bounds);
    g.setColour(theme::roseBright.withAlpha(0.92f));
    const float top = static_cast<float>(bounds.getY());
    const float bottom = static_cast<float>(bounds.getBottom());
    const float height = static_cast<float>(bounds.getHeight());
    for (float x = static_cast<float>(bounds.getX()) - height - 24.0f + offset;
         x < static_cast<float>(bounds.getRight()) + 24.0f; x += 12.0f) {
        g.drawLine(x, bottom, x + height, top, 6.0f);
    }
    g.restoreState();
}

void Console::drawLamp(juce::Graphics& g, juce::Point<float> centre, float radius, bool lit) const {
    if (lit) {
        g.setColour(theme::roseBright.withAlpha(0.25f));
        g.fillEllipse(centre.x - radius - 2.0f, centre.y - radius - 2.0f, (radius + 2.0f) * 2.0f, (radius + 2.0f) * 2.0f);
        g.setColour(theme::roseBright);
        g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        return;
    }

    g.setColour(theme::glass);
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(theme::rule2);
    g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.0f);
}
