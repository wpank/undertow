# Phase 4: the sequencer

**Goal**: Undertow 2 plays its pattern. The 8 steps and their drift, as a pure function of the step count; the
sequencer, which follows the clock and the keys held and says when each step starts; and the engine playing the
voice from it in the `Key` and `Free` modes. A looped bar replays exactly, drift included, and the default pattern,
played as the PRD's render, keeps its energy between 30 and 150 Hz.

**Depends on**: phase 3.

**Size**: M, 3 tasks in order, about 250 lines.

---

## Scope

In scope: `core/include/undertow/pattern.h`, `core/src/pattern.cpp`, `core/include/undertow/sequencer.h` and
`core/src/sequencer.cpp` (new files: the core's source glob builds them without a change to `core/CMakeLists.txt`),
the sequencer wired into `core/src/engine.cpp`, and the step's count and root added to `Meters` in
`core/include/undertow/engine.h`.

Out of scope: the presets, the darker rumble and the renderer's list of hits (phase 5); the window, its step lamps and
its drift marks (phase 6), which read `meters().step`, `count` and `root`.

## Context

- The PRD (`docs/PRD.md`) sections 1.5 (the sequencer and its drift) and 3.4 (the clock) are part of this spec.
- The spec tests: `tests/spec/p4/pattern_test.cpp`, `sequencer_test.cpp` and `engine_test.cpp`, with helpers in
  `tests/spec/support/`. Change the code, never the tests.
- Phase 3's `undertow::Voice` plays each step: `hit(note, velocity)` starts a hit, and velocity 0 is a rest that lets
  a ringing note carry on. Read its spec, `docs/specs/phase-3-voice.md`, for the contract.

## What to build

1. **The pattern and its drift** (`core/include/undertow/pattern.h`, `core/src/pattern.cpp`)
   - `setParams` reads the 8 steps' pitches (`step1_pitch` to `step8_pitch`, whole semitones) and velocities
     (`step1_vel` to `step8_vel`), `seq_length`, `seq_drift`, and `sub_octave` for the drift's guard.
     `home(i)` is step `i` as set, and `length()` is `seq_length`, from 1 to 8.
   - **Counts.** Count 0 is the step that starts at beat 0 of the clock, and the counts before it are negative. With
     `L = seq_length`, count `c` plays step `s = c mod L`, from 0 to `L - 1` also for negative counts (count -1 is the
     last step), on pass `q = floor(c / L)`. At drift 0 every count plays its home step.
   - **The drift** is part of what a count plays on a root, so the same count on the same root always plays the same
     step:
     - **Rests stay rests:** only sounding steps (velocity above 0) drift, so the beats a pattern leaves to the kick
       stay empty.
     - The passes make arcs of 16: arc `a = floor(q / 16)`, and pass `j = q mod 16`, from 0 to 15, within it.
     - On pass `j`, `n = min(S, floor(3 * seq_drift * sin(pi * j / 16) + 0.5))` steps are changed, computed in
       double precision, where `S` is the number of sounding steps among the first `L`: none at an arc's start,
       `round(3 * seq_drift)` in its middle, and never more than one step more or fewer than on the pass before.
     - **Which steps:** order the arc's sounding steps `t` by `hash32(A * 8 + t)`, smallest first, ties to the lower
       `t`, where `A` is the arc `a` as a 32-bit unsigned integer (arc -1 is 4294967295) and the arithmetic wraps at
       32 bits. The first `n` steps in that order are changed. So a step joins as the count rises and leaves as it
       falls, and each arc picks its own steps.
     - **How:** a changed step takes the change `hash32((A * 8 + s) ^ 0x9E3779B9) % 4`: 0 is an octave up (+12), 1
       a fifth up (+7), 2 a fourth down (-5), and 3 a ghost hit at half its velocity, its pitch kept.
     - **The other way:** a pitch move goes the other way to the same note (+12 becomes -12, +7 becomes -5, and -5
       becomes +7) when its own way would leave -12 to +12, or take the sub under the bottom of its range: the sub's
       note, the root plus the new pitch (less 12 when `sub_octave` is -1 oct), must be at least 28, E1 at 41.2 Hz.
       When neither way fits, the step stays at home. On one root, a step keeps its change for the whole arc.
     - `hash32`, exactly:

       ```cpp
       std::uint32_t hash32(std::uint32_t x) {
           x ^= x >> 16;
           x *= 0x7feb352du;
           x ^= x >> 15;
           x *= 0x846ca68bu;
           x ^= x >> 16;
           return x;
       }
       ```

     - So at the default drift (0.7) the passes of every arc change 0, 0, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1 and 0
       of the sounding steps. On the default root, G2, a fourth down from pitch 0 would put the sub at D1 (36.7 Hz),
       so it goes up a fifth instead.

2. **The sequencer** (`core/include/undertow/sequencer.h`, `core/src/sequencer.cpp`)
   - `setParams` gives the pattern its parameters and reads `seq_mode`, `seq_rate` and `seq_root`. A step lasts 4, 2,
     1, 1/2, 1/4, 1/8, 1/3 or 1/6 beats, for the rates 1 bar, 1/2, 1/4, 1/8, 1/16, 1/32, 1/8T and 1/16T.
   - **The keys.** `keyOn(note)` holds a key and `keyOff(note)` lets it go; `allKeysOff()` lets them all go. Hold up to
     16 keys in a fixed array, in the order they went down: a key pressed again while held becomes the latest, and a
     17th pushes out the oldest.
   - **When the pattern plays:**
     - `Key` (mode 0): while a key is held.
     - `Free` (mode 1): whenever the clock runs, and also while a key is held when the clock is stopped.
     - `Notes` (mode 2): never; the engine plays the MIDI notes itself.
   - **The root**, which each step takes as it starts: in `Key`, the latest key still held; in `Free`, the last key
     pressed since `reset()`, or `seq_root` before any. The step's note is the root plus its pitch, the drift
     included (`Pattern::at` on that root), held within 0 to 127. A key pressed mid-step changes nothing until the
     next step starts.
   - **The clock.** The engine calls `tick` once a sample, after that sample's MIDI, with the clock's beat, whether
     it runs and its tempo:
     - While it runs, the count is `floor(beat / stepLength + 1e-9)` (the `1e-9` absorbs rounding in the host's
       position). A step starts at the first sample of each new count, so step `k` starts within one sample of beat
       `k * stepLength`.
     - A step also starts at once, at the count the beat is in, when the pattern begins to play (a key from silence
       in `Key`, the clock starting in `Free`, or a change of mode that starts it) and when the beat moves backwards
       (a host's loop, or a jump back). A jump forward into another count is a new count. So a loop replays its
       steps exactly, drift included, even a loop one step long; a jump within the step that is playing starts
       nothing.
     - While the clock is stopped (the host is stopped), the pattern plays on its own count: it starts at count 0
       (step 1, at home) at the sample it began to play (a key from silence, or the host stopping under a held key)
       and moves on at the given tempo, one step every `stepLength` beats. When the host plays again, its position
       takes over at once: the step there starts at once.
   - `tick` returns true when a step starts, and fills `out` with the step (0 to 7), its note and its velocity. A rest
     starts its step too, at velocity 0.
   - `step()` is the step playing, from 0 to 7, or -1 while the pattern does not play; `count()` is that step's count
     (what `Pattern::at` took) and `root()` its root, the MIDI note it played on; 0 and -1 while nothing plays.
     `reset()` lets every key go, forgets the last key and stops the pattern.

3. **The engine plays the pattern** (`core/src/engine.cpp`)
   - In `prepare`, prepare the sequencer; in `reset`, reset it and give it the parameters; in `process`, give it the
     parameters at each block.
   - MIDI, at each event's own sample, on any channel:
     - a note-on with velocity above 0 is `keyOn(note)`, in every mode; in `Notes` it is also the voice's hit, as in
       phase 3;
     - a note-off, or a note-on at velocity 0, is `keyOff(note)`;
     - controller 123 (all notes off) is `allKeysOff()`;
     - controller 120 (all sound off) resets the voice, the rumble and the output stage, as in phase 3, and is
       `allKeysOff()`.
   - Each sample, after its MIDI, `tick` the sequencer with the engine's clock (PRD section 3.4): the beat at this
     sample, whether the clock runs (the host plays; the engine's own clock always runs, and from phase 7.1 in Key and
     Notes modes only while a key is held) and its tempo (the host's, or `seq_bpm`). When a step starts,
     `voice.hit(note, velocity)`: a rest is no hit, so a ringing note carries on.
   - `meters().step`, `meters().count` and `meters().root` are the sequencer's `step()`, `count()` and `root()` at
     the end of the block, so the window can show which steps have wandered on the pass that plays.

## API

The headers the tasks write. Names, types and signatures are what the spec tests compile against; the private parts
are free.

```cpp
// core/include/undertow/pattern.h
#pragma once

namespace undertow {

// A step as it plays: its pitch in semitones from the root, and its velocity from 0 to 1, where 0 is a rest.
struct Step {
    int pitch = 0;
    float velocity = 0.0f;
};

class Pattern {
public:
    void setParams(const float* values);  // kNumParams values in real units
    int length() const;                   // seq_length, 1 .. 8
    Step home(int index) const;           // step `index` (0 .. 7) as set, before the drift
    Step at(long long count, int root) const;  // what count `count` plays on MIDI note `root`, the drift included
};

}  // namespace undertow
```

```cpp
// core/include/undertow/sequencer.h
#pragma once

#include "undertow/pattern.h"

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
    void setParams(const float* values);   // kNumParams values in real units
    void keyOn(int note);                  // a MIDI key goes down
    void keyOff(int note);                 // a MIDI key comes up
    void allKeysOff();                     // every key comes up
    // One sample of the clock, after that sample's MIDI: `beat` is its position in quarter notes, `running` whether
    // it moves and `bpm` its tempo. Returns true when a step starts at this sample, and fills `out`.
    bool tick(double beat, bool running, double bpm, SeqStep& out);
    int step() const;                      // the step playing (0 .. 7), or -1 while the pattern does not play
    long long count() const;               // the count of the step playing, 0 while none plays
    int root() const;                      // the root of the step playing (a MIDI note), -1 while none plays
};

}  // namespace undertow
```

```cpp
// core/include/undertow/engine.h: two fields join Meters, after step
    long long count = 0;      // phase 4: that step's count from beat 0 (what Pattern::at takes), 0 when idle
    int root = -1;            // phase 4: that step's root, a MIDI note, -1 when idle
```

## Acceptance checks

Each check is one command, run from the repository root; the checks that share a command are one verify step. The
three test files build with the `core` preset.

| Check | Command | Proves |
|---|---|---|
| P4.01 | `tools/spec.sh 4 pattern` | At drift 0 every count plays its home step, count mod length, for every length and on both sides of 0 |
| P4.02 | `tools/spec.sh 4 pattern` | The drift follows the recipe above exactly, for every pattern, length, drift, root and count |
| P4.03 | `tools/spec.sh 4 pattern` | Each arc of 16 passes leaves home and comes back one step at a time, never more than round(3 x drift) steps away; at the default two steps through its middle |
| P4.04 | `tools/spec.sh 4 pattern` | A changed step goes up an octave, up a fifth or down a fourth within -12 to +12, or plays as a ghost; rests stay rests; no move takes the sub under E1 (41.2 Hz); every change occurs, and arcs differ |
| P4.05 | `tools/spec.sh 4 pattern` | The pattern is a pure function of its parameters and the count, and never allocates |
| P4.06 | `tools/spec.sh 4 sequencer` | With the clock running each step starts within one sample of its beat, at all 8 rates, from 60 to 200 BPM, from any position, once each, through host positions that jitter |
| P4.07 | `tools/spec.sh 4 sequencer` | Free plays whenever the clock runs, rooted on seq_root, then on the last key pressed; notes stay within 0 to 127; the drift's guard reads the root played |
| P4.08 | `tools/spec.sh 4 sequencer` | Key plays only while a key is held, rooted on the latest key held; a key from silence sounds its step at once; with every key up it stops |
| P4.09 | `tools/spec.sh 4 sequencer` | A loop plays its steps again exactly, drift included, even a loop one step long; a jump into another step sounds it at once |
| P4.10 | `tools/spec.sh 4 sequencer` | With the host stopped a held key plays from step 1, counting from the key at the host's tempo; when the host plays its grid takes over at once |
| P4.11 | `tools/spec.sh 4 sequencer` | step(), count() and root() show the step playing, -1, 0 and -1 when nothing plays; Notes mode plays nothing; reset() forgets the keys; no allocation |
| P4.12 | `tools/spec.sh 4 engine` | Key mode in the engine: a held key plays the pattern on the host's grid, each hit within one sample, and meters().count and root follow it; nothing without a key |
| P4.13 | `tools/spec.sh 4 engine` | Free mode plays on seq_root without a key; without a host the engine's own clock at seq_bpm drives it; a stopped host auditions a held key |
| P4.14 | `tools/spec.sh 4 engine` | Each hit takes its root and pitch when it starts and holds them; a rest lets the note ring; Notes mode is unchanged |
| P4.15 | `tools/spec.sh 4 engine` | All notes off and all sound off let every key go; reset() too |
| P4.16 | `tools/spec.sh 4 engine` | The default patch's own pattern, drift included (8 bars at 130 BPM on G2): at least 90% of its power below 150 Hz, 98.5% below 300, 88% from 30 to 150, at most 1% below 30, the centroid at most 90 Hz, the bands above 500 Hz, 1 kHz and 2 kHz at least 25, 40 and 55 dB under, 150 to 500 Hz at 1 to 8%, the brightest frames at most 150 Hz |
| P4.17 | `tools/spec.sh 4 engine` | A looped bar replays exactly in the engine: the same steps and notes on every pass, drift included |
| P4.18 | `tools/spec.sh 4 engine` | Under random transports, keys and settings the engine stays finite, within the clipper, allocation-free and inside its CPU budget, then ends in exact silence |
| P4.19 | `tools/check.sh --plugin` | The whole local check still passes: phases 1 to 4, both presets, pluginval |

## Rules every task keeps

- The rules of phases 1 to 3 still hold.
- Phase 3's classes are used as they are: `voice.h` and `decay.h` do not change, nor do phase 2's.
- In `Pattern::setParams`, `Pattern::at` and every `Sequencer` call: no allocation, lock or I/O.

## Also required, not checked automatically

- Listening: the default patch under a held G2 rolls the same line bar after bar, with one or two steps wandering
  through the middle of every 8 bars and coming home, never a jump in tempo or a doubled hit.

## Notes for the planner

- This spec is `docs/specs/phase-4-sequencer.md`. Put it first among the files every task reads: its "What to build"
  holds the drift's recipe and the clock's rules the tests check exactly.
- Three tasks, in this order. Each task's verify step is exactly its check command, unchanged:
  1. **The pattern and its drift.** Integrative: about 80 lines.
     - Creates: `core/include/undertow/pattern.h`, `core/src/pattern.cpp`.
     - Its files are those 2 paths; it edits and deletes nothing.
     - Checks: P4.01 to P4.05, one step.
  2. **The sequencer.** Needs task 1. Integrative: about 140 lines.
     - Creates: `core/include/undertow/sequencer.h`, `core/src/sequencer.cpp`.
     - Its files are those 2 paths; it edits and deletes nothing.
     - Checks: P4.06 to P4.11, one step.
  3. **The engine plays the pattern.** Needs task 2. Focused: about 30 lines.
     - Edits: `core/include/undertow/engine.h`, `core/src/engine.cpp`.
     - Its files are those 2 paths; it creates and deletes nothing.
     - Checks: P4.12 to P4.18, one step.
- `tools/check.sh --plugin` (P4.19) is the plan's final check, not a task's.
- Every task builds C++: its agent needs `bash` and `edit_file`, to run `cmake` and its own `tools/spec.sh` check
  before it hands off. Leave `allowed_tools` out, or include `bash`, `edit_file`, `read_file`, `write_file`, `grep`
  and `glob`.
