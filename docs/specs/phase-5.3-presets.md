# Phase 5.3: Low Pressure as named, and the Roller at level

**Goal**: two presets made what the PRD and phase 5's table say:
- SUB Low Pressure becomes "sub at -2 oct with a little saturation, a slow swell", where today it is a copy of SUB Pure
  Foundation. It stays the clean sub that P5.2.02 holds every SUB preset to.
- BASS Hypnotic Roller's showcase render plays at the level of the other three, where today it dips 2.3 dB.

**Depends on**: phase 5.2.

**Size**: S, 1 task, about 5 lines of preset values and one showcase render made again.

---

## Scope

In scope:
- SUB Low Pressure's and BASS Hypnotic Roller's values in `core/src/presets.cpp`;
- the four committed showcase renders in `artifacts/audio/`, of which only `bass-hypnotic-roller.wav` changes.

No new files, no API change, no other preset, and no engine change.

Out of scope: everything else.

## Context

All numbers are audio-qa's, measured on phase 5.2's delivered presets.

- **SUB Low Pressure is the same sound as SUB Pure Foundation.** Read on the voice alone (holding A2 at velocity 100,
  its rumble layer muted, as P5.09 renders it):
  - both are a clean 55 Hz sine: harmonics at -130 dB, the same 3 ms attack, no dip, sustain 1;
  - only a light rumble layer, 1.9% of the energy, tells them apart.
- **Its description isn't in its values.** Phase 5's table says "sub at -2 oct with a little saturation, a slow
  swell". The values are:
  - `sub_octave` -1 oct (A2's sub at 55 Hz, not 27.5 Hz);
  - `sat_drive` 0;
  - `amp_attack` 3 ms.
- **Phase 5.2 didn't cover it.** It took the saw out of every SUB preset (P5.2.02), which left Low Pressure a copy of
  Pure Foundation.
- **audio-qa's measurements of Low Pressure, on the voice alone:**
  - **-2 oct:** with `sub_octave` -2 oct, 100% of the voice's energy is at 25 to 30 Hz at saturation 0, and 99.53%
    at 0.5. Today 0% is there: it is all at 50 to 60 Hz.
  - **Saturation:** the output saturation is symmetric, so it adds odd harmonics only (the 2nd stays at -135 dB).
    The 3rd harmonic, 82.5 Hz, re the 27.5 Hz sub at each `sat_drive` setting:

    | `sat_drive` | 0.05 | 0.1 | 0.2 | 0.3 | 0.4 | 0.5 |
    |---|---|---|---|---|---|---|
    | 3rd harmonic | -50.2 dB | -42.9 dB | -35.1 dB | -30.2 dB | -26.7 dB | -23.9 dB |

    Pure Foundation's is at -138 dB. From -20 dB up it is an audible growl.
  - **Swell:** the voice's first 40 ms re its level at 280 to 320 ms, at each `amp_attack` setting:

    | `amp_attack` | 3 ms | 50 ms | 100 ms | 150 ms | 200 ms | 300 ms |
    |---|---|---|---|---|---|---|
    | first 40 ms | -1.6 dB | -8.2 dB | -14.1 dB | -17.6 dB | -20.1 dB | -23.4 dB |

    Pure Foundation reads -0.7 dB. The windows are 40 ms because one cycle of 27.5 Hz is 36 ms.
  - **Apart from Pure Foundation:** today both voices centre at 55.0 Hz below 200 Hz. At -2 oct, Low Pressure's
    centres at 27.5 Hz, a full octave apart.
  - **The candidate** (`sub_octave` -2 oct, `sat_drive` 0.2, `amp_attack` 150 ms, rumble kept):
    - voice: 99.97% at 25 to 30 Hz, the 3rd harmonic at -35.1 dB, the onset at -17.6 dB;
    - the full preset: 99.56% below 80 Hz and 0.09% below 20 Hz;
    - level: -19.15 dBFS at `master_gain` -15. At -14 it lands about -18.2.
    - More saturation lowers the level, about -2.6 dB at 0.5.
- **BASS Hypnotic Roller (audio-qa's I12)** renders at -20.39 dBFS in P5.09's 2 bars, 0.61 dB inside its floor. Its
  8-bar showcase render plays at -20.56 dBFS RMS, against -18.3 for init and -18.2 for rumble-dark-room, so the reel
  dips 2.3 dB at it. About +2 dB of `master_gain` brings it level.
- **The spec tests:**
  - `tests/spec/p5_3/presets_test.cpp` and `tests/spec/p5_3/demos_test.cpp` (core preset).
  - Both wait for `presets.h`, which main has, so `tools/check.sh` fails on them until this phase lands.
  - Change the code, never the tests.

## What to build

1. **Low Pressure as named, and the Roller at level** (`core/src/presets.cpp` and the four `artifacts/audio/*.wav`)
   - **SUB Low Pressure:**
     - **Sub at -2 oct:** `sub_octave` -2 oct. Holding A2, at least 97% of its voice's energy is at 25 to 30 Hz.
     - **A little saturation:** the voice's 3rd harmonic 25 to 45 dB under its fundamental, the 27.5 Hz sub. That
       is `sat_drive` about 0.1 to 0.4; 0.2 gives -35 dB.
     - **A slow swell:** the voice's first 40 ms at least 12 dB under its level at 280 to 320 ms. That is an amp
       attack of about 100 ms or more; 150 ms gives -17.6 dB.
     - **Apart from Pure Foundation:** their voices centre at least half an octave apart below 200 Hz.
     - **The level:** within 3 dB of -18 dBFS RMS (P5.09). About `master_gain` -14.
     - **The clean sub:** it stays what P5.2.02 asks of every SUB preset, at least 95% of its energy below 80 Hz,
       with no main oscillator added, and out of the infrasonic (P5.2.03).
     - It keeps its sequencer off, its pattern's steps and its rumble layer.
   - **BASS Hypnotic Roller:** raise its `master_gain` about 2 dB, so its showcase render plays within 1.5 dB of
     init's. Nothing else in it changes.
   - **Over-fixes the checks hold against:**
     - "Distinct" doesn't mean louder: P5.09.
     - It doesn't mean dirtier: saturation above about 0.4 fails P5.3.02's upper bound.
     - It doesn't mean another oscillator: a saw fails P5.2.02.
     - "Level" doesn't mean louder than the rest: P5.3.05 holds each showcase within 1.5 dB of init on both sides.
   - **Nothing else changes:** the task changes no other preset and no pattern's steps.
   - **The showcases:** run `tools/render-demos.sh` and commit the four showcase renders it writes.
     `bass-hypnotic-roller.wav` changes; the other three stay byte for byte.

## API

No change.

## Acceptance checks

Checks that share a command are one verify step: the table repeats the command on every row that command proves.

| Check | Command | Proves |
|---|---|---|
| P5.3.01 | `tools/spec.sh 5_3 presets` | SUB Low Pressure sounds its sub at -2 oct: holding A2, at least 97% of its voice's energy at 25 to 30 Hz |
| P5.3.02 | `tools/spec.sh 5_3 presets` | A little saturation: its voice's 3rd harmonic 25 to 45 dB under its fundamental |
| P5.3.03 | `tools/spec.sh 5_3 presets` | A slow swell: its voice's first 40 ms at least 12 dB under its level at 280 to 320 ms |
| P5.3.04 | `tools/spec.sh 5_3 presets` | SUB Low Pressure is not SUB Pure Foundation: their voices centre at least half an octave apart below 200 Hz |
| P5.3.05 | `tools/render-demos.sh && tools/spec.sh 5_3 demos` | The four showcase renders play within 1.5 dB of init's level, BASS Hypnotic Roller's included |
| P5.3.06 | `tools/spec.sh 5_2 presets` | P5.2.01-P5.2.04 still pass: Pure Foundation clean, every SUB preset the sub, nothing below 20 Hz, Sidechain Fog's floors. A regression check: it passes on the base too |
| P5.3.07 | `tools/spec.sh 5 presets` | P5.01-P5.03 and P5.09 still pass: the presets keep their categories, characters and levels. A regression check: it passes on the base too |
| P5.3.08 | `tools/render-demos.sh && tools/spec.sh 5_1 demos` | P5.1.04 and P5.1.05 still pass: the committed showcase renders are the renderer's files as written. A regression check: it passes on the base too |
| P5.3.09 | `tools/render-demos.sh && tools/spec.sh 5 demos` | P5.06 still passes. A regression check: it passes on the base too |

## Rules every task keeps

- The rules of phases 0 to 5.2 still hold.
- Only SUB Low Pressure and BASS Hypnotic Roller change. No engine change.
- No new files.

## Also required, not checked automatically

- Listening: Low Pressure swells in under the floor, warmer than Pure Foundation and an octave lower, not louder. The
  showcase reel plays at one level.

## Notes for the planner

- This spec is `docs/specs/phase-5.3-presets.md`. Put it first among the files the task reads.
- One task: **Low Pressure as named, and the Roller at level** (`core/src/presets.cpp` and the four
  `artifacts/audio/*.wav`).
  - Its verify steps, with their commands unchanged:
    - P5.3.01 to P5.3.04 in one step;
    - P5.3.05 in another;
    - then P5.3.06 to P5.3.09 (`expect = "pass_on_base"`).
  - About 5 lines. It commits the four WAVs its demos step writes.
- The task builds C++: its agent needs `bash` and `edit_file`, to run `cmake` and its own `tools/spec.sh` check before
  it hands off. Leave `allowed_tools` out, or include `bash`, `edit_file`, `read_file`, `write_file`, `grep` and
  `glob`.
- `tools/check.sh --plugin` is the plan's final check, not the task's: every present spec test, both presets, and
  pluginval at strictness 5. It runs `tools/render-demos.sh` and a test install before the core tests.
