#pragma once
#include "undertow/params.h"
#include "undertow/sequencer.h"
#include <cstdint>
#include <memory>

namespace undertow {

constexpr int kScopeSamples = 4096;

struct MidiEvent {
    int sampleOffset;      // 0 .. numSamples - 1, sorted
    std::uint8_t status;   // raw MIDI status byte, any channel
    std::uint8_t data1;
    std::uint8_t data2;
};

struct Transport {
    bool valid = false;    // the host gave a beat position; false in the Standalone app
    bool playing = false;
    double bpm = 120.0;
    double ppq = 0.0;      // quarter notes at the block's first sample
};

struct Meters {
    float peak = 0;        // output peak of the last block, both channels
    float rms = 0;         // smoothed output RMS, about 50 ms
    bool clipping = false; // phase 2
    int note = -1;         // phase 1: the sounding note, -1 when idle
    float noteHz = 0;      // phase 1: the voice's pitch now, 0 when idle
    int step = -1;         // phase 3: the sequencer step playing, -1 when stopped
    float rumble = 0;      // phase 2: the rumble bus RMS
    double ppq = 0;        // the beat clock at the end of the last block
    bool hostClock = false;   // the host's transport drives the clock
    double bpm = 120.0;       // the tempo in use
};

class Engine {
public:
    Engine();
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void prepare(double sampleRate, int maxBlockSize);  // allocates everything; calls reset()
    void reset();                                       // silence and idle; internal clock to 0
    void setParam(int index, float value);              // real units; clamped, rounded
    float getParam(int index) const;
    void process(float* left, float* right, int numSamples,
                 const MidiEvent* events, int numEvents, const Transport& transport);
    Meters meters() const;                              // safe from another thread
    int copyScope(float* destination, int count) const;  // latest samples, oldest first; any thread; no allocation

    void setPattern(const Pattern& pattern);            // any thread, lock-free; used from the next block on
    Pattern pattern() const;                            // any thread; the newest pattern (set, or in use)

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace undertow
