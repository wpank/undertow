// SPDX-License-Identifier: AGPL-3.0-only
// Phase 6 spec tests: the window as one, its colour rules, its frame budget and its review images
// (docs/specs/phase-6-window.md, checks P6.35 to P6.38; docs/design/window.md sections 1, 4.1, 6, 9 and 11).
// spec-preset: plugin
// spec-needs: plugin/ui/Controls.cpp
// spec-needs: plugin/ui/Steps.cpp
// spec-needs: plugin/ui/Console.cpp

#include "doctest.h"

#include "spec_engine.h"
#include "spec_paths.h"
#include "spec_window.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

using namespace spec;

namespace {

// The three review states: a new instance (idle), the line playing with the cutoff hovered, and the output clipping.
void showState(Fixture& f, const std::string& state) {
    if (state == "idle") {
        f.hold(idleFrame(), 30);
        return;
    }
    f.perform(kPlayingMoment);
    if (state == "playing") {
        f.editor->hover("vcf_cutoff");
        return;
    }
    auto frame = playingAt(kPlayingMoment);
    frame.clipping = true;
    frame.peak = 0.966f;
    f.hold(frame, 6);
}

// The dial of a knob: where its arc and pointer are drawn.
juce::Rectangle<int> dial(const char* id) {
    const auto& p = designPlace(id);
    return juce::Rectangle<float>(p.x - p.w / 2, p.y - p.w / 2, p.w, p.w).toNearestInt();
}

// The sequencer on a step, as the engine reports it: a 98 Hz sine at -10 dBFS on the host's clock.
WindowFrame stepFrame(int step, long long count) {
    WindowFrame f = sineFrame(98.0, -10.0);
    f.step = step;
    f.count = count;
    f.root = 43;
    f.hostClock = true;
    return f;
}

}  // namespace

TEST_CASE("P6.35 the window as one: the playing column's knobs light with its cell and fade with it; past the length and "
          "in Notes mode a column's knobs dim") {
    Fixture f;
    f.set("step2_vel", 0.55f);  // steps 2 and 4 at the same velocity, so only the light differs
    f.set("step4_vel", 0.55f);
    f.hold(stepFrame(3, 43), 3);
    auto image = f.shot();
    CHECK_MESSAGE(measure(image, dial("step4_vel")).bright >= 20, "the playing column's velocity knob lights rose_bright "
                  "(controls.setLight(step, steps.light(step)) on every tick)");
    CHECK_MESSAGE(measure(image, dial("step4_pitch")).bright >= 4, "the playing column's pitch knob lights too (its pointer)");
    CHECK_MESSAGE(measure(image, dial("step2_vel")).bright <= 2, "a column that is not playing stays rose");
    f.hold(stepFrame(4, 44), 1);
    CHECK_MESSAGE(measure(f.shot(), dial("step4_vel")).bright >= 1, "the knobs fade back with their cell, over 250 ms");
    f.hold(stepFrame(4, 44), 16);
    CHECK_MESSAGE(measure(f.shot(), dial("step4_vel")).bright <= 2, "250 ms after its step moved on the column's knobs are rose");

    const auto full = f.shot();
    f.set("seq_length", 5.0f);
    f.hold(stepFrame(4, 44), 2);
    const auto shortened = f.shot();
    for (const char* id : {"step6_vel", "step7_vel", "step8_vel", "step7_pitch"}) {
        const double before = meanLuma(measure(full, dial(id))), after = meanLuma(measure(shortened, dial(id)));
        CHECK_MESSAGE(after <= 0.75 * before, "`" << id << "` is past the length (5): its knob dims to 32% "
                                              "(controls.setColumnDim); mean luma " << before << " to " << after);
    }
    CHECK_MESSAGE(meanLuma(measure(shortened, dial("step3_vel"))) >= 0.9 * meanLuma(measure(full, dial("step3_vel"))),
                  "a knob inside the length keeps its look");
    f.set("seq_length", 8.0f);
    f.set("seq_mode", 2.0f);
    f.hold(idleFrame(), 2);
    const auto notes = f.shot();
    CHECK_MESSAGE(meanLuma(measure(notes, dial("step3_vel"))) <= 0.75 * meanLuma(measure(full, dial("step3_vel"))),
                  "in Notes mode every step's knobs dim");
}

TEST_CASE("P6.36 the whole window keeps ROSEDUST's rules: near-black, never white, rose, and bone only for the number") {
    for (const std::string state : {"idle", "playing", "clipping"})
        for (const float scale : {1.0f, 2.0f}) {
            Fixture f;
            showState(f, state);
            const auto image = f.shot(scale);
            const auto r = measure(image, {0, 0, 1200, 760}, scale);
            INFO("the window " << state << " at " << scale << "x");
            CHECK_MESSAGE(r.voidPixels >= r.pixels * 0.80, "only " << 100.0 * r.voidPixels / r.pixels << "% of the window is "
                                                           "near-black (luma under 0.06); restraint keeps at least 80%: no "
                                                           "boxes around sections, glass only for live displays");
            CHECK_MESSAGE(r.nearWhite == 0, r.nearWhite << " pixels are near-white; the brightest colours are rose_bright and "
                                                           "bone, never white");
            CHECK_MESSAGE(r.rose >= r.colouredPixels * 0.90, "only " << 100.0 * r.rose / std::max(1, r.colouredPixels)
                                                             << "% of the coloured pixels are in the rose hue band (300 to 15 "
                                                             "degrees); every token but bone and dream is rose");
            const auto box = (kBoneBox.toFloat() * scale).toNearestInt();
            if (state == "idle") {
                CHECK_MESSAGE(r.bone == 0, r.bone << " pixels are bone-like before any note: bone is the one number, drawn "
                                                     "only once a note has played");
            } else {
                CHECK_MESSAGE(r.bone >= 200 * scale * scale, "the sub's number (bone) must show while a note plays; found "
                                                             << r.bone << " bone-like pixels");
                CHECK_MESSAGE(box.contains(r.boneBox), "bone-like pixels span " << str(r.boneBox) << "; bone is only for the "
                                                       "one number, inside " << str(box));
            }
        }
}

TEST_CASE("P6.37 a full repaint keeps 60 Hz with headroom: median 11 ms and 95th percentile 16 ms at 1x, median 27 ms "
          "at 2x") {
    Fixture f;
    auto timeFrames = [&](float scale) {
        juce::Image image(juce::Image::ARGB, static_cast<int>(1200 * scale), static_cast<int>(760 * scale), true);
        std::vector<double> ms;
        for (int i = 0; i < 140; ++i) {
            const double t = 0.5 + i / 60.0;
            const double start = threadSeconds();
            f.editor->advance(playingAt(t), 1.0 / 60.0);
            juce::Graphics g(image);
            g.addTransform(juce::AffineTransform::scale(scale));
            f.editor->paintEntireComponent(g, false);
            ms.push_back((threadSeconds() - start) * 1000.0);
        }
        ms.erase(ms.begin(), ms.begin() + 20);
        std::sort(ms.begin(), ms.end());
        return std::pair<double, double>(ms[ms.size() / 2], ms[ms.size() * 95 / 100]);
    };
    const char* const how = "draw each static layer once per display scale and copy it 1:1; draw glows as wide strokes "
                            "at low alpha, never blurs; analyse the spectrum on every other tick";
    const auto one = timeFrames(1.0f);
    CHECK_MESSAGE(one.first <= 11.0, "a tick and a full repaint at 1x take " << one.first << " ms of CPU (median of 120); "
                                     "the target is 11 ms: " << how);
    CHECK_MESSAGE(one.second <= 16.0, "the 95th percentile at 1x is " << one.second << " ms; the target is 16 ms (one 60 Hz "
                                      "frame): " << how);
    const auto two = timeFrames(2.0f);
    CHECK_MESSAGE(two.first <= 27.0, "a tick and a full repaint at 2x (Retina) take " << two.first << " ms of CPU (median); "
                                     "the target is 27 ms: " << how);
}

TEST_CASE("P6.38 the window writes its review images: idle, playing and clipping at 2x in artifacts/ui, 1x in "
          "build/previews") {
    const juce::File source{juce::String(spec::sourceDir())};
    const juce::File artifacts = source.getChildFile("artifacts/ui"), previews = source.getChildFile("build/previews");
    REQUIRE(artifacts.createDirectory());
    REQUIRE(previews.createDirectory());
    for (const std::string state : {"idle", "playing", "clipping"})
        for (const float scale : {2.0f, 1.0f}) {
            Fixture f;
            showState(f, state);
            const auto image = f.shot(scale);
            const auto file = (scale == 2.0f ? artifacts : previews).getChildFile("window-" + juce::String(state) + ".png");
            file.deleteFile();
            {
                juce::FileOutputStream out(file);
                REQUIRE(out.openedOk());
                juce::PNGImageFormat png;
                REQUIRE(png.writeImageToStream(image, out));
            }
            const auto back = juce::ImageFileFormat::loadFrom(file);
            INFO(file.getFullPathName().toStdString());
            CHECK_MESSAGE(back.getWidth() == static_cast<int>(1200 * scale), "the image must be the window at " << scale << "x");
            CHECK_MESSAGE(back.getHeight() == static_cast<int>(760 * scale), "the image must be the window at " << scale << "x");
            if (scale == 2.0f)
                CHECK_MESSAGE(file.getSize() <= 3 * 1024 * 1024, "a committed review image stays under 3 MB; it is "
                                                                 << file.getSize() << " bytes");
        }
}
