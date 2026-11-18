// SPDX-License-Identifier: AGPL-3.0-only
// Phase 7.4 spec tests: the step cells' outlines (docs/specs/phase-7.4-step-outlines.md, checks P7.4.01 and P7.4.02;
// docs/design/window.md section 4.1).
// spec-preset: plugin

#include "doctest.h"

#include "spec_window.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace spec;

namespace {

WindowFrame stepFrame(int step, long long count, double hz = 98.0) {
    WindowFrame f = sineFrame(hz, -10.0);
    f.step = step;
    f.count = count;
    f.root = 43;
    f.bpm = 130.0;
    f.hostClock = true;
    return f;
}

// The luma of a 2x snapshot's pixel.
double at(const juce::Image& image, int px, int py) { return luma(image.getPixelAt(px, py)); }

// The share of a cell's top edge left unlit in a 2x snapshot: its 2x pixel columns from 4 px in from each corner whose
// two rows on the edge (y 96 to 97) both read under luma 0.07. A `rule` line reads about 0.09, a gap the ground.
double topEdgeGaps(const juce::Image& image, int step) {
    const auto cell = designCell(step);
    int gaps = 0, columns = 0;
    for (int px = 2 * (cell.getX() + 4); px < 2 * (cell.getRight() - 4); ++px, ++columns)
        gaps += std::max(at(image, px, 2 * cell.getY()), at(image, px, 2 * cell.getY() + 1)) < 0.07 ? 1 : 0;
    return static_cast<double>(gaps) / columns;
}

// Every cell's top edge: solid for a step that sounds, lit or not; dashed for a rest.
void checkOutlines(Fixture& f, const juce::Image& image, const char* when) {
    for (int s = 0; s < 8; ++s) {
        const bool rest = f.get(("step" + std::to_string(s + 1) + "_vel").c_str()) <= 0.0f;
        const double gaps = topEdgeGaps(image, s);
        INFO(when << ", step " << s + 1 << (rest ? " (a rest)" : " (sounding)") << ": " << 100.0 * gaps
                  << "% of its top edge is gaps");
        if (rest)
            CHECK_MESSAGE(gaps >= 0.30, "a rest is outlined in 1 px rule2 dashes, 3 on and 3 off");
        else
            CHECK_MESSAGE(gaps <= 0.05, "a sounding cell's border is a solid 1 px rule line (rose_bright while lit), "
                                        "never a rest's dashes: the dashes mark a rest alone");
    }
}

}  // namespace

TEST_CASE("P7.4.01 a sounding cell's border is solid, lit or not; only a rest's is dashed, in idle and while "
          "playing") {
    Fixture f;
    f.set("seq_drift", 0.0f);
    f.hold(idleFrame(), 2);  // a new instance, not playing: no cell has played or lit
    checkOutlines(f, f.shot(2.0f), "idle");
    f.hold(stepFrame(1, 41), 8);
    f.hold(stepFrame(3, 43), 3);  // step 4 plays, lit; step 2's light is fading; steps 3, 6, 7 and 8 are unlit
    checkOutlines(f, f.shot(2.0f), "while playing");
}

TEST_CASE("P7.4.02 a trace's edges are strokes joining its columns' extremes: a steep wave's outline never breaks") {
    Fixture f;
    f.set("seq_drift", 0.0f);
    f.hold(stepFrame(1, 41, 49.0), 8);   // step 2 plays a 49 Hz sine through, steep across its cell
    f.hold(stepFrame(2, 42, 49.0), 16);  // step 3 plays on, and step 2's light has gone (250 ms)
    const auto image = f.shot(2.0f);
    const auto trace = designTrace(1);
    double brightest = 0.0;
    for (int py = 2 * trace.getY(); py < 2 * trace.getBottom(); ++py)
        for (int px = 2 * trace.getX(); px < 2 * trace.getRight(); ++px)
            brightest = std::max(brightest, at(image, px, py));
    const double edge = 0.4 * brightest;  // the edges, at the trace's alpha; the band between them is at 20% of it

    // On each side of the middle line (y 131, 2 px clear of it), the rows each 2x pixel column's edge lights. Where
    // two neighbouring columns both light, a joined stroke leaves at most one row between them; a tick per column
    // leaves the whole step of the wave dark.
    auto edgeRows = [&](int px, bool above) {
        std::vector<int> rows;
        for (int py = 2 * trace.getY(); py < 2 * trace.getBottom(); ++py)
            if ((above ? py < 258 : py > 265) && at(image, px, py) >= edge)
                rows.push_back(py);
        return rows;
    };
    int pairs = 0, breaks = 0;
    for (const bool above : {true, false}) {
        auto previous = edgeRows(2 * trace.getX() + 1, above);
        for (int px = 2 * trace.getX() + 2; px < 2 * trace.getRight() - 1; ++px) {
            const auto rows = edgeRows(px, above);
            if (!rows.empty() && !previous.empty()) {
                int gap = 1 << 20;
                for (const int a : previous)
                    for (const int b : rows)
                        gap = std::min(gap, std::abs(a - b) - 1);
                ++pairs;
                breaks += gap >= 2 ? 1 : 0;
            }
            previous = rows;
        }
    }
    CHECK_MESSAGE(pairs >= 100, "a 49 Hz sine at -10 dBFS draws its edges across step 2's trace; found " << pairs
                  << " neighbouring pixel columns both lit by an edge");
    CHECK_MESSAGE(breaks <= pairs / 20, "each edge is one 1 px stroke joining the columns' maxima (or minima), never a "
                  "tick per column: the outline breaks between " << breaks << " of " << pairs << " neighbouring pixel "
                  "columns");
}
