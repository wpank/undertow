// Phase 2 spec tests: the tempo-synced LFO (docs/specs/phase-2-rumble.md, P2.01 to P2.03).
// spec-needs: core/include/undertow/lfo.h

#include "doctest.h"

#include "undertow/lfo.h"

#include <cmath>
#include <set>
#include <string>

namespace {

using undertow::LfoShape;

bool near(double value, double want, double tolerance) { return std::fabs(value - want) <= tolerance; }

undertow::Lfo make(int rate, LfoShape shape, double offset = 0.0, std::uint32_t seed = 1) {
    undertow::Lfo lfo;
    lfo.setRate(rate);
    lfo.setShape(shape);
    lfo.setPhaseOffset(offset);
    lfo.setSeed(seed);
    return lfo;
}

} // namespace

TEST_CASE("P2.01 the rate choices are cycle lengths in beats, straight, triplet, dotted and polyrhythmic") {
    const double want[] = {16, 8, 4, 2, 1, 0.5, 0.25, 0.125, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 0.75, 1.25, 1.75, 3, 5, 7};
    const char* names[] = {"4 bars", "2 bars", "1 bar", "1/2",  "1/4", "1/8", "1/16", "1/32", "1/4T",
                           "1/8T",   "1/16T",  "3/16",  "5/16", "7/16", "3/4", "5/4",  "7/4"};
    REQUIRE(undertow::kLfoRates == 17);
    for (int i = 0; i < 17; ++i)
        CHECK_MESSAGE(near(undertow::lfoCycleBeats(i), want[i], 1e-12),
                      "rate " << i << " (" << std::string(names[i]) << ") must last " << want[i] << " beats; lfoCycleBeats gives "
                              << undertow::lfoCycleBeats(i));
}

TEST_CASE("P2.02 the shapes, the phase offset and sample-and-hold") {
    // A 1-beat LFO, so the beat position is the phase.
    struct Point {
        LfoShape shape;
        double phase, value;
    };
    const Point points[] = {
        {LfoShape::Sine, 0.0, 0.0},      {LfoShape::Sine, 0.25, 1.0},      {LfoShape::Sine, 0.75, -1.0},
        {LfoShape::Triangle, 0.0, 0.0},  {LfoShape::Triangle, 0.125, 0.5}, {LfoShape::Triangle, 0.25, 1.0},
        {LfoShape::Triangle, 0.5, 0.0},  {LfoShape::Triangle, 0.75, -1.0}, {LfoShape::SawDown, 0.0, 1.0},
        {LfoShape::SawDown, 0.5, 0.0},   {LfoShape::SawDown, 0.75, -0.5},  {LfoShape::SawUp, 0.0, -1.0},
        {LfoShape::SawUp, 0.25, -0.5},   {LfoShape::SawUp, 0.5, 0.0},      {LfoShape::Square, 0.1, 1.0},
        {LfoShape::Square, 0.49, 1.0},   {LfoShape::Square, 0.51, -1.0},   {LfoShape::Square, 0.9, -1.0},
    };
    const char* names[] = {"Sine", "Triangle", "Saw Down", "Saw Up", "Square", "Sample & Hold"};
    for (const auto& point : points) {
        const auto lfo = make(4, point.shape);
        const double got = lfo.valueAt(7.0 + point.phase);
        CHECK_MESSAGE(near(got, point.value, 1e-9), std::string(names[static_cast<int>(point.shape)])
                                                       << " at phase " << point.phase << " must be " << point.value
                                                       << "; valueAt gives " << got);
    }
    const auto shifted = make(4, LfoShape::Sine, 0.25);
    CHECK_MESSAGE(near(shifted.valueAt(3.0), 1.0, 1e-9), "a phase offset of 0.25 must put a sine's peak on the beat; got "
                                                             << shifted.valueAt(3.0));

    // Sample & hold: one value per cycle, the same for the same seed and cycle, spread over -1 .. 1.
    const auto hold = make(4, LfoShape::SampleHold, 0.0, 7);
    std::set<double> values;
    double sum = 0, squares = 0;
    for (int cycle = 0; cycle < 1000; ++cycle) {
        const double v = hold.valueAt(cycle + 0.1);
        CHECK_MESSAGE(near(hold.valueAt(cycle + 0.9), v, 0.0), "Sample & Hold must hold one value for the whole cycle");
        CHECK_MESSAGE(near(make(4, LfoShape::SampleHold, 0.0, 7).valueAt(cycle + 0.5), v, 0.0),
                      "Sample & Hold must give the same value for the same seed and cycle");
        CHECK_MESSAGE(std::fabs(v) <= 1.0, "Sample & Hold left -1 .. 1: " << v);
        values.insert(v);
        sum += v;
        squares += v * v;
    }
    CHECK_MESSAGE(values.size() >= 990, "Sample & Hold repeated itself: only " << values.size() << " values in 1000 cycles");
    CHECK_MESSAGE(std::fabs(sum / 1000.0) <= 0.08, "Sample & Hold's mean over 1000 cycles is " << sum / 1000.0 << "; want about 0");
    CHECK_MESSAGE(std::sqrt(squares / 1000.0) >= 0.45, "Sample & Hold's values must spread over -1 .. 1 (RMS about 0.58)");
    int same = 0;
    const auto other = make(4, LfoShape::SampleHold, 0.0, 8);
    for (int cycle = 0; cycle < 100; ++cycle)
        same += near(other.valueAt(cycle + 0.5), hold.valueAt(cycle + 0.5), 1e-12) ? 1 : 0;
    CHECK_MESSAGE(same <= 2, "a different seed must give different Sample & Hold values (" << same << " of 100 equal)");
}

TEST_CASE("P2.03 an LFO's value is a function of the beat position: locked to the bar, polyrhythms included") {
    for (int rate = 0; rate < 17; ++rate) {
        const auto lfo = make(rate, LfoShape::Triangle, 0.1);
        const double cycle = undertow::lfoCycleBeats(rate);
        for (double ppq : {0.0, 0.3, 5.7, 123.45})
            CHECK_MESSAGE(near(lfo.valueAt(ppq + cycle), lfo.valueAt(ppq), 1e-9),
                          "rate " << rate << " must repeat every " << cycle << " beats (at ppq " << ppq << ")");
    }
    const auto five = make(12, LfoShape::Sine); // 5/16: 1.25 beats
    CHECK_MESSAGE(!near(five.valueAt(4.0), five.valueAt(0.0), 0.1), "a 5/16 LFO must drift against the bar");
    CHECK_MESSAGE(near(five.valueAt(20.0), five.valueAt(0.0), 1e-9), "a 5/16 LFO comes back to the bar after 5 bars (20 beats)");
    const auto three = make(11, LfoShape::Square); // 3/16: 0.75 beats
    CHECK_MESSAGE(near(three.valueAt(12.0 + 0.1), three.valueAt(0.1), 1e-12), "a 3/16 LFO comes back after 3 bars (12 beats)");
    // The same beat position gives the same value however it was reached: a host loop replays the same motion.
    const auto bar = make(2, LfoShape::Sine);
    const double before = bar.valueAt(1.0);
    (void)bar.valueAt(57.3);
    CHECK_MESSAGE(near(bar.valueAt(1.0), before, 0.0), "valueAt must not depend on earlier calls");
    CHECK_MESSAGE(near(bar.valueAt(1.0), 1.0, 1e-9), "a 1-bar sine peaks on beat 2 (ppq 1)");
}
