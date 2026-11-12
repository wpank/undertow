# Phase 2: the sources and the ladder

**Goal**: the voice's building blocks, as plain classes: two oscillators clear of aliases, each a triangle or a square,
with a sine sub locked to the first; white noise; and the ladder low-pass, with its resonance and drive. Phase 3
wires them into the voice.

**Depends on**: phase 1.

**Size**: M, 2 tasks in parallel, about 260 lines.

---

## Scope

In scope: `core/include/undertow/vco.h`, `noise.h` and `ladder.h`, with their `core/src/*.cpp`. They are new files:
the core's source glob builds them without a change to `core/CMakeLists.txt`.

Out of scope: the voice, the decays and the engine (phase 3). No existing file changes this phase.

## Context

- The PRD (`docs/PRD.md`) sections 1.2 (the voice) and 1.3 (the ladder) are part of this spec.
- The spec tests: `tests/spec/p2/vco_test.cpp` (the oscillators, the sub, the noise) and `ladder_test.cpp`, with
  helpers in `tests/spec/support/`. Change the code, never the tests.
- How phase 3 will use these classes, so their contracts make sense:
  - VCO 1 plays the note and gives its triangle to VCO 2 as the frequency modulator: each sample, VCO 2's frequency is
    `f2 * (1 + vco_fm * vco1.triangle())`.
  - The sub is `vco1.sub(0)` or `vco1.sub(-1)`, which is why it lives in the oscillator: it must stay locked to it.
  - The mixer sums VCO 1, VCO 2, the noise and the sub into the ladder, whose cutoff moves every sample.

## What to build

1. **The oscillator** (`core/include/undertow/vco.h`, `core/src/vco.cpp`)
   - A phase in [0, 1). `process()` returns the wave at the current phase, then advances the phase by
     `dt = frequency / sampleRate`, wrapping it back into [0, 1).
   - Both waves are aligned with `sin(2 pi phase)`:
     - the triangle is `4 p` on [0, 0.25), `2 - 4 p` on [0.25, 0.75) and `4 p - 4` on [0.75, 1): 0 at phase 0, +1
       at 0.25, -1 at 0.75;
     - the square is +1 on [0, 0.5) and -1 on [0.5, 1).
   - Band-limiting, with each sample's own `dt`, so it holds while the frequency moves:
     - the square gets a two-sample PolyBLEP at its rising edge (phase 0) and its falling edge (phase 0.5):
       `square + blep(p) - blep(wrap(p + 0.5))`, where `blep(t)` is `2x - x^2 - 1` with `x = t / dt` for `t < dt`,
       `x^2 + 2x + 1` with `x = (t - 1) / dt` for `t > 1 - dt`, and 0 otherwise;
     - the triangle needs no correction: its harmonics fall 12 dB per octave, so at these pitches its aliases already
       stay far under the checks (76 dB under the fundamental at C5).
   - `setFrequency` clamps a negative frequency to 0 Hz. At or above half the sample rate the wave is 0: it would only
     alias. The phase still advances.
   - `triangle()` is the plain triangle (no band-limiting) at the phase the last `process()` played, whatever the wave:
     the FM modulator.
   - **The sub**, locked to the phase:
     - `sub(0)` is `sin(2 pi p)` at the phase the last `process()` played;
     - `sub(-1)` is `sin(pi (c + p))`, where `c` is 0 or 1, flipping each time the phase wraps, so the sine runs at
       half the frequency and crosses zero rising at every other start of the oscillator's cycle.
     - Compute both from the oscillator's own phase every sample, in double precision. Never run a second phase for
       the sub: a free-running one drifts away from the oscillator, and the two stop adding the same way.
   - `reset(phase)` sets the phase (wrapped into [0, 1)) and puts `c` back to 0.

2. **The noise** (`core/include/undertow/noise.h`, `core/src/noise.cpp`)
   - White noise with an RMS of 0.5 (for example a 32-bit xorshift scaled to a uniform `+-sqrt(3) / 2`), within 1 in
     peak, and the same sequence after every `reset()` (a fixed seed).

3. **The ladder** (`core/include/undertow/ladder.h`, `core/src/ladder.cpp`)
   - Four one-pole stages in a feedback loop, each saturating, solved every sample without a unit delay (zero-delay
     feedback). The simple way, after Teemu Voipio's "cheap non-linear zero-delay filters": linearise each stage's
     `tanh` at its state, `tanh(v) = v * T(s)`, with `T(x) = tanh(x) / x`, and solve the linear loop in closed form.
     Per sample, with `f = tan(pi cutoff / sampleRate)`, states `s0..s3`, and the resonance `k`:
     - the input `u = x * gain * (1 + k)`, and `h = (u + previous u) / 2`;
     - `t0 = T(h - k s3)`, `t1 = T(s0)`, `t2 = T(s1)`, `t3 = T(s2)`, `t4 = T(s3)`;
     - `gi = 1 / (1 + f t(i+1))` for i = 0 .. 3;
     - `f3 = f t3 g3`, `f2 = f t2 g2 f3`, `f1 = f t1 g1 f2`, `f0 = f t0 g0 f1`;
     - the output `y3 = (g3 s3 + f3 g2 s2 + f2 g1 s1 + f1 g0 s0 + f0 u) / (1 + k f0)`;
     - `xx = t0 (u - k y3)`, `y0 = t1 g0 (s0 + f xx)`, `y1 = t2 g1 (s1 + f y0)`, `y2 = t3 g2 (s2 + f y1)`;
     - the states: `s0 += 2f (xx - y0)`, `s1 += 2f (y0 - y1)`, `s2 += 2f (y1 - y2)`, `s3 += 2f (y2 - t4 y3)`;
     - `process` returns `y3 / gain`.

     `T(x)` may be Lambert's continued fraction, `((a + 105) a + 945) / ((15 a + 420) a + 945)` with `a = x^2`. A
     different ladder is fine if it meets the checks.
   - **Resonance**: `k = 2.5 r` for `r` in 0 .. 1 (self-oscillation would start at `k = 4`). Scaling the input by
     `1 + k` keeps the passband at 0 dB at every resonance.
   - **Drive**: `gain = 0.5 * 6^d` for `d` in 0 .. 1, into the stages, and divided back out of the output.
   - `setCutoff` clamps to 20 Hz .. 0.45 sampleRate and may be called every sample; the stages carry on from where they
     are, so the output stays smooth.
   - Flush the state to exact zero, all four stages and the input history together, once they are all below about
     1e-20. Zeroing one of the coupled stages alone can keep the loop ringing near 1e-19 for ever.

4. **Realtime**: no allocation, lock or I/O anywhere in these classes after `prepare`.

## API

The headers the tasks write. Names, types and signatures are what the spec tests compile against; the private parts
are free.

```cpp
// core/include/undertow/vco.h
#pragma once

namespace undertow {

enum class Wave { Triangle = 0, Square = 1 };  // the vco1_wave and vco2_wave choices

class Vco {
public:
    void prepare(double sampleRate);
    void reset(double phase = 0.0);  // the next process() plays this phase; the sub's octave count starts even
    void setWave(Wave wave);
    void setFrequency(double hz);    // clamped at 0; at or above sampleRate / 2 the wave is silent
    float process();                 // the wave at the current phase (the square band-limited); then the phase advances
    float triangle() const;          // the triangle at the phase the last process() played, whatever the wave
    float sub(int octaves) const;    // the sine at that phase: octaves 0 (the VCO's pitch) or -1 (an octave under)
    double phase() const;            // the phase the next process() plays
};

}  // namespace undertow
```

```cpp
// core/include/undertow/noise.h
#pragma once

namespace undertow {

class Noise {
public:
    void reset();     // the same sequence from every reset
    float process();  // white, RMS 0.5, within 1
};

}  // namespace undertow
```

```cpp
// core/include/undertow/ladder.h
#pragma once

namespace undertow {

class Ladder {
public:
    void prepare(double sampleRate);
    void reset();                    // silence
    void setCutoff(double hz);       // clamped to 20 Hz .. 0.45 sampleRate; may change every sample
    void setResonance(double r);     // 0 .. 1: feedback k = 2.5 r, with the input scaled by 1 + k
    void setDrive(double d);         // 0 .. 1: gain 0.5 * 6^d into the stages, divided back out after them
    float process(float x);
};

}  // namespace undertow
```

## Acceptance checks

Each check is one command, run from the repository root; the checks that share a command are one verify step. Both
test files build with the `core` preset in seconds.

| Check | Command | Proves |
|---|---|---|
| P2.01 | `tools/spec.sh 2 vco` | Each wave plays the frequency it is given within 0.1%, from 49 Hz to 1234.5 Hz |
| P2.02 | `tools/spec.sh 2 vco` | The waves: full scale, a triangle's and a square's odd harmonics, no even ones, aligned with sin(2 pi phase) |
| P2.03 | `tools/spec.sh 2 vco` | Clear of aliases: at C5 no component below 5 kHz but the harmonics within 60 dB of the fundamental, and at 1234.5 Hz the power between the harmonics below 4 kHz 50 dB under |
| P2.04 | `tools/spec.sh 2 vco` | reset puts the phase where it is told, wrapped, the same way every time, the sub's octave count included |
| P2.05 | `tools/spec.sh 2 vco` | Per-sample frequency changes under FM keep the mean frequency and the phase never runs backwards; triangle() is the modulator whatever the wave |
| P2.06 | `tools/spec.sh 2 vco` | The sub is a pure full-scale sine at the oscillator's pitch or an octave under, locked to its phase through every frequency change |
| P2.07 | `tools/spec.sh 2 vco` | At or above half the sample rate the wave is silent |
| P2.08 | `tools/spec.sh 2 vco` | The noise is white (flat within 2 dB from 100 Hz to 12.8 kHz), RMS 0.5, within 1, and the same after every reset |
| P2.09 | `tools/spec.sh 2 vco` | The oscillators and the noise never allocate, and run at least 40 times faster than real time |
| P2.10 | `tools/spec.sh 2 ladder` | At resonance 0 the ladder is a 24 dB/octave low-pass: 0 dB in the passband, -12 dB at the cutoff, -28 an octave up, -49 two octaves up, 23 to 25 dB per octave beyond |
| P2.11 | `tools/spec.sh 2 ladder` | Resonance never thins the bass: below a quarter of the cutoff the gain stays within 1 dB at every resonance |
| P2.12 | `tools/spec.sh 2 ladder` | Resonance raises a peak just under the cutoff, rising with the knob: +11 dB (within 1 dB) at the top, 4 to 6 dB at 0.6 |
| P2.13 | `tools/spec.sh 2 ladder` | It never self-oscillates: at the top of the knob an impulse's response stays 60 dB under its peak after 12 cycles of the cutoff, and silence ends in exact zeros |
| P2.14 | `tools/spec.sh 2 ladder` | Drive warms, never fuzzes: a -6 dBFS 55 Hz sine at a 220 Hz cutoff has its harmonics 38 dB under at drive 0 and 12 to 30 dB under at the top, its level within 4 dB |
| P2.15 | `tools/spec.sh 2 ladder` | The cutoff may move every sample: a filter decay sweeping 2 kHz to 100 Hz at full resonance stays smooth and bounded |
| P2.16 | `tools/spec.sh 2 ladder` | Nothing a host sets blows it up: random cutoffs, resonances and drives on a loud input stay finite and bounded |
| P2.17 | `tools/spec.sh 2 ladder` | The ladder never allocates, and runs at least 30 times faster than real time with its cutoff moving every sample |
| P2.18 | `tools/check.sh --plugin` | The whole local check still passes: phase 1's tests and phase 2's, both presets, pluginval |

## Rules every task keeps

- The rules of phase 1 still hold.
- No existing file changes: the three classes are new files, picked up by the core's source glob.
- `core/` stays plain C++17.

## Also required, not checked automatically

- The code reads like the rest of the core: small files, plain names, comments only where the why is not obvious.

## Notes for the planner

- This spec is `docs/specs/phase-2-sources.md`. Put it first among the files every task reads: its "What to build"
  holds the formulas the tests measure against (the waves' alignment, the square's PolyBLEP, the sub's lock, the
  ladder's per-sample solution).
- Two tasks, which can run in parallel: they write different files and their checks build different executables.
  1. **The oscillators and the noise.** Integrative: about 150 lines.
     - Creates: `core/include/undertow/vco.h`, `core/src/vco.cpp`, `core/include/undertow/noise.h`,
       `core/src/noise.cpp`.
     - Its files are those 4 paths; it edits and deletes nothing.
     - Checks: P2.01 to P2.09, one step.
  2. **The ladder.** Integrative: about 110 lines.
     - Creates: `core/include/undertow/ladder.h`, `core/src/ladder.cpp`.
     - Its files are those 2 paths; it edits and deletes nothing.
     - Checks: P2.10 to P2.17, one step.
- `tools/check.sh --plugin` (P2.18) is the plan's final check, not a task's.
- Every task builds C++: its agent needs `bash` and `edit_file`, to run `cmake` and its own `tools/spec.sh` check
  before it hands off. Leave `allowed_tools` out, or include `bash`, `edit_file`, `read_file`, `write_file`, `grep`
  and `glob`.
