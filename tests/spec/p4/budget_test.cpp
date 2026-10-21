// Phase 4 spec tests: the frame budget and the review images (docs/specs/phase-4-ui.md, P4.11 and P4.12).
// spec-preset: plugin
// spec-needs: plugin/ui/UiFrame.h

#include "doctest.h"

#include "spec_paths.h"
#include "spec_ui.h"
#include "undertow/params.h"
#include "undertow/sequencer.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include <time.h>

using namespace spec;

namespace {

// The CPU time this thread has used, in ms: other processes loading the machine do not count against the editor.
double threadMs() {
    timespec t{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return static_cast<double>(t.tv_sec) * 1e3 + static_cast<double>(t.tv_nsec) / 1e6;
}

} // namespace

TEST_CASE("P4.11 a full repaint keeps 60 Hz with headroom: median 11 ms and 95th percentile 16 ms at 1x, median 27 ms at 2x") {
    Fixture f;
    auto timeFrames = [&](float scale) {
        juce::Image image(juce::Image::ARGB, static_cast<int>(1200 * scale), static_cast<int>(760 * scale), true);
        std::vector<double> ms;
        UiFrame frame = playingFrame();
        for (int i = 0; i < 140; ++i) {
            frame.energy = 0.5f + 0.5f * static_cast<float>(std::sin(i * 0.1));
            frame.step = (i / 8) % 16;
            frame.ppq += 1.0 / 60.0 * 128.0 / 60.0;
            const double start = threadMs();
            f.editor->advance(frame, 1.0 / 60.0);
            juce::Graphics g(image);
            g.addTransform(juce::AffineTransform::scale(scale));
            f.editor->paintEntireComponent(g, false);
            ms.push_back(threadMs() - start);
        }
        ms.erase(ms.begin(), ms.begin() + 20); // warm-up
        std::sort(ms.begin(), ms.end());
        return std::pair<double, double>(ms[ms.size() / 2], ms[ms.size() * 95 / 100]);
    };
    // A 60 Hz frame is 16.7 ms; a full repaint (every layer, as when the editor opens) must leave the host room.
    const char* const techniques = "cache the static layers at the display's scale (getPhysicalPixelScaleFactor) and draw "
                                   "them 1:1, render the tunnel from precomputed tables at a quarter of the CHEST's "
                                   "resolution, and keep per-pixel trigonometry (atan2) and blurs out of full-resolution "
                                   "layers";
    const auto one = timeFrames(1.0f);
    CHECK_MESSAGE(one.first <= 11.0, "a full repaint at 1x takes " << one.first
                                                                   << " ms of CPU (median of 120 frames); the target is 11 ms, "
                                                                      "so the editor keeps 60 Hz with room for the host: "
                                                                   << std::string(techniques));
    CHECK_MESSAGE(one.second <= 16.0, "the 95th percentile repaint at 1x takes " << one.second
                                                                                 << " ms of CPU; the target is 16 ms (one 60 Hz "
                                                                                    "frame): "
                                                                                 << std::string(techniques));
    const auto two = timeFrames(2.0f);
    CHECK_MESSAGE(two.first <= 27.0, "a full repaint at 2x (Retina) takes " << two.first
                                                                            << " ms of CPU (median); the target is 27 ms (a "
                                                                               "tick repaints only the dynamic regions): "
                                                                            << std::string(techniques));
}


TEST_CASE("P4.12 the editor writes its review images: committed 1x artifacts and 2x previews") {
    Fixture f;
    const juce::File source{juce::String(spec::sourceDir())};  // the checkout this test runs in (spec_paths.h)
    const juce::File artifacts = source.getChildFile("artifacts/ui"), previews = source.getChildFile("build/previews");
    REQUIRE(artifacts.createDirectory());
    REQUIRE(previews.createDirectory());
    const std::pair<const char*, UiFrame> states[] = {
        {"idle", idleFrame()}, {"playing", playingFrame()}, {"clipping", clippingFrame()}};
    for (const auto& [name, frame] : states) {
        for (const float scale : {1.0f, 2.0f}) {
            const auto image = f.shot(frame, 60, scale);
            const auto file = scale == 1.0f ? artifacts.getChildFile("editor-" + juce::String(name) + ".png")
                                            : previews.getChildFile("undertow-" + juce::String(name) + ".png");
            file.deleteFile();
            {
                juce::FileOutputStream out(file);
                REQUIRE(out.openedOk());
                juce::PNGImageFormat png;
                REQUIRE(png.writeImageToStream(image, out));
            }
            const auto back = juce::ImageFileFormat::loadFrom(file);
            INFO(file.getFullPathName().toStdString());
            CHECK_MESSAGE(back.getWidth() == static_cast<int>(1200 * scale), "the image must be the editor at " << scale << "x");
            CHECK_MESSAGE(back.getHeight() == static_cast<int>(760 * scale), "the image must be the editor at " << scale << "x");
            if (scale == 1.0f)
                CHECK_MESSAGE(file.getSize() <= 2 * 1024 * 1024, "a committed artifact must stay under 2 MB; it is "
                                                                     << file.getSize() << " bytes");
        }
    }
}
