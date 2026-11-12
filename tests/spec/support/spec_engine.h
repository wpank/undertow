// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

// Helpers for the spec tests that drive undertow::Engine: parameter sets, MIDI events and offline rendering in
// blocks with a moving host transport.

#include "spec_analysis.h"
#include "undertow/engine.h"
#include "undertow/params.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <vector>

#include <time.h>

namespace spec {

// The CPU time this thread has used, in seconds. The time budgets count it rather than wall time, so other work
// loading the machine (builds, other tests) cannot fail a check.
inline double threadSeconds() {
    timespec t{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return static_cast<double>(t.tv_sec) + static_cast<double>(t.tv_nsec) * 1e-9;
}

using Params = std::array<float, undertow::kNumParams>;

inline Params defaults() {
    Params p{};
    for (int i = 0; i < undertow::kNumParams; ++i)
        p[static_cast<std::size_t>(i)] = undertow::paramSpec(i).def;
    return p;
}

inline void set(Params& p, int index, float value) { p[static_cast<std::size_t>(index)] = value; }

// Everything that makes a sound turned off: the voice's sources and the rumble. The engine's output is then exact
// silence.
inline Params silentPatch() {
    using namespace undertow::param;
    Params p = defaults();
    set(p, Vco1Level, 0);
    set(p, Vco2Level, 0);
    set(p, NoiseLevel, 0);
    set(p, SubLevel, 0);
    set(p, RumbleLevel, 0);
    set(p, RumbleSend, 0);
    set(p, RumbleKick, 0);
    return p;
}

// The rumble alone, as the defaults have it, with the voice's sources off.
inline Params rumblePatch() {
    using namespace undertow::param;
    Params p = defaults();
    set(p, Vco1Level, 0);
    set(p, Vco2Level, 0);
    set(p, NoiseLevel, 0);
    set(p, SubLevel, 0);
    set(p, RumbleSend, 0);
    set(p, MasterGain, 0);
    return p;
}

inline void apply(undertow::Engine& engine, const Params& p) {
    for (int i = 0; i < undertow::kNumParams; ++i)
        engine.setParam(i, p[static_cast<std::size_t>(i)]);
}

inline undertow::MidiEvent noteOn(int at, int note, int velocity = 100) {
    return {at, 0x90, static_cast<std::uint8_t>(note), static_cast<std::uint8_t>(velocity)};
}
inline undertow::MidiEvent noteOff(int at, int note) { return {at, 0x80, static_cast<std::uint8_t>(note), 0}; }
inline undertow::MidiEvent controller(int at, int number, int value) {
    return {at, 0xB0, static_cast<std::uint8_t>(number), static_cast<std::uint8_t>(value)};
}

struct Stereo {
    Signal left, right;
};

// Render `samples` samples in blocks of `block`, with `events` at absolute sample positions (sorted). With
// `transport.valid`, the transport's ppq moves on with each block while it plays.
inline Stereo render(undertow::Engine& engine, std::size_t samples, std::vector<undertow::MidiEvent> events = {},
                     undertow::Transport transport = {}, int block = 256, double sampleRate = 48000.0) {
    Stereo out;
    out.left.resize(samples);
    out.right.resize(samples);
    std::vector<undertow::MidiEvent> blockEvents;
    blockEvents.reserve(events.size());
    std::size_t next = 0;
    for (std::size_t start = 0; start < samples; start += static_cast<std::size_t>(block)) {
        const int n = static_cast<int>(std::min<std::size_t>(static_cast<std::size_t>(block), samples - start));
        blockEvents.clear();
        while (next < events.size() &&
               static_cast<std::size_t>(events[next].sampleOffset) < start + static_cast<std::size_t>(n)) {
            auto e = events[next++];
            e.sampleOffset = std::max(0, e.sampleOffset - static_cast<int>(start));
            blockEvents.push_back(e);
        }
        engine.process(out.left.data() + start, out.right.data() + start, n, blockEvents.data(),
                       static_cast<int>(blockEvents.size()), transport);
        if (transport.valid && transport.playing)
            transport.ppq += n / sampleRate * transport.bpm / 60.0;
    }
    return out;
}

inline std::size_t ms(double milliseconds, double sampleRate = 48000.0) {
    return static_cast<std::size_t>(std::lround(milliseconds * 0.001 * sampleRate));
}

// The sample at which beat `beat` falls at `bpm`, from beat 0 at sample 0.
inline std::size_t beatSample(double beat, double bpm, double sampleRate = 48000.0) {
    return static_cast<std::size_t>(std::lround(beat * 60.0 / bpm * sampleRate));
}

} // namespace spec
