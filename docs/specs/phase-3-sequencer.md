# Phase 3: the hypnotic sequencer

**Goal**: a 16-step bassline sequencer locked to the host's transport: per step a note, rest, accent, slide and
probability; rate, length, gate and swing; a root and a scale; MIDI transposes it; Mutate changes a set share of the
steps within the scale, and Evolve mutates every N bars, so a loop keeps moving without leaving the key.

**Depends on**: phase 1 (the voice in the engine). Phase 2 is not needed by its checks, but run the phases in order:
both edit `core/src/engine.cpp`.

**Size**: M, 2 tasks, about 700 lines.

---

## Scope

In scope: `undertow/sequencer.h` with `core/src/sequencer.cpp`; `Engine::setPattern` and `Engine::pattern` in
`undertow/engine.h`; the sequencer wired into `core/src/engine.cpp`; the new source in `core/CMakeLists.txt`.

Out of scope: editing the pattern in the editor and saving it with the plugin's state (phase 4).

## Context

- Sound-design goal: `docs/PRD.md` section 1.4; parameters `seq_*` in section 4; the beat clock in section 3.4.
- The spec tests: `tests/spec/p3/sequencer_test.cpp` (the `Sequencer` on its own) and `engine_test.cpp` (in the
  engine). Change the code, never the tests. Phases 0 to 2 must keep passing (`tools/check.sh`).
- The steps are state, not host parameters: the engine holds the pattern; the editor (phase 4) changes it through
  `Engine::setPattern`, from the message thread, while the audio thread plays.

## What to build

1. **Steps, patterns and scales** (`core/include/undertow/sequencer.h`, `core/src/sequencer.cpp`)
   - `Step`, `Pattern` (16 steps), `operator==`/`!=` for both, `Scale`, and `inScale(scale, semitones)`, true when
     `semitones` modulo 12 is a degree of the scale: Chromatic all; Minor 0 2 3 5 7 8 10; Phrygian 0 1 3 5 7 8 10;
     Dorian 0 2 3 5 7 9 10; Minor Pent 0 3 5 7 10.
   - `defaultPattern()`, what a new instance plays (A minor with the default root 33):

     | Step | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
     |---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
     | semitones | 0 | 0 | 12 | 0 | 0 | 0 | 12 | 10 | 0 | 0 | 12 | 0 | 3 | 0 | 7 | 12 |
     | gate | on | on | on | on | on | rest | on | on | on | on | on | on | on | on | on | on |
     | accent | | | yes | | | | | yes | | | yes | | | | | yes |
     | slide | | | | | | | yes | | | | | | | | yes | |
     | probability | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 0.5 | 1 | 1 | 1 | 1 |

2. **The sequencer** (`Sequencer`, same files)
   - Step length in beats from `seq_rate`: 1/8 0.5, 1/16 0.25, 1/32 0.125, 1/16T 1/6. Steps are numbered from beat 0:
     step `k` starts at `k * stepBeats`, odd steps `seq_swing / 3` of a step later. It plays pattern step
     `k mod seq_length`.
   - `process(ppq, bpm, running, numSamples, out, maxEvents)` writes the block's note events with their sample
     offsets (`round((beat - ppq) / beatsPerSample)`), in time order, and returns how many it wrote.
   - A step plays when its gate is on and `random(seq_seed, k) < probability`, where `random` is a well-mixed hash
     of the seed and the step number `k` to [0, 1): the same position always decides the same way, so a host loop
     replays the same hits.
   - The note is `seq_root + transpose + semitones`, clamped to 0 .. 127. A note-on carries the step's accent.
   - A note ends `seq_gate` of a step after it starts. A slide step's note is held into the next step instead: the
     next note's note-on comes first and the slid note's note-off at the same offset right after it (legato, so the
     voice glides); a slide into a rest or a step that does not play ends at that step's start.
   - Start and jumps: when the clock starts, or the block's `ppq` is not where the last block ended (a loop or a
     relocation), end any playing note at offset 0 and carry on from the first step that starts at or after `ppq`
     (a step exactly at `ppq`, within half a sample, plays at offset 0).
   - Stop (`running` false): end any playing note at offset 0, send nothing else; `currentStep()` is -1.
   - `currentStep()` is the pattern index of the last step reached (whether or not it played); -1 before the first.
   - `mutate(amount)`: each step, with probability `amount`, gets one edit: half the time a new note, a random degree
     of the scale within -12 .. +12 semitones; a quarter of the time its accent toggled; a quarter its slide toggled.
     Gates and probabilities never change. The random sequence comes from `seq_seed` and a count of mutations since
     `reset()`, so the same seed and pattern mutate the same way.
   - Evolve: with `seq_evolve` N bars (1, 2, 4, 8, 16), call `mutate(seq_mutate)` at every step that starts a bar
     whose number is a multiple of N (beat a multiple of 4 N), except the first step after a start or jump.
   - No allocation anywhere: `out` is the caller's array.

3. **The engine** (`core/include/undertow/engine.h`, `core/src/engine.cpp`, `core/CMakeLists.txt`)
   - The engine owns a `Sequencer` (prepared with the engine, holding `defaultPattern()` after `prepare`).
   - The clock: with `seq_sync` Host and a valid host transport, the host's position and tempo; with `seq_sync`
     Internal, or without a host transport, the internal clock at `seq_bpm` (it always runs). The LFOs and the
     rumble follow the same clock.
   - With `seq_on`: each block, the sequencer's events drive the voice at their offsets (note-on velocity 1.0 when
     accented, else 90/127); MIDI note-ons do not play the voice but set the transpose to `note - seq_root` (latched
     until the next one), applied before the sequencer plays the block they arrive in, so a key pressed on a step
     transposes that step; MIDI note-offs are ignored; pitch bend and controllers still apply. Turning `seq_on` off
     ends the sequencer's note; with it off, MIDI plays the voice as in phase 1.
   - `setPattern(pattern)` may be called from any thread while `process` runs: no lock, no allocation (a seqlock
     or double buffer); the audio thread takes the new pattern at the start of its next block. `pattern()` returns,
     from any thread, the newest pattern, never a mix of two: one set with `setPattern` that the audio thread has not
     taken yet, else the one it is using (including evolve's mutations). So a caller reads back at once what it set,
     even with no audio running (an editor makes several edits between two audio blocks; a host may stop its audio).
   - Apple silicon is weakly ordered, so a seqlock needs fences. The writer stores an odd count, issues a release
     fence, copies the pattern, and stores the next even count with release. The reader loads the count with
     acquire, copies, issues an acquire fence, loads the count again, and retries on an odd or changed count. Without
     the fences P3.13 sees torn patterns.
   - `meters().step` is the sequencer's current step while `seq_on`, else -1.
   - Add `src/sequencer.cpp` to `undertow_core`.

## API

```cpp
// core/include/undertow/sequencer.h
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
bool operator==(const Step& a, const Step& b);
bool operator!=(const Step& a, const Step& b);

constexpr int kSteps = 16;
struct Pattern {
    std::array<Step, kSteps> steps{};
};
bool operator==(const Pattern& a, const Pattern& b);
bool operator!=(const Pattern& a, const Pattern& b);
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
    void prepare(double sampleRate);
    void reset();                              // stopped, nothing playing, mutation count 0
    void setParams(const float* values);       // the seq_* parameters
    void setPattern(const Pattern& pattern);   // the caller's thread; the engine calls it on the audio thread
    const Pattern& pattern() const;
    void setTranspose(int semitones);
    int process(double ppq, double bpm, bool running, int numSamples, SeqEvent* out, int maxEvents);
    int currentStep() const;
    void mutate(float amount);
};
}  // namespace undertow

// core/include/undertow/engine.h, added to class Engine (with #include "undertow/sequencer.h")
void setPattern(const Pattern& pattern);   // any thread, lock-free; used from the next block on
Pattern pattern() const;                   // any thread; the newest pattern (set, or in use)
```

## Acceptance checks

| Check | Command | Proves |
|---|---|---|
| P3.01 | `tools/spec.sh 3 sequencer 'P3.01*'` | Steps start on the 1/16 grid to within a sample at 128 BPM across odd block sizes, notes last seq_gate of a step, and currentStep follows |
| P3.02 | `tools/spec.sh 3 sequencer 'P3.02*'` | seq_rate 1/8 doubles the step, seq_length 5 cycles five steps, and swing 1 delays odd steps by a third of a step |
| P3.03 | `tools/spec.sh 3 sequencer 'P3.03*'` | Rests play nothing, accents reach the note-on, a slide holds into the next note (note-on before note-off, same sample), and a slide into a rest ends there |
| P3.04 | `tools/spec.sh 3 sequencer 'P3.04*'` | Stopping ends the note at once and sends nothing more; a loop back to beat 0 ends the note and plays step 0 at once; a start mid-step waits for the next step |
| P3.05 | `tools/spec.sh 3 sequencer 'P3.05*'` | Probability 0.5 plays about half the steps, the same seed and position decide the same way (loops repeat), another seed differs, 0 and 1 are never and always |
| P3.06 | `tools/spec.sh 3 sequencer 'P3.06*'` | Notes are root + transpose + the step's semitones |
| P3.07 | `tools/spec.sh 3 sequencer 'P3.07*'` | The scales are right; mutate(0) changes nothing; mutate(0.25) changes about a quarter of the steps, the same way for the same seed, in scale, within an octave, gates untouched |
| P3.08 | `tools/spec.sh 3 sequencer 'P3.08*'` | Evolve mutates at the bar line and never inside a bar; with Evolve off the pattern never changes |
| P3.09 | `tools/spec.sh 3 sequencer 'P3.09*'` | The sequencer's process, setParams, mutate and setPattern never allocate |
| P3.10 | `tools/spec.sh 3 engine 'P3.10*'` | With seq_on the engine plays the pattern's notes on the host's grid, meters().step follows, a MIDI note transposes it (latched; the key never plays or cuts the voice), and with seq_on off MIDI plays directly again |
| P3.11 | `tools/spec.sh 3 engine 'P3.11*'` | An accented step is about twice as loud at accent 1; a slide glides into the next note in glide_ms without retriggering the envelope |
| P3.12 | `tools/spec.sh 3 engine 'P3.12*'` | seq_sync Internal runs at seq_bpm ignoring the host; Host falls back to the internal clock without a host; a host stop releases the note |
| P3.13 | `tools/spec.sh 3 engine 'P3.13*'` | setPattern and pattern() from another thread while the audio runs: pattern() reads back a set pattern at once, the new pattern is used, never torn, and process never allocates |
| P3.14 | `tools/spec.sh 3 engine 'P3.14*'` | A new engine starts with defaultPattern(), and defaultPattern() is the table above |
| P3.15 | `tools/spec.sh 3 engine 'P3.15*'` | 60 s of the default patch with the sequencer and Evolve on renders in at most 4 s of CPU time (15x real time) |

## Rules every task keeps

- The rules of phases 0 to 2 still hold: never edit `tests/spec/`, `third_party/`, `docs/` or the given tools; `core/`
  stays plain C++17; nothing `process` calls allocates, locks, does I/O or logs.
- The sequencer decides everything from the beat position, the seed and its pattern: no clock, no global random
  generator, no time of day.

## Also required, not checked automatically

- Listening: at 128 BPM the default pattern with the default patch sounds like a rolling hypnotic techno bassline;
  Evolve every 4 bars at Mutate 0.25 keeps it moving without losing its shape.
- A pattern change from the editor (phase 4) takes effect within one block and never clicks worse than a note change.

## Notes for the planner

- Two tasks, one per test file. Each task's verify steps are exactly the checks named for it, with their commands
  unchanged, and each passes once that task (and the one it needs) is done:
  1. **The sequencer** (`sequencer.h`, `sequencer.cpp`): steps, patterns, scales, the default pattern, `process`,
     `mutate` and evolve. P3.01 to P3.09.
  2. **The engine wiring and the pattern handoff** (`engine.h`, `engine.cpp`, `core/CMakeLists.txt`): P3.10 to P3.15.
     Needs task 1.
- A test file is one executable, so the sequencer is one task: `sequencer_test.cpp` compiles only once every
  function it calls exists (including `mutate`).
- Every task builds C++: its agent needs `bash` and `edit_file`, to run `cmake` and its own `tools/spec.sh` check
  before it hands off. Leave `allowed_tools` out, or include `bash`, `edit_file`, `read_file`, `write_file`, `grep`
  and `glob`.
- `tools/check.sh` (every present spec test, phases 0 to 3) is the plan's final check, not any one task's.
- The sequencer tests run it at 128 BPM, where a 1/16 step is 5625 samples at 48 kHz; read `sequencer_test.cpp` for
  the patterns each test uses.
