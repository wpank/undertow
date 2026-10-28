# Phase 5.4: Kick Companion as named

**Goal**: SUB Kick Companion becomes what phase 5's table calls it, "a short, round sub note that sits after a kick".
- Today it holds its note at sustain 0.9, like the two other SUB presets. The person chose short: a decaying note of
  about 200 to 300 ms, not a sustain at 0.9.
- It stays the clean sub that P5.2.02 holds every SUB preset to.
- So a short note isn't read as a quiet one, the level check (P5.09) reads a SUB preset over its loudest 300 ms.

**Depends on**: phase 5.3.

**Size**: S, 1 task, about 5 lines of preset values.

---

## Scope

In scope: SUB Kick Companion's values in `core/src/presets.cpp`.

No new files, no API change, no other preset, no engine change, and no committed render: Kick Companion isn't one of
the four showcases, so every showcase WAV in `artifacts/audio/` stays byte for byte.

The spec, not the task, changes four earlier checks for a short SUB note:
- **P5.09** reads a SUB preset's level over its loudest 300 ms in the first second, instead of the 2 bars
  (`tests/spec/p5/presets_test.cpp`).
- **P5.02's -30 dBFS floor** leaves SUB presets to P5.09's floor (the same file).
- **P5.1.03** reads the renderer's 8 bars of a SUB preset as P5.09 does, over its loudest 300 ms in the first second
  (`tests/spec/p5_1/render_test.cpp`).
- **P5.1.02** counts a bar under -120 dBFS as silence (the same file). Its renderer writes 24-bit files, which hold
  nothing under -144 dBFS, while a short note's tail can still read -250 dBFS in the engine's floats a bar later.

Out of scope: everything else.

## Context

All numbers were measured on phase 5.3's delivered presets (main `88391a5`), by audio-qa and again by the spec tests.
"The voice" is a preset held as P5.09 renders it (A2 at velocity 100), with its rumble layer muted, as the mid of the two
channels. Its envelope is its RMS over one cycle of 55 Hz (18 ms), every 1 ms.

- **Kick Companion holds its note.**
  - Its voice is the 55 Hz sine sub at `amp_sustain` 0.9 after an 80 ms decay. It never falls 20 dB under its peak:
    at 300 ms it is 0.1 dB under, and from 1 s on, still 0.1 dB under.
  - Its first 40 ms sit -0.4 dB re its level at 280 to 320 ms, P5.3.03's reading. Pure Foundation reads -0.7 dB.
  - It never set `sat_drive` or `rumble_level`, so it plays the defaults, 0.2 and 0.3. Its harmonics 2 to 10 sit 35 dB
    under the sub over 20 to 190 ms, where Pure Foundation's sit 92 dB under.
- **The person's choice:** "a short, round sub note": a decaying note of about 200 to 300 ms.
- **Short:** how fast the voice falls 20 dB under its peak, at `amp_sustain` 0, for each amp decay:

  | `amp_decay` | 150 ms | 180 ms | 200 ms | 250 ms | 300 ms | 400 ms | 500 ms | 600 ms |
  |---|---|---|---|---|---|---|---|---|
  | falls 20 dB at | 95 ms | 112 ms | 122 ms | 148 ms | 171 ms | 223 ms | 276 ms | 326 ms |

  - From 1 s on, each of these sits 65 dB or more under its peak.
  - A sustain left above 0 holds a quiet note instead: 0.02 holds the voice at -32 dB from 1 s on, 0.1 at -18 dB.
- **Round:**
  - **No click:** the engine starts each note at phase 0, so the sine sub has no click at any amp attack (0.5 to
    30 ms):
    - its energy above 2 kHz in the first 5 ms sits 60 to 120 dB under its peak;
    - its steepest step from one sample to the next is 0.1 to 1.1 times a 55 Hz sine's at the peak level.
  - **A click comes from a bright layer:**
    - a saw blip through the filter envelope (the saw at 0.15, an octave up, through a 60 Hz filter opened 6 octaves
      for 60 ms) reads -40 dB and 4.2 times;
    - the square left open reads -29 dB and 15 times.
  - **Saturation** makes harmonics. Harmonics 2 to 10 re the sub over 20 to 190 ms, for each `sat_drive`:

    | `sat_drive` | 0 | 0.1 | 0.2 | 0.3 | 0.4 | 0.6 |
    |---|---|---|---|---|---|---|
    | harmonics 2 to 10 | -89 dB | -56 dB | -46 dB | -40 dB | -36 dB | -29 dB |

- **The level check (P5.09) reads 2 bars of RMS.**
  - A short note's 2 bars are mostly the silence after it: the candidate below reads -29.1 dBFS over the 2 bars and
    -18.2 over its loudest 300 ms.
  - For a held note, the loudest 300 ms in the first second reads within 0.3 dB of the 2 bars:

    | Preset | Loudest 300 ms | 2 bars |
    |---|---|---|
    | Pure Foundation | -17.54 | -17.55 |
    | Low Pressure | -17.88 | -18.15 |
    | Kick Companion today | -18.40 | -18.64 |

  - A quiet held preset still fails: Pure Foundation 6 dB down reads -23.5 either way.
  - The loudest 300 ms no longer reads a fade, though. Pure Foundation fading to sustain 0.5 at master -11.5 reads
    -18.5 there, so P5.4.06 holds the two held SUB presets to their held note.
- **P5.02's floor:** -30 dBFS RMS on the left channel over the 2 bars, so a preset must be heard.
  - The candidate reads -29.1 dBFS there, 0.9 dB clear, and a 250 ms decay landed on -18 reads -29.7.
  - So SUB presets are left to P5.09's floor on their loudest 300 ms, 9 dB higher.
- **The rumble:** Kick Companion never set `rumble_level`, so it plays the default 0.3. With a short voice, the rumble
  fills the 2 bars alone:
  - 75% of the energy is below 80 Hz, where P5.2.02 needs 95%;
  - 3% is below 20 Hz, where P5.2.03 allows 2%.
  - At `rumble_level` 0 it is 100% and 0%.
- **The candidate:** `amp_sustain` 0, `amp_decay` 400 ms, `sat_drive` 0.1, `rumble_level` 0, `master_gain` -6, the
  1 ms attack kept.
  - It falls 20 dB at 223 ms and sits 100 dB under from 1 s on.
  - Above 2 kHz in the first 5 ms: -67 dB. Its steepest step is 0.63 times a sine's. Harmonics: -56 dB.
  - Its first 40 ms sit 26.8 dB over its level at 280 to 320 ms. Pure Foundation's sit at -0.7, Low Pressure's at -17.6.
  - Level: -18.2 dBFS over its loudest 300 ms. Peak -7.9 dBFS.
- **The spec tests:**
  - `tests/spec/p5_4/presets_test.cpp` (core preset) waits for `presets.h`, which main has, so `tools/check.sh` fails
    on it until this phase lands.
  - `tests/spec/p5/presets_test.cpp` and `tests/spec/p5_1/render_test.cpp` hold P5.02, P5.09, P5.1.02 and P5.1.03 as
    this phase changes them. They pass before and after.
  - Change the code, never the tests.

## What to build

1. **Kick Companion as named** (`core/src/presets.cpp`)
   - **Short:** `amp_sustain` 0 and an amp decay of about 250 to 500 ms.
     - Holding A2, its voice falls 20 dB under its peak 120 to 300 ms after it starts (P5.4.01). 400 ms falls at
       223 ms.
     - From 1 s on, it stays at least 40 dB under its peak: the note ends rather than holding quietly (P5.4.02).
   - **Round:**
     - No click: in its first 5 ms, its energy above 2 kHz at least 45 dB under its peak, and no step from one
       sample to the next steeper than twice a 55 Hz sine's at its peak level (P5.4.03).
     - A round tone: harmonics 2 to 10 at least 40 dB under its 55 Hz sub over 20 to 190 ms (P5.4.04). That is
       `sat_drive` about 0.3 or less; 0.1 gives -56 dB.
   - **The sub:** A2's sub at -1 oct, 55 Hz, as today, with `osc_level` 0.
     - `rumble_level` 0: a short note leaves nothing for a rumble to fill the 2 bars with. P5.2.02 holds every SUB
       preset to 95% of its energy below 80 Hz, and P5.2.03 every preset to 2% below 20 Hz.
   - **Apart from the held SUB presets:** its first 40 ms at least 20 dB further over its level at 280 to 320 ms than
     Pure Foundation's and Low Pressure's (P5.4.05).
   - **The level:** P5.09 now reads a SUB preset over its loudest 300 ms in the first second, within 3 dB of -18 dBFS,
     and P5.1.03 reads the renderer's 8 bars of it the same way. About `master_gain` -6.
   - It keeps its sequencer off and its pattern's steps.
   - **Over-fixes the checks hold against:**
     - "Short" doesn't mean a blip. A note 20 dB down in under 120 ms fails P5.4.01 whatever its level: a 150 ms decay
       at `master_gain` -3 reads -19.7 dBFS over its loudest 300 ms and still fails.
     - "Level" doesn't mean louder: P5.09 holds the loudest 300 ms to 3 dB either side of -18.
     - The other SUB presets keep holding their note (P5.4.06), and a quiet one still fails P5.09.
   - **Nothing else changes:** the task changes no other preset and no pattern's steps. It leaves every showcase WAV
     in `artifacts/audio/` byte for byte: the demos steps' `tools/render-demos.sh` writes them again, unchanged.

## API

No change.

## Acceptance checks

Checks that share a command are one verify step: the table repeats the command on every row that command proves.

| Check | Command | Proves |
|---|---|---|
| P5.4.01 | `tools/spec.sh 5_4 presets` | SUB Kick Companion is short: holding A2, its voice falls 20 dB under its peak 120 to 300 ms after it starts |
| P5.4.02 | `tools/spec.sh 5_4 presets` | Its note ends: from 1 s on, its voice stays at least 40 dB under its peak |
| P5.4.03 | `tools/spec.sh 5_4 presets` | It starts round: in its first 5 ms, its energy above 2 kHz at least 45 dB under its peak, and no step steeper than twice a 55 Hz sine's at its peak level. It passes on the base too: a guard against a click |
| P5.4.04 | `tools/spec.sh 5_4 presets` | It is round in tone: harmonics 2 to 10 at least 40 dB under its 55 Hz sub over 20 to 190 ms |
| P5.4.05 | `tools/spec.sh 5_4 presets` | It is apart from SUB Pure Foundation and SUB Low Pressure: its first 40 ms at least 20 dB further over its level at 280 to 320 ms than theirs |
| P5.4.06 | `tools/spec.sh 5_4 presets` | SUB Pure Foundation and SUB Low Pressure still hold their note: the last 300 ms of 2 bars within 3 dB of their peak. It passes on the base too: a guard |
| P5.4.07 | `tools/spec.sh 5 presets` | P5.01-P5.03 and P5.09 pass, with P5.09 reading a SUB preset over its loudest 300 ms in the first second and P5.02's -30 dBFS floor leaving SUB presets to it, as this phase changes them: Kick Companion's level. It passes on the base too |
| P5.4.08 | `tools/spec.sh 5_1 render` | P5.1.02 and P5.1.03 pass, as this phase changes them (a bar under -120 dBFS is silence; a SUB preset's render read as P5.09 does): the renderer plays the short note as the engine does, in level. It passes on the base too |
| P5.4.09 | `tools/spec.sh 5_3 presets` | P5.3.01-P5.3.04 still pass: Low Pressure as named. A regression check: it passes on the base too |
| P5.4.10 | `tools/spec.sh 5_2 presets` | P5.2.01-P5.2.04 still pass: every SUB preset the sub, nothing below 20 Hz. A regression check: it passes on the base too |
| P5.4.11 | `tools/render-demos.sh && tools/spec.sh 5_1 demos` | P5.1.04 and P5.1.05 still pass: the committed showcase renders are the renderer's files as written. A regression check: it passes on the base too |
| P5.4.12 | `tools/render-demos.sh && tools/spec.sh 5 demos` | P5.06 still passes. A regression check: it passes on the base too |

## Rules every task keeps

- The rules of phases 0 to 5.3 still hold.
- Only SUB Kick Companion changes. No engine change.
- No new files.
- The person's own install is in place (`~/Library/Audio/Plug-Ins`, `~/Applications`). No task installs, runs `auval`
  or `tools/auval.sh` (which validates the installed AU), or calls `tools/install.sh` without `--dest`.

## Also required, not checked automatically

- Listening, in FL Studio at 128 BPM: Kick Companion is a short, round thump that sits after a kick and is gone before
  the next. It doesn't click.
- **A known auval warning**, from the person's install of main `88391a5` (`artifacts/reports/auval.txt`). The AU
  validation succeeded, with one WARNING, on Seq Gate (0.05 to 1, default 0.5): "retrievedValue = 0.473684 (was
  0.473684), Parameter did not retain default value when set".
  - JUCE's AU wrapper hands the host every continuous parameter as 0 to 1. auval prints each one's range through the
    parameter's own text: Seq Gate's 0.05 to 1, default 0.5; Cutoff's default 600.0000610.
  - So 0.473684 is Seq Gate's default, 0.5, as its normalised position in 0.05 to 1.
  - Only Seq Gate warns. Each parameter's default, Seq Gate's included, comes back exactly when converted to 0 to 1
    and back in single precision, as its range does it. So the value read back differs from the one written only past
    the six digits auval prints, for a reason not found without auval. The parameter's own test passes.
  - No check here covers it. A fix changes the parameter's definition, which touches automation and saved state, so
    it needs its own spec.

## Notes for the planner

- This spec is `docs/specs/phase-5.4-presets.md`. Put it first among the files the task reads.
- One task: **Kick Companion as named** (`core/src/presets.cpp`).
  - Its verify steps, with their commands unchanged:
    - P5.4.01 to P5.4.06 in one step;
    - then P5.4.07 to P5.4.12 (`expect = "pass_on_base"`).
  - About 5 lines. It commits no render.
- The task builds C++: its agent needs `bash` and `edit_file`, to run `cmake` and its own `tools/spec.sh` check before
  it hands off. Leave `allowed_tools` out, or include `bash`, `edit_file`, `read_file`, `write_file`, `grep` and
  `glob`.
- `tools/check.sh --plugin` is the plan's final check, not the task's: every present spec test, both presets, and
  pluginval at strictness 5. It runs `tools/render-demos.sh` and a test install into `build/install-test` before the
  core tests.
