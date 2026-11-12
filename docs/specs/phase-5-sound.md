# Phase 5: the sound

**Goal**: Undertow 2 sounds finished. The rumble becomes the dark room under the kick, breathing with the beat and
moving from beat to beat, and the output stage cuts what lies under the sub; the engine tells which hits it played,
and the renderer writes them beside each render; and the factory presets, Roll, Hold, Drift, Rumble and Pressure
among them, each meet the PRD's sound goals.

**Depends on**: phase 4.

**Size**: M, 3 tasks: the rumble and the hits in parallel, then the presets; about 500 lines.

---

## Scope

In scope: the rumble, rewritten (`core/include/undertow/rumble.h`, `core/src/rumble.cpp`); the output stage's
sub-sonic high-pass (`core/include/undertow/output_stage.h`, `core/src/output_stage.cpp`); the hits the engine
reports (`core/include/undertow/engine.h`, `core/src/engine.cpp`); the renderer's hits file, preset list and root
(`tools/render/main.cpp`, `tools/render-demos.sh`); the factory presets (`core/src/presets.cpp`).

Out of scope: the window (phase 6); strict pluginval, auval and the install (phase 7).

## Context

- The PRD (`docs/PRD.md`) sections 1.1 (the range), 1.4 (roll and hold), 1.6 (the rumble), 1.7 (never acid), 1.8
  (level and safety), 1.9 (under the kick) and 1.10 (the presets) are part of this spec.
- The spec tests: `tests/spec/p5/rumble_test.cpp`, `render_test.cpp` and `presets_test.cpp`, with helpers in
  `tests/spec/support/` (`spec_sound.h` adds integrated loudness, the true peak, the repeat spread, a plain kick, the
  valleys its tail digs and the drop before the next hit). Change the code, never the tests.
- The rumble replaces Undertow 1's: its public API stays as it is, so `engine.cpp` calls it as before, and phase 1's
  rumble checks (P1.09) still hold.

## What to build

1. **The rumble and the output's high-pass** (`core/include/undertow/rumble.h`, `core/src/rumble.cpp`,
   `core/include/undertow/output_stage.h`, `core/src/output_stage.cpp`)
   - **The ghost kick and the grid** stay as they are: a sine falling from 150 to 45 Hz (`45 + 105 e^(-t / 30 ms)`),
     decaying with an 80 ms time constant, 300 ms long, scaled by `rumble_kick`, fired at every point of the
     `rumble_rate` grid (1/4, 1/2, 1 bar or 1/8) the running clock crosses or lands on (after a jump back, and on the
     first sample the clock runs), never while it is stopped.
   - **The reverb**, fed the ghost kick plus `send` (the voice times `rumble_send`), at 48 kHz (scale every length
     with the sample rate):
     - a 12 ms pre-delay;
     - four allpasses in series (`w = x + g * w[n - D]`, `y = w[n - D] - g * w`), of 142, 107, 379 and 277 samples,
       with g = 0.7, 0.7, 0.65 and 0.65;
     - an eight-line feedback network: delays of 2903, 3511, 4229, 4801, 5521, 6091, 6763 and 7417 samples, each
       drifting by up to 40 samples (`D + 40 sin(phase)`, read with linear interpolation) at 0.11, 0.13, 0.17, 0.19,
       0.23, 0.29, 0.31 and 0.37 Hz, from phases `2 pi i / 8`; each line's output scaled by `10^(-3 D /
       (sampleRate * rumble_decay))`, so the tail falls 60 dB in `rumble_decay`; the eight outputs mixed back into
       every line through an 8 x 8 Hadamard matrix scaled by `1 / sqrt(8)`, with the diffused input added to each;
     - the output: `(line 0 - line 3 + line 4 - line 7) / 8`.
   - **After the reverb**, in this order: a second-order Butterworth high-pass at 30 Hz; the drive, `tanh(g * 6 x) /
     tanh(g)` with `g = 0.5 + 3.4 * rumble_drive`; a fourth-order Butterworth low-pass at `rumble_tone` (two biquads,
     Q 0.5412 and 1.3066). **The low-pass comes after the drive**, so the drive's harmonics never reach the room.
   - **The duck**, at every grid point while the clock runs, whatever `rumble_kick` is: its envelope rises from where
     it is to 1 along half a cosine over 8 ms, then falls back as `e^(-(t - 8 ms) / 150 ms)`; the gain is `1 -
     rumble_duck * envelope`, and `duckGain()` reports it.
   - **The output**: the low-passed signal times the duck's gain, `rumble_level` and 0.67, so the default rumble sits
     under the roll without filling it; exact zero when `rumble_level` is 0.
   - Size every line in `prepare()`; flush the network's state to exact zero when it is negligible (under 1e-20).
     In `process()` and `setParams()`: no allocation, lock or I/O. Recompute the filters' and the lines' gains only
     when `rumble_tone` or `rumble_decay` changes.
   - **The output stage's sub-sonic high-pass**: after the DC blocker, a fourth-order Butterworth high-pass at 20 Hz
     (two biquads, Q 0.5412 and 1.3066), its coefficients set in `prepare()` and its state flushed to exact zero when
     negligible, so silence in is still silence out. It takes out the low rumble each restarted hit leaves under the
     sub (the first half-cycle of every hit is the loudest), and keeps 30 Hz within 0.5 dB.

2. **The hits and the renderer** (`core/include/undertow/engine.h`, `core/src/engine.cpp`, `tools/render/main.cpp`,
   `tools/render-demos.sh`)
   - **The engine keeps a list of the hits of each `process()` call**, in order, up to `kMaxHits` (64): every hit the
     sequencer starts with a velocity above 0 (its sample in the block, its step, note and velocity, the drift
     included), and in Notes mode every note-on with velocity above 0 (step -1, velocity `data2 / 127`). Rests are not
     hits. Each `process()` call starts a new list; `hits(destination, count)` copies at most `count` of them and
     returns how many it copied.
   - **The renderer**:
     - `--hits <path>` writes every hit of the render to `<path>` as JSON, after the WAV:

       ```json
       {
         "rate": 48000,
         "bpm": 130,
         "bars": 8,
         "root": 43,
         "hits": [
           {"sample": 5539, "step": 2, "note": 43, "pitch": 0, "velocity": 0.22},
           {"sample": 11077, "step": 3, "note": 43, "pitch": 0, "velocity": 1}
         ]
       }
       ```

       `sample` counts from the render's first sample, `step` counts from 1 (0 for a Notes-mode hit), `pitch` is the
       note less the root, and `velocity` is written with up to six significant digits. Writing the hits never changes
       the WAV.
     - Without `--note`, it holds the preset's own root, `seq_root`; `--note` accepts 0 to 127 and refuses anything
       else.
     - `--list` prints every factory preset's slug, one per line, in program order, and exits.
   - **`tools/render-demos.sh`** empties `build/demos` and renders every preset `--list` names, 8 bars at 130 BPM on
     its own root, to `<slug>.wav` with its hits in `<slug>-hits.json`.

3. **The presets** (`core/src/presets.cpp`): eight, in this order. Each is the defaults with the values listed; a
   step's velocities are given for steps 1 to 8. `findFactoryPreset` finds each by name or slug.
   - **Init** (`init`): the defaults.
   - **Roll** (`roll`), the rolling sub with more swing: `vcf_cutoff` 130, `vcf_env` 2.4, `pitch_env` 6,
     `pitch_decay` 60, `vca_decay` 330, `seq_drift` 0.6; velocities 0, 0.35, 0.7, 0.5, 0, 0.9, 0.4, 1.
   - **Hold** (`hold`), Roll held: Roll's values, and `vcf_decay` 1400, `vca_attack` 1 (Slow), `vca_decay` 2800 and
     `master_gain` -18.5.
   - **Drift** (`drift`), a square line that wanders: `vco1_wave` 1, `vco1_level` 0.4, `vco2_wave` 0, `vco2_level`
     0.35, `vco_fm` 0.3, `noise_level` 0.08, `sub_level` 0.85, `pitch_env` 9, `pitch_decay` 32, `vcf_cutoff` 110,
     `vcf_res` 0.44, `vcf_drive` 0.49, `vcf_env` 2.6, `vcf_decay` 184, `vca_decay` 276, `seq_drift` 0.9, `master_gain`
     -9.5; velocities 0, 0.55, 1, 0.6, 0, 0.7, 0.9, 0.5.
   - **Rumble** (`rumble`), a bed led by the rumble: `vco1_level` 0.15, `vco2_level` 0.1, `noise_level` 0, `sub_level`
     0.6, `vcf_cutoff` 90, `vcf_env` 1.2, `vca_decay` 600, `seq_rate` 3 (1/8), `seq_drift` 0.3, `rumble_level` 1,
     `rumble_send` 0.4, `rumble_kick` 0.8, `rumble_decay` 6, `rumble_tone` 90, `rumble_duck` 0.9, `master_gain` -13;
     velocities 0, 0.5, 0, 0.35, 0, 0.6, 0, 0.3.
   - **Pressure** (`pressure`), Drift's square driven harder: Drift's values but `vcf_cutoff` 115, `vcf_res` 0.48,
     `vcf_drive` 0.875, `vcf_decay` 230, `vca_decay` 390, `seq_drift` 0.5 and `master_gain` -7; Drift's velocities.
   - **Offbeat** (`offbeat`), eighths on the off-beats: `pitch_env` 5, `pitch_decay` 50, `vcf_cutoff` 120,
     `vca_decay` 250, `seq_rate` 3 (1/8), `seq_drift` 0.4, `master_gain` -8; velocities 0, 1, 0, 0.8, 0, 0.9, 0, 0.7.
   - **Deep** (`deep`), lower and darker, on E: `vco2_level` 0.2, `vcf_cutoff` 100, `vcf_env` 1.5, `vca_decay` 400,
     `seq_drift` 0.5, `seq_root` 40 (E2), `master_gain` -10; velocities 0, 0.4, 0.7, 1, 0, 0.5, 0.8, 0.3.

## API

`rumble.h`'s public part does not change. `engine.h` adds:

```cpp
// core/include/undertow/engine.h (added)
constexpr int kMaxHits = 64;  // hits kept per process() call; more are dropped

// A hit the voice played: where in the block it started, its step (0 .. 7, or -1 for a MIDI note in Notes mode), its
// note and its velocity, the drift included.
struct Hit {
    int sampleOffset = 0;
    int step = -1;
    int note = 0;
    float velocity = 0.0f;
};

class Engine {
public:
    // ... as before, and:
    // The hits of the last process() call, in order, at most `count` of them; returns how many it copied. Call it on
    // the thread that calls process(), after it.
    int hits(Hit* destination, int count) const;
};
```

## Acceptance checks

Each check is one command, run from the repository root; the checks that share a command are one verify step. The
three test files build with the `core` preset; `render_test.cpp` and `presets_test.cpp` also build the renderer.

| Check | Command | Proves |
|---|---|---|
| P5.01 | `tools/spec.sh 5 rumble` | The rumble alone is dark: at least 97% of its power below 150 Hz, its centroid at most 85 Hz, the band above 500 Hz at least 60 dB under |
| P5.02 | `tools/spec.sh 5 rumble` | It breathes: 12 ms after each kick at least 10 dB under its level before it, back within 3 dB by 400 ms |
| P5.03 | `tools/spec.sh 5 rumble` | It moves: from beat to beat its bed spreads by at least 1 dB, 150 to 450 ms after the kick |
| P5.04 | `tools/spec.sh 5 rumble` | The duck never splatters: no 20 ms frame puts more than -25 dB of its power above 300 Hz |
| P5.05 | `tools/spec.sh 5 rumble` | The duck falls to 1 - rumble_duck along half a cosine in 8 ms on every grid point, the ghost kick on or off, and comes back with a 150 ms time constant; the grid follows rumble_rate and jumps, and stops with the clock |
| P5.06 | `tools/spec.sh 5 rumble` | rumble_decay sets the tail: one ghost kick rings down 60 dB in that time, within 15% |
| P5.07 | `tools/spec.sh 5 rumble` | rumble_tone moves the darkness: the centroid rises with it, even at 250 Hz the band above 500 Hz stays 50 dB under, and the low-pass falls 24 dB per octave |
| P5.08 | `tools/spec.sh 5 rumble` | Under random settings the rumble stays finite and bounded, never allocates and fits its CPU budget; level 0 and reset() are exact silence |
| P5.09 | `tools/spec.sh 5 rumble` | The output stage cuts what lies under the sub: 10 Hz at least 20 dB down, 20 Hz about 3 dB, 30 Hz within 0.5 dB and E1 within 0.2 dB; silence in is silence out |
| P5.10 | `tools/spec.sh 5 rumble` | The default rumble never fills the roll: Init's whole output still falls 8 dB (median) before each next hit |
| P5.11 | `tools/spec.sh 5 render` | The engine reports every hit it plays, at its sample, with its step, note and velocity, rests left out; Notes-mode hits have step -1 |
| P5.12 | `tools/spec.sh 5 render` | The renderer's hits file lists every hit of the render, drift included, and leaves the WAV as it was |
| P5.13 | `tools/spec.sh 5 render` | The renderer holds the preset's own root unless `--note` says otherwise, and refuses a note outside MIDI |
| P5.14 | `tools/spec.sh 5 render` | `--list` prints every preset's slug; `render-demos.sh` renders each, with its hits, into an emptied `build/demos` |
| P5.15 | `tools/spec.sh 5 presets` | The eight presets, in order, at the values above; Roll and Hold differ only in their decays, attack and master level |
| P5.16 | `tools/spec.sh 5 presets` | Never acid: resonance at most 0.6, the thud at most +9 on 1/16 and faster lines, and no rolling preset hits on the kick's beats in 32 bars at drift 1.0 |
| P5.17 | `tools/spec.sh 5 presets` | Every preset's voice, 8 bars on its own root at the output: at least 90% below 150 Hz, 98.5% below 300, 88% from 30 to 150, at most 0.25% below 20 and 2% below 30, the centroid at most 90 Hz, above 500 Hz, 1 kHz and 2 kHz at least 25, 40 and 55 dB under, 150 to 500 Hz at 1 to 8% (a bed led by the rumble may be darker), the brightest frames at most 150 Hz |
| P5.18 | `tools/spec.sh 5 presets` | Rolling presets fall 8 dB (median) before each next hit; a held preset's note falls at most 3 dB 115 ms after its peak |
| P5.19 | `tools/spec.sh 5 presets` | Every preset plays within 2 LU of Init, its true peaks at most -1 dBTP |
| P5.20 | `tools/spec.sh 5 presets` | Under a plain kick: rolling presets repeat within 0.5 dB at drift 0; a held preset's valleys deepen by at most 6 dB under the kick, the worst at most 18 dB |
| P5.21 | `tools/spec.sh 5 presets` | The Rumble preset is a bed led by its rumble (3 dB over its voice alone) and dark: 97% below 150 Hz, its centroid at most 85 Hz |
| P5.22 | `tools/spec.sh 5 presets` | The renderer holds each preset's own root: Deep renders on E2 |
| P5.23 | `tools/check.sh --plugin` | The whole local check still passes: phases 1 to 5, both presets, pluginval |

## Rules every task keeps

- The rules of phases 1 to 4 still hold.
- The rumble keeps its public API, so `engine.cpp` calls it as before; the output stage keeps its API too.
- In the rumble's and the output stage's `process()` and `setParams()`, and in `Engine::hits()`: no allocation, lock or
  I/O.

## Also required, not checked automatically

- Listening: under a plain kick at 130 BPM, the default patch rolls with the rumble breathing under it and never
  filling the gaps; Roll and Hold sound like one line, short and long; Rumble is a dark room that pumps with the beat.

## Notes for the planner

- This spec is `docs/specs/phase-5-sound.md`. Put it first among the files every task reads: its "What to build" holds
  the rumble's design, the hits file's format and the presets' values the tests check exactly.
- Three tasks. Each task's verify step is exactly its check command, unchanged:
  1. **The rumble and the output's high-pass.** Architectural: about 280 lines.
     - Edits: `core/include/undertow/rumble.h`, `core/src/rumble.cpp`, `core/include/undertow/output_stage.h`,
       `core/src/output_stage.cpp`.
     - Its files are those 4 paths; it creates and deletes nothing.
     - Checks: P5.01 to P5.10, one step.
  2. **The hits and the renderer.** In parallel with task 1. Integrative: about 130 lines.
     - Edits: `core/include/undertow/engine.h`, `core/src/engine.cpp`, `tools/render/main.cpp`,
       `tools/render-demos.sh`.
     - Its files are those 4 paths; it creates and deletes nothing.
     - Checks: P5.11 to P5.14, one step.
  3. **The presets.** Needs tasks 1 and 2. Focused: about 90 lines.
     - Edits: `core/src/presets.cpp`.
     - Its files are that 1 path; it creates and deletes nothing.
     - Checks: P5.15 to P5.22, one step.
- `tools/check.sh --plugin` (P5.23) is the plan's final check, not a task's.
- Every task builds C++: its agent needs `bash` and `edit_file`, to run `cmake` and its own `tools/spec.sh` check
  before it hands off. Leave `allowed_tools` out, or include `bash`, `edit_file`, `read_file`, `write_file`, `grep`
  and `glob`.
