#pragma once
#include <array>
#include <cstdint>
namespace undertow {

struct Step {
    int semitones = 0;          // relative to the root, -24 .. 24
    bool gate = true;           // false: a rest
    bool accent = false;
    bool slide = false;         // hold into the next step (legato, the voice glides)
    float probability = 1.0f;   // 0 .. 1
};
inline bool operator==(const Step& a, const Step& b) {
    return a.semitones == b.semitones && a.gate == b.gate && a.accent == b.accent
           && a.slide == b.slide && a.probability == b.probability;
}
inline bool operator!=(const Step& a, const Step& b) { return !(a == b); }

constexpr int kSteps = 16;
struct Pattern {
    std::array<Step, kSteps> steps{};
};
inline bool operator==(const Pattern& a, const Pattern& b) { return a.steps == b.steps; }
inline bool operator!=(const Pattern& a, const Pattern& b) { return !(a == b); }
Pattern defaultPattern();

enum class Scale { Chromatic = 0, Minor, Phrygian, Dorian, MinorPentatonic };
bool inScale(Scale scale, int semitones);

struct SeqEvent {
    enum class Type { NoteOn, NoteOff };
    int sampleOffset;
    Type type;
    int note;
    bool accent;
};

class Sequencer {
public:
    Sequencer() = default;
    Sequencer(Sequencer&& o) noexcept;
    Sequencer& operator=(Sequencer&& o) noexcept;
    Sequencer(const Sequencer&) = delete;
    Sequencer& operator=(const Sequencer&) = delete;

    void prepare(double sampleRate);
    void reset();                              // stopped, nothing playing, mutation count 0
    void setParams(const float* values);       // all kNumParams values by parameter id, in real units
    void setPattern(const Pattern& pattern);   // the caller's thread; the engine calls it on the audio thread
    const Pattern& pattern() const;
    void setTranspose(int semitones);
    int process(double ppq, double bpm, bool running, int numSamples, SeqEvent* out, int maxEvents);
    int currentStep() const;
    void mutate(float amount);

private:
    double stepBeat(int k) const;
    int patternStep(int k) const;
    bool stepPlays(int k) const;
    int noteForStep(int k) const;

    Pattern pattern_;

    double sampleRate_ = 48000.0;
    double stepBeats_ = 0.25;
    int seqLength_ = 16;
    float seqGate_ = 0.5f;
    float seqSwing_ = 0.0f;
    int seqRoot_ = 33;
    int seqScale_ = 1;
    int seqSeed_ = 1;
    int seqEvolve_ = 0;
    float seqMutate_ = 0.25f;
    int transpose_ = 0;

    bool running_ = false;
    double lastPpq_ = 0.0;
    bool playing_ = false;
    int playingNote_ = -1;
    bool playingAccent_ = false;
    int playingStepK_ = -1;
    bool playingSlide_ = false;
    int currentStep_ = -1;
    int mutationCount_ = 0;
};

}  // namespace undertow
