#pragma once
#include "undertow/params.h"
#include <cstdint>
#include <memory>

namespace undertow {

constexpr int kScopeSamples = 4096;

struct MidiEvent {
    int sampleOffset;
    std::uint8_t status;
    std::uint8_t data1;
    std::uint8_t data2;
};

struct Transport {
    bool valid = false;
    bool playing = false;
    double bpm = 120.0;
    double ppq = 0.0;
};

struct Meters {
    float peak = 0;
    float rms = 0;
    bool clipping = false;
    int note = -1;
    float noteHz = 0;
    int step = -1;
    float rumble = 0;
    double ppq = 0;
    bool hostClock = false;
    double bpm = 120.0;
};

class Engine {
public:
    Engine();
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void setParam(int index, float value);
    float getParam(int index) const;
    void process(float* left, float* right, int numSamples,
                 const MidiEvent* events, int numEvents, const Transport& transport);
    Meters meters() const;
    int copyScope(float* destination, int count) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace undertow
