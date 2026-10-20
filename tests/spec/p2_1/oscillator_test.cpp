// Phase 2.1 spec tests: the oscillator is clamped at Nyquist (docs/specs/phase-2.1-voice-fixes.md, P2.1.07).
// spec-needs: core/include/undertow/oscillator.h

#include "doctest.h"

#include "spec_analysis.h"
#include "undertow/oscillator.h"

#include <string>

namespace {

constexpr double kRate = 48000.0;

} // namespace

TEST_CASE("P2.1.07 a frequency at or above Nyquist plays nothing audible: no component from 20 Hz to 20 kHz above -60 dB") {
    // Note 127 with a 12-semitone bend at osc_octave +1 asks for 50.2 kHz; phase 2's LFO 2 on Pitch adds 12 more.
    struct Shape {
        const char* name;
        undertow::Wave wave;
        double width;
    };
    const Shape shapes[] = {{"saw", undertow::Wave::Saw, 0.5}, {"square", undertow::Wave::Square, 0.5},
                            {"pulse 0.1", undertow::Wave::Square, 0.1}};
    for (const double hz : {24000.0, 26000.0, 30000.0, 50200.0, 96000.0, 1.0e6}) {
        for (const auto& shape : shapes) {
            undertow::Oscillator osc;
            osc.prepare(kRate);
            osc.setWave(shape.wave);
            osc.setPulseWidth(shape.width);
            osc.reset(0.0);
            osc.setFrequency(hz);
            spec::Signal out(1u << 15);
            for (auto& v : out)
                v = osc.process();
            const auto s = spec::spectrum(out, 8192, 1u << 14, kRate);
            double worst = 0, worstHz = 0;
            for (std::size_t k = 0; k < s.amplitude.size(); ++k) {
                const double f = static_cast<double>(k) * s.binHz();
                if (f >= 20.0 && f <= 20000.0 && s.amplitude[k] > worst) {
                    worst = s.amplitude[k];
                    worstHz = f;
                }
            }
            CHECK_MESSAGE(spec::toDb(worst) <= -60.0, "the " << std::string(shape.name) << " at " << hz << " Hz plays a component "
                                                      "at " << worstHz << " Hz, " << spec::toDb(worst) << " dB: clamp the "
                                                      "frequency to sampleRate / 2, where it plays nothing audible");
        }
    }
}
