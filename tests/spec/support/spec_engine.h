#pragma once

// Helpers for the spec tests that drive undertow::Engine or undertow::MonoVoice (phase 1 on): parameter sets,
// MIDI events and offline rendering in blocks with a moving host transport.

#include "spec_analysis.h"
#include "undertow/engine.h"
#include "undertow/params.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <vector>

namespace spec {

using Params = std::array<float, undertow::kNumParams>;

inline int id(std::string_view name) { return undertow::findParam(name); }

inline Params defaults() {
    Params p{};
    for (int i = 0; i < undertow::kNumParams; ++i)
        p[static_cast<std::size_t>(i)] = undertow::paramSpec(i).def;
    return p;
}

inline void set(Params& p, std::string_view name, float value) { p[static_cast<std::size_t>(id(name))] = value; }

// A plain, measurable patch: the saw at level 0.5 through a wide-open filter, no sub, flat envelopes, no glide,
// no accent, no modulation, and every later phase's stage neutral (no rumble, saturation, width or sequencer;
// master gain 0 dB). Tests change only what they measure.
inline Params cleanPatch() {
    Params p = defaults();
    set(p, "osc_wave", 0);
    set(p, "osc_pw", 0.5f);
    set(p, "osc_level", 0.5f);
    set(p, "osc_octave", 0);
    set(p, "osc_fine", 0);
    set(p, "sub_level", 0);
    set(p, "sub_octave", 0);
    set(p, "filter_cutoff", 18000);
    set(p, "filter_res", 0);
    set(p, "filter_drive", 0);
    set(p, "filter_env", 0);
    set(p, "filter_track", 0);
    set(p, "amp_attack", 0.5f);
    set(p, "amp_decay", 1);
    set(p, "amp_sustain", 1);
    set(p, "amp_release", 1);
    set(p, "filt_attack", 0.5f);
    set(p, "filt_decay", 1);
    set(p, "filt_sustain", 0);
    set(p, "filt_release", 1);
    set(p, "glide_ms", 0);
    set(p, "glide_mode", 1);
    set(p, "accent", 0);
    set(p, "bend_range", 2);
    set(p, "lfo1_cutoff", 0);
    set(p, "lfo2_depth", 0);
    set(p, "rumble_level", 0);
    set(p, "rumble_send", 0);
    set(p, "rumble_kick", 0);
    set(p, "sat_drive", 0);
    set(p, "width", 0);
    set(p, "master_gain", 0);
    set(p, "seq_on", 0);
    return p;
}

// A sub-only patch: the sine at level 0.5, nothing else.
inline Params subPatch() {
    Params p = cleanPatch();
    set(p, "osc_level", 0);
    set(p, "sub_level", 0.5f);
    return p;
}

inline void apply(undertow::Engine& engine, const Params& p) {
    for (int i = 0; i < undertow::kNumParams; ++i)
        engine.setParam(i, p[static_cast<std::size_t>(i)]);
}

inline undertow::MidiEvent noteOn(int at, int note, int velocity = 90) {
    return {at, 0x90, static_cast<std::uint8_t>(note), static_cast<std::uint8_t>(velocity)};
}
inline undertow::MidiEvent noteOff(int at, int note) { return {at, 0x80, static_cast<std::uint8_t>(note), 0}; }
inline undertow::MidiEvent bend(int at, double amount) { // -1 .. 1
    const int value = std::max(0, std::min(16383, static_cast<int>(std::lround(8192 + amount * 8192))));
    return {at, 0xE0, static_cast<std::uint8_t>(value & 0x7F), static_cast<std::uint8_t>(value >> 7)};
}
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
        while (next < events.size() && static_cast<std::size_t>(events[next].sampleOffset) < start + static_cast<std::size_t>(n)) {
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

} // namespace spec
