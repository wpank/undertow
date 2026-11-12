# Phase 3: the voice

**Goal**: Undertow 2 makes its sound. An attack-decay envelope; the voice, which mixes the two oscillators, the noise
and the sine sub into the drive and the ladder, then the VCA, with three decays shaping every hit; and the engine
playing it from MIDI notes (the sequence mode `Notes`). The default patch, played as its pattern would be, meets the
PRD's range and darkness.

**Depends on**: phase 2.

**Size**: M, 3 tasks in order, about 330 lines.

---

## Scope

In scope: `core/include/undertow/decay.h`, `core/src/decay.cpp`, `core/include/undertow/voice.h` and
`core/src/voice.cpp` (new files; `voice.h` and `voice.cpp` are new to Undertow 2), and the voice wired into
`core/src/engine.cpp`.

Out of scope: the sequencer and the `Key` and `Free` modes (phase 4: until then a MIDI note in those modes plays
nothing), the presets and the darker rumble (phase 5), the window (phase 6).

## Context

- The PRD (`docs/PRD.md`) sections 1.1 (the range), 1.2 (the voice), 1.4 (the decays and the VCA) and 1.8 (level
  and safety) are part of this spec.
- The spec tests: `tests/spec/p3/decay_test.cpp`, `voice_test.cpp` and `engine_test.cpp`, with helpers in
  `tests/spec/support/` (`spec_sound.h` has the PRD's sound measures: band shares, the centroid, the frames'
  brightness, the 20 ms envelope and the highs around each hit). Change the code, never the tests.
- The classes of phase 2 (`undertow::Vco`, `Noise`, `Ladder`) are the voice's parts; read their spec,
  `docs/specs/phase-2-sources.md`, for their contracts.

## What to build

1. **The envelope** (`core/include/undertow/decay.h`, `core/src/decay.cpp`)
   - `trigger(peak)` starts a hit from wherever the level is: it rises to `peak` along half a cosine,
     `level = start + (peak - start) * (1 - cos(pi t / T)) / 2` for `t` from 0 to `T`, then falls exponentially,
     `level *= c` each sample with `c = 0.01^(1 / (decayMs * sampleRate / 1000))`, so it is at 1% of the peak (-40 dB)
     after `decayMs`.
   - The attack time is the rise's 10-to-90% time. Half a cosine covers 10% to 90% of the way in 0.5903 of its length,
     so `T = attackMs / 0.5903`. **Half a cosine, never a straight line**: the VCA comes after the ladder, so the
     corners of a straight rise would splash highs past it, and click.
   - A hit softer than the current level glides down to it over `max(T, 4 ms)`, so the level never steps.
   - An attack of 0 jumps to the peak at once (the pitch decay).
   - Under 1e-5 (-100 dB) the level is exactly 0 and the envelope is idle. `reset()` silences it at once.

2. **The voice** (`core/include/undertow/voice.h`, `core/src/voice.cpp`)
   - The parts: VCO 1, VCO 2, the noise, the ladder (phase 2's classes), and three envelopes: pitch, filter and
     level.
   - **A hit** `hit(note, velocity)`, with velocity 0 .. 1:
     - velocity 0 is a rest: nothing happens, and a ringing note carries on;
     - if the level envelope is under 0.1 (-20 dB), restart VCO 1 and VCO 2 at phase 0 (`reset()`), so a pattern's
       hits meet the kick the same way on every repeat; into a louder note they keep running;
     - trigger the pitch envelope at 1, the filter envelope at `velocity` and the level envelope at `velocity`.
   - **Each sample**, in this order:
     - the thud: VCO 1's frequency is `440 * 2^((note - 69 + pitch_env * pitch) / 12)`, where `pitch` is the pitch
       envelope (attack 0, decay `pitch_decay`);
     - VCO 1 plays; the sub is `vco1.sub(0)` or `vco1.sub(-1)` (`sub_octave` 0 or 1), so it follows the thud too;
     - VCO 2's frequency is VCO 1's times `2^(vco2_tune / 12) * (1 + vco_fm * vco1.triangle())`, and VCO 2 plays;
     - the mixer: `vco1_level * vco1 + vco2_level * vco2 + noise_level * noise + sub_level * sub`;
     - the ladder: its cutoff is `vcf_cutoff * 2^(vcf_env * filter)`, where `filter` is the filter envelope (decay
       `vcf_decay`), with `vcf_res` and `vcf_drive`;
     - the VCA: the ladder's output times the level envelope (decay `vca_decay`).
   - **Attack** (`vca_attack`): Fast gives both the level and the filter envelope a 1 ms attack; Slow gives the level
     100 ms and the filter 500 ms (10 to 90%), so a held note breathes rather than blooms. Phase 7 softens Fast: the
     level rises in 1.2 ms and the filter in 0.6 ms (`docs/specs/phase-7-ship.md`).
   - **Smoothing**: the four mixer levels, `vco2_tune`, `vco_fm`, the cutoff (in octaves, `log2` of it), the
     resonance and the drive each glide towards their parameter with a 5 ms one-pole, so a knob moved during a note
     never clicks. The first `setParams` after `reset()` sets them at once.
   - **Idle**: once the level envelope is idle the voice outputs exact zeros, `note()` is -1 and `noteHz()` 0; reset
     the ladder then, so the next phrase starts from the same state every time.
   - The accessors `vco1Hz()`, `cutoffHz()` and `level()` report the values the last sample used: the tests read the
     envelopes through them.

3. **The engine plays the voice** (`core/src/engine.cpp`)
   - In `prepare` and `reset`, prepare and reset the voice and give it the parameters; in `process`, give it the
     parameters at each block.
   - MIDI, at each event's own sample:
     - in Notes mode (`seq_mode` 2), a note-on with velocity above 0, on any channel, is `voice.hit(note,
       velocity / 127)`; a note-off, or a note-on at velocity 0, changes nothing;
     - in the Key and Free modes notes play nothing yet (phase 4);
     - controller 120 (all sound off), in any mode: reset the voice, the rumble and the output stage, so the output
       is silent at once.
   - Per sample: the voice's sample, plus the rumble fed `voice * rumble_send`, into the output stage, to both
     channels and the scope.
   - `meters().note` and `meters().noteHz` are the voice's `note()` and `noteHz()` at the end of the block.

## API

The headers the tasks write. Names, types and signatures are what the spec tests compile against; the private parts
are free.

```cpp
// core/include/undertow/decay.h
#pragma once

namespace undertow {

class Decay {
public:
    void prepare(double sampleRate);
    void reset();                  // idle at 0
    void setAttack(double ms);     // the rise's 10-90% time; 0 jumps to the peak at once
    void setDecay(double ms);      // the time to fall to 1% of the peak
    void trigger(float peak);      // start a hit, from wherever the level is
    float process();               // advance one sample, return the level
    float level() const;
    bool active() const;           // not yet fallen under -100 dB
};

}  // namespace undertow
```

```cpp
// core/include/undertow/voice.h
#pragma once

#include "undertow/decay.h"
#include "undertow/ladder.h"
#include "undertow/noise.h"
#include "undertow/vco.h"

namespace undertow {

class Voice {
public:
    void prepare(double sampleRate);
    void reset();                          // silence; every envelope idle
    void setParams(const float* values);   // kNumParams values in real units
    void hit(int note, float velocity);    // one hit: VCO 1 at MIDI note `note`, velocity 0 .. 1; 0 is a rest
    float process();                       // one sample
    bool active() const;                   // the level envelope has not fallen under -100 dB
    int note() const;                      // the last hit's note while active, else -1
    double noteHz() const;                 // that note's pitch without the thud, 0 when idle
    double vco1Hz() const;                 // VCO 1's frequency at the last sample, the thud included
    double cutoffHz() const;               // the ladder's cutoff at the last sample, the filter envelope included
    float level() const;                   // the VCA's gain at the last sample
};

}  // namespace undertow
```

`engine.h` does not change.

## Acceptance checks

Each check is one command, run from the repository root; the checks that share a command are one verify step. The
three test files build with the `core` preset.

| Check | Command | Proves |
|---|---|---|
| P3.01 | `tools/spec.sh 3 decay` | A hit rises along half a cosine, 10 to 90% of the way in the attack time (1 ms, 100 ms, 500 ms), flat at both ends |
| P3.02 | `tools/spec.sh 3 decay` | The decay is exponential and falls 40 dB in the decay time, within 5% (30 ms, 300 ms, 3 s) |
| P3.03 | `tools/spec.sh 3 decay` | A hit rises from wherever the level is, never stepping it; a softer hit glides down over at least 4 ms |
| P3.04 | `tools/spec.sh 3 decay` | Under -100 dB the level is exact zero and the envelope idle; reset silences it at once |
| P3.05 | `tools/spec.sh 3 decay` | With no attack a hit jumps to its peak at once; the envelope never allocates |
| P3.06 | `tools/spec.sh 3 voice` | A hit plays VCO 1 at its note, VCO 2 at vco2_tune semitones, the sub at 0 or -1 oct, the noise, and VCO 1's FM of VCO 2 with its mean frequency kept |
| P3.07 | `tools/spec.sh 3 voice` | The thud: each hit starts pitch_env semitones high (within 0.05) and falls to 1% of that in pitch_decay (within 5%) |
| P3.08 | `tools/spec.sh 3 voice` | The filter: the cutoff rises vcf_env x velocity octaves and falls back in vcf_decay; resonance and drive reach the ladder |
| P3.09 | `tools/spec.sh 3 voice` | The attack: Fast 1 ms for the level and the filter (1.2 ms and 0.6 ms from phase 7); Slow 100 ms for the level and 500 ms for the filter (10 to 90%) |
| P3.10 | `tools/spec.sh 3 voice` | Velocity sets the peak level and darkens a softer hit; velocity 0 is a rest |
| P3.11 | `tools/spec.sh 3 voice` | A hit under -20 dB restarts the oscillators, so a repeated phrase plays the same every time; into a louder note they run on |
| P3.12 | `tools/spec.sh 3 voice` | A knob moved during a note never clicks: the levels and the cutoff glide |
| P3.13 | `tools/spec.sh 3 voice` | The default patch's rolling line: at least 90% of its power below 150 Hz, 98.5% below 300, 88% from 30 to 150, at most 1% below 30 (2%, the PRD's, from phase 7), the centroid at most 90 Hz, the bands above 500 Hz, 1 kHz and 2 kHz at least 25, 40 and 55 dB under, 150 to 500 Hz at 1 to 8%, the brightest frames at most 150 Hz |
| P3.14 | `tools/spec.sh 3 voice` | Short decays roll and long ones hold: 115 ms after its peak the default note is 10 dB down or more, a held one 3 dB down at most |
| P3.15 | `tools/spec.sh 3 engine` | In Notes mode each MIDI note-on is one hit at its pitch and velocity, from its own sample; a note-off changes nothing; all sound off silences at once |
| P3.16 | `tools/spec.sh 3 engine` | The meters show the note and its pitch while it sounds, and -1 and 0 once it has died away |
| P3.17 | `tools/spec.sh 3 engine` | The voice feeds the rumble: rumble_send gives the bass its own tail |
| P3.18 | `tools/spec.sh 3 engine` | The default patch at the output: peaks between -12 and -6 dBFS, the highs at every hit under -60 dBFS, no DC, mono |
| P3.19 | `tools/spec.sh 3 engine` | Under random notes and settings the engine stays finite, within the clipper, allocation-free and inside its CPU budget, then ends in exact silence |
| P3.20 | `tools/check.sh --plugin` | The whole local check still passes: phases 1 to 3, both presets, pluginval |

## Rules every task keeps

- The rules of phases 1 and 2 still hold.
- Phase 2's classes are used as they are: `vco.h`, `noise.h` and `ladder.h` do not change.
- In `Voice::process`, `hit` and `setParams`: no allocation, lock or I/O.

## Also required, not checked automatically

- Listening: the default patch played as a 1/16 line on G2 sounds like a dark, rolling sub with a short thud at each
  hit, never a squelch.

## Notes for the planner

- This spec is `docs/specs/phase-3-voice.md`. Put it first among the files every task reads: its "What to build"
  holds the order of the voice's sample and the formulas the tests measure against (the half-cosine rise, the
  restart rule, the thud, the filter's swing, the attack times).
- Three tasks, in this order. Each task's verify step is exactly its check command, unchanged:
  1. **The envelope.** Integrative: about 80 lines.
     - Creates: `core/include/undertow/decay.h`, `core/src/decay.cpp`.
     - Its files are those 2 paths; it edits and deletes nothing.
     - Checks: P3.01 to P3.05, one step.
  2. **The voice.** Needs task 1. Architectural: about 200 lines.
     - Creates: `core/include/undertow/voice.h`, `core/src/voice.cpp`.
     - Its files are those 2 paths; it edits and deletes nothing.
     - Checks: P3.06 to P3.14, one step.
  3. **The engine plays the voice.** Needs task 2. Integrative: about 50 lines.
     - Edits: `core/src/engine.cpp`.
     - Its files are that 1 path; it creates and deletes nothing.
     - Checks: P3.15 to P3.19, one step.
- `tools/check.sh --plugin` (P3.20) is the plan's final check, not a task's.
- Every task builds C++: its agent needs `bash` and `edit_file`, to run `cmake` and its own `tools/spec.sh` check
  before it hands off. Leave `allowed_tools` out, or include `bash`, `edit_file`, `read_file`, `write_file`, `grep`
  and `glob`.
