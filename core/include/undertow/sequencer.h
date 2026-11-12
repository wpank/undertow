#pragma once

#include "undertow/pattern.h"

#include <array>

namespace undertow {

// A step starting: which of the 8 it is (0 .. 7), the note it plays and its velocity (0 is a rest).
struct SeqStep {
    int step = 0;
    int note = 0;
    float velocity = 0.0f;
};

class Sequencer {
public:
    void prepare(double sampleRate);
    void reset();                          // no key held or remembered, nothing playing
    void setParams(const float* values);    // kNumParams values in real units
    void keyOn(int note);                  // a MIDI key goes down
    void keyOff(int note);                 // a MIDI key comes up
    void allKeysOff();                     // every key comes up
    bool anyKeyHeld() const;               // any key is down
    // One sample of the clock, after that sample's MIDI: `beat` is its position in quarter notes, `running` whether
    // it moves and `bpm` its tempo. Returns true when a step starts at this sample, and fills `out`.
    bool tick(double beat, bool running, double bpm, SeqStep& out);
    int step() const;                      // the step playing (0 .. 7), or -1 while the pattern does not play
    long long count() const;               // the count of the step playing, 0 while none plays
    int root() const;                      // the root of the step playing (a MIDI note), -1 while none plays

private:
    static long long floorDiv(long long a, long long b);
    static long long countFromBeat(double beat, double stepBeats);
    static int positiveMod(long long count, int length);

    enum class Source { None, Host, Local };

    int latestHeld() const;
    int computeRoot(int mode) const;
    long long computeCount(Source source, double beat, double stepBeats) const;
    void startStep(long long count, int root, SeqStep& out);

    Pattern pattern_;
    double sampleRate_ = 48000.0;
    float params_[kNumParams]{};

    std::array<int, 16> held_{};
    int heldCount_ = 0;
    bool hasLastPressed_ = false;
    int lastPressed_ = 0;

    bool playing_ = false;
    Source source_ = Source::None;
    long long count_ = 0;
    int step_ = -1;
    int root_ = -1;
    double localBeat_ = 0.0;
    double hostBeat_ = 0.0;
    bool haveHostBeat_ = false;
};

}  // namespace undertow
