// SPDX-License-Identifier: AGPL-3.0-only
// Phase 6 spec tests: the window's frame, its typefaces and its hooks (docs/specs/phase-6-window.md, checks P6.08 to
// P6.12; docs/design/window.md sections 2, 5 and 10).
// spec-preset: plugin
// spec-needs: plugin/ui/Frame.h

#include "doctest.h"

#include "spec_paths.h"
#include "spec_window.h"
#include "ui/Theme.h"
#include "undertow/engine.h"

#include <cmath>
#include <string>
#include <utility>
#include <vector>

using namespace spec;

TEST_CASE("P6.08 the window is a fixed 1200 x 760 UndertowEditor, and opens and closes cleanly") {
    juce::ScopedJuceInitialiser_GUI gui;
    std::unique_ptr<juce::AudioProcessor> processor{createPluginFilter()};
    for (int round = 0; round < 3; ++round) {
        std::unique_ptr<juce::AudioProcessorEditor> editor{processor->createEditor()};
        REQUIRE_MESSAGE(dynamic_cast<UndertowEditor*>(editor.get()) != nullptr, "createEditor() must return an UndertowEditor");
        CHECK_MESSAGE(editor->getWidth() == 1200, "the window is " << editor->getWidth() << " px wide; it is 1200");
        CHECK_MESSAGE(editor->getHeight() == 760, "the window is " << editor->getHeight() << " px high; it is 760");
        CHECK_MESSAGE(!editor->isResizable(), "the window has one size: setResizable(false, false)");
        auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
        CHECK(image.getWidth() == 1200);
    }
}

TEST_CASE("P6.08 Undertow 1's window parts are gone, so the ui/*.cpp glob never compiles them") {
    for (const char* path : {"plugin/ui/Displays.h", "plugin/ui/Displays.cpp", "plugin/ui/Look.h", "plugin/ui/Look.cpp",
                             "plugin/ui/Status.h", "plugin/ui/Status.cpp", "plugin/ui/UnitArray.h",
                             "plugin/ui/UnitArray.cpp", "plugin/ui/UiFrame.h"}) {
        const juce::File file{juce::String(spec::sourceDir()) + "/" + path};
        CHECK_MESSAGE(!file.exists(), std::string(path) << " is still in the tree: task 2 deletes Undertow 1's window "
                                                           "parts");
    }
}

TEST_CASE("P6.09 the bundled typefaces: Michroma for words, Geist Mono Regular and Light for numbers") {
    juce::ScopedJuceInitialiser_GUI gui;
    namespace theme = undertow::ui::theme;
    const auto legend = theme::legendFont(8.0f, 1.1f);
    const auto number = theme::numberFont(9.5f);
    const auto light = theme::numberLightFont(44.0f);
    REQUIRE(legend.getTypefacePtr() != nullptr);
    CHECK_MESSAGE(legend.getTypefacePtr()->getName() == "Michroma",
                  "legendFont's typeface is \"" << legend.getTypefacePtr()->getName() << "\"; it is Michroma, compiled in "
                  "from third_party/fonts/Michroma-Regular.ttf (juce_add_binary_data, Typeface::createSystemTypefaceFor)");
    CHECK_MESSAGE(number.getTypefacePtr()->getName() == "Geist Mono", "numberFont's typeface is \""
                                                                      << number.getTypefacePtr()->getName() << "\"");
    CHECK_MESSAGE(number.getTypefacePtr()->getStyle() == "Regular", "numberFont is Geist Mono Regular; its style is "
                                                                    << number.getTypefacePtr()->getStyle());
    CHECK_MESSAGE(light.getTypefacePtr()->getName() == "Geist Mono", "numberLightFont's typeface is \""
                                                                     << light.getTypefacePtr()->getName() << "\"");
    CHECK_MESSAGE(light.getTypefacePtr()->getStyle() == "Light", "numberLightFont is Geist Mono Light; its style is "
                                                                 << light.getTypefacePtr()->getStyle());
    CHECK_MESSAGE(legend.getHeightInPoints() == doctest::Approx(8.0).epsilon(0.01),
                  "a font's size is its em square in logical pixels (FontOptions::withPointHeight): legendFont(8) is "
                  << legend.getHeightInPoints() << " points");
    const float tracked = juce::GlyphArrangement::getStringWidth(theme::legendFont(8.0f, 1.1f), "LEVEL");
    const float plain = juce::GlyphArrangement::getStringWidth(theme::legendFont(8.0f), "LEVEL");
    CHECK_MESSAGE(tracked - plain == doctest::Approx(5 * 1.1).epsilon(0.08),
                  "tracking adds its pixels after each letter: \"LEVEL\" at 1.1 px is " << tracked - plain << " px wider, "
                  "5.5 expected (withExtraKerningFactor(tracking / height))");
}

TEST_CASE("P6.10 captureFrame reads the processor: the meters, the sample rate and the newest 16384 samples") {
    Fixture f;
    auto& processor = f.undertow();
    f.set("seq_mode", 1.0f);  // Free: the pattern plays on seq_root with the engine's own clock
    processor.setRateAndBufferSizeDetails(48000.0, 512);
    processor.prepareToPlay(48000.0, 512);
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    for (int i = 0; i < 120; ++i)
        processor.processBlock(buffer, midi);
    const auto frame = f.editor->captureFrame();
    const auto m = processor.engine().meters();
    REQUIRE_MESSAGE(m.step >= 0, "in Free mode the pattern plays on the engine's own clock");
    CHECK_MESSAGE(frame.step == m.step, "frame.step is " << frame.step << "; meters().step is " << m.step);
    CHECK_MESSAGE(frame.count == m.count, "frame.count is " << frame.count << "; meters().count is " << m.count);
    CHECK_MESSAGE(frame.root == m.root, "frame.root is " << frame.root << "; meters().root is " << m.root);
    CHECK_MESSAGE(frame.note == m.note, "frame.note is " << frame.note << "; meters().note is " << m.note);
    CHECK(frame.noteHz == doctest::Approx(m.noteHz));
    CHECK(frame.peak == doctest::Approx(m.peak));
    CHECK(frame.rms == doctest::Approx(m.rms));
    CHECK(frame.rumble == doctest::Approx(m.rumble));
    CHECK(frame.ppq == doctest::Approx(m.ppq));
    CHECK(frame.bpm == doctest::Approx(m.bpm));
    CHECK(frame.hostClock == m.hostClock);
    CHECK(frame.clipping == m.clipping);
    CHECK_MESSAGE(frame.sampleRate == doctest::Approx(48000.0), "frame.sampleRate is the processor's, 48000; got "
                                                                << frame.sampleRate);
    std::vector<float> scope(undertow::kScopeSamples);
    REQUIRE(processor.engine().copyScope(scope.data(), undertow::kScopeSamples) == undertow::kScopeSamples);
    int mismatches = 0;
    for (int i = 0; i < undertow::kScopeSamples; ++i)
        mismatches += frame.scope[static_cast<std::size_t>(i)] != scope[static_cast<std::size_t>(i)] ? 1 : 0;
    CHECK_MESSAGE(mismatches == 0, mismatches << " of the frame's scope samples differ from copyScope's newest 16384");
}

TEST_CASE("P6.11 the hooks: the step cells, hit-testing, the plot, and freeze leaves the picture to advance") {
    Fixture f;
    for (int s = 0; s < 8; ++s) {
        INFO("step " << s);
        CHECK_MESSAGE(f.editor->stepCellBounds(s) == designCell(s), "stepCellBounds is " << str(f.editor->stepCellBounds(s))
                                                                    << "; the design puts it at " << str(designCell(s)));
        CHECK_MESSAGE(f.editor->stepAt(designCell(s).getCentre()) == s, "stepAt(the cell's centre) must be " << s);
    }
    CHECK_MESSAGE(f.editor->stepAt({600, 600}) == -1, "stepAt outside the cells is -1");
    CHECK_MESSAGE(f.editor->stepAt({105, 120}) == -1, "between the row labels and cell 1 there is no cell");
    CHECK_MESSAGE(f.editor->plotBounds() == kPlot, "plotBounds is " << str(f.editor->plotBounds()) << "; the design's "
                                                   "plot is " << str(kPlot));
    CHECK_MESSAGE(f.editor->controlFor("no_such_parameter") == nullptr, "controlFor an unknown id is null");
    const auto a = f.shot();
    const auto b = f.shot();
    int changed = 0;
    for (int y = 0; y < 760; y += 2)
        for (int x = 0; x < 1200; x += 2)
            changed += a.getPixelAt(x, y) != b.getPixelAt(x, y) ? 1 : 0;
    CHECK_MESSAGE(changed == 0, changed << " sampled pixels changed between two snapshots of a frozen window: everything "
                                           "drawn depends on the frames advanced, never on wall time");
}

TEST_CASE("P6.12 the theme holds every token of the design's section 6, at its value") {
    namespace theme = undertow::ui::theme;
    const std::vector<std::pair<const char*, std::pair<juce::Colour, juce::uint32>>> tokens{
        {"bg_void", {theme::bgVoid, 0xff060608}},       {"bg_mid", {theme::bgMid, 0xff080810}},
        {"glass", {theme::glass, 0xff070609}},          {"rule", {theme::rule, 0xff1e1620}},
        {"rule2", {theme::rule2, 0xff30222e}},          {"rose", {theme::rose, 0xffaa7088}},
        {"rose_bright", {theme::roseBright, 0xffcc90a8}}, {"rose_dim", {theme::roseDim, 0xff7a5060}},
        {"rose_ember", {theme::roseEmber, 0xff482838}}, {"rose_deep", {theme::roseDeep, 0xff3a2030}},
        {"legend", {theme::legend, 0xff8e6474}},        {"value", {theme::value, 0xff988090}},
        {"track", {theme::track, 0xff2e1b28}},          {"pointer", {theme::pointer, 0xffa8829a}},
        {"cap (top)", {theme::capHi, 0xff1d151c}},      {"cap (bottom)", {theme::capLo, 0xff0a070a}},
        {"cap (rim)", {theme::capRim, 0xff2e202c}},
        {"bone", {theme::bone, 0xffc8b890}},            {"bone_dim", {theme::boneDim, 0xff8a7a5a}},
        {"text_dim", {theme::textDim, 0xff584858}},     {"text_ghost", {theme::textGhost, 0xff302830}},
        {"text_phantom", {theme::textPhantom, 0xff201820}}, {"dream", {theme::dream, 0xff585878}},
    };
    for (const auto& [name, token] : tokens)
        CHECK_MESSAGE(token.first.getARGB() == token.second,
                      "the token " << std::string(name) << " is #" << token.first.toDisplayString(false).toStdString()
                                   << " in plugin/ui/Theme.h; section 6 of the design says #"
                                   << juce::Colour(token.second).toDisplayString(false).toStdString());
}

TEST_CASE("P6.12 the ground: near-black margins, never white, at 1x and 2x") {
    Fixture f;
    f.hold(idleFrame(), 2);
    for (const float scale : {1.0f, 2.0f}) {
        const auto image = f.shot(scale);
        INFO("at " << scale << "x");
        CHECK_MESSAGE(image.getWidth() == static_cast<int>(1200 * scale), "the snapshot is the window at the scale asked");
        const auto all = measure(image, {0, 0, 1200, 760}, scale);
        CHECK_MESSAGE(all.nearWhite == 0, all.nearWhite << " pixels are near-white");
        for (const auto margin : {juce::Rectangle<int>{0, 736, 24, 24}, juce::Rectangle<int>{1176, 610, 24, 120},
                                  juce::Rectangle<int>{0, 52, 20, 20}, juce::Rectangle<int>{764, 330, 16, 260}}) {
            const auto r = measure(image, margin, scale);
            CHECK_MESSAGE(meanLuma(r) < 0.06, "the margin " << str(margin) << " has mean luma " << meanLuma(r) << "; the ground "
                                              "is bg_void, #060608, with a faint vignette and noise");
        }
    }
}
