# Phase 2.2: gain staging

**Goal**: levels that behave. Width adds stereo without reshaping the mono signal's peaks; turning a drive up colours
the sound instead of turning it down; and at the defaults the rumble sits audibly under the bass.

**Depends on**: phase 2.1 (merged). Run it before phase 3, or after it: it touches no file phase 3 edits.

**Size**: S, 3 tasks, about 60 lines.

---

## Scope

In scope: `core/src/output_stage.cpp`, `core/src/filter.cpp` and `core/src/rumble.cpp`, with their headers' private
members. No new files, no new parameters, no change to any public interface or default.

Out of scope: everything under "Not now", the sequencer (phase 3) and the editor (phase 4).

## Context

- A listening test of phase 2 (offline renders through `undertow::Engine`, round v7's second wave) found three level
  problems. Each check below reproduces one, measured on phase 2's code with phase 2.1's fixes:
  - **The crossover reshapes peaks.** Width summed the crossover's low and high bands back together. That sum is an
    all-pass: magnitudes stay flat, but the phase rotates around `mono_freq`, which raises a saw's crest factor by 5 to
    6 dB and a square's by 4 to 7 dB at the same RMS. The default patch's accented notes owe about 4 dB of their peak
    to it.
  - **Drives turn the sound down.** `filter_drive` at 1 takes 12.2 dB off a saw at the default `osc_level`
    (0.9, 3.1 and 6.5 dB at 0.25, 0.5 and 0.75). `rumble_drive` takes 4.7 dB off the rumble at its default 0.4 and
    9.7 dB at 1, because `tanh(k x) / k` caps it at `1 / k`.
  - **The rumble is barely there.** At the defaults, holding a note, the rumble sits 25 dB under the bass below
    200 Hz.
- The spec tests: `tests/spec/p2_2/output_test.cpp`, `filter_test.cpp` and `rumble_test.cpp`. Change the code, never the
  tests. They join `tools/check.sh` as soon as they are on main: check.sh fails on the base until the three tasks land.
- Phase 2's P2.11 now allows a channel to gain up to 3.5 dB at width 1 (a side added to broadband noise gains 3 dB). It
  still requires flat bands at width 0, mono lows and decorrelated highs.

## What to fix

1. **Width** (`output_stage.h`, `output_stage.cpp`): a side signal, and no crossover on the mono path.
   - The mono signal (after the DC blocker, the saturation and the master gain) goes to both channels untouched:
     `left = x + s` and `right = x - s`.
   - The side `s` is `width` times a 12 ms delayed copy of `x`, high-passed at `mono_freq` with an 8th-order
     high-pass (two Linkwitz-Riley high-passes in cascade, four Butterworth sections). It is about 49 dB down at
     `mono_freq / 2`, so the low end stays mono, and the delay decorrelates the highs.
   - At width 0 both channels are exactly `x`. Nothing filters `x`, so its peaks keep their shape at any width.
2. **The filter's drive** (`filter.h`, `filter.cpp`): a makeup gain.
   - Keep the drive's curve, and multiply by a makeup gain that brings a voice-level signal back to its level: a
     110 Hz saw at 0.6 through cutoff 600 Hz and resonance 0.35 stays within 3 dB of drive 0 at every drive.
   - For the current blend `x + d (tanh(K x) / K - x)` with `K = 1 + 10 d`, a makeup of about `12 d^2` dB keeps that saw
     within 0.3 dB. Exactly 1 at drive 0: phase 1 and 2.1 still require drive 0 to be linear.
   - Quiet notes then get a little louder with drive and loud ones a little quieter. That is the compression a drive
     is for.
3. **The rumble** (`rumble.h`, `rumble.cpp`): a makeup gain on its saturation, and an output gain.
   - Multiply `tanh(k x) / k` by a makeup gain that keeps the rumble alone (its ghost kick, nothing sent) within 3 dB
     of drive 0 at every drive. For the current rumble about `10 drive` dB does it. The saturation must still round the
     peaks: full drive lowers the rumble's crest factor by at least 2 dB.
   - Raise the rumble's output gain (a constant after the duck, before `rumble_level`) so that, at every default, the
     rumble sits 9 to 15 dB under the bass below 200 Hz. `rumble_level`'s default stays 0.3, so the knob keeps its
     room. At 1 the rumble comes within a few dB of the bass, which is what the RUMBLE presets want.

## API

No change.

## Acceptance checks

Checks that share a command are one verify step: the table repeats the command on every row that command proves.

| Check | Command | Proves |
|---|---|---|
| P2.2.01 | `tools/spec.sh 2_2 output 'P2.2.01*,P2.2.02*'` | At width 0 the output stage leaves a saw's (110 and 220 Hz) and a square's (220 and 440 Hz) crest factor within 0.5 dB, both channels the same |
| P2.2.02 | `tools/spec.sh 2_2 output 'P2.2.01*,P2.2.02*'` | At width 0.5 and 1 the mid, (L + R) / 2, is the width-0 output (within -60 dB), and below 60 Hz the side stays 30 dB under the mid |
| P2.2.03 | `tools/spec.sh 2_2 filter 'P2.2.03*'` | A 110 Hz saw at 0.6 through cutoff 600 Hz and resonance 0.35 stays within 3 dB of its drive-0 RMS at filter_drive 0.25, 0.5, 0.75 and 1 |
| P2.2.04 | `tools/spec.sh 2_2 rumble 'P2.2.04*,P2.2.05*'` | The rumble alone stays within 3 dB of its drive-0 RMS at rumble_drive 0.25 to 1, and full drive lowers its crest factor by at least 2 dB |
| P2.2.05 | `tools/spec.sh 2_2 rumble 'P2.2.04*,P2.2.05*'` | At every default the rumble sits 9 to 15 dB under the bass below 200 Hz holding A1 and then A2 (velocity 100, 4 bars), and 4 to 11 dB under a rolling line on A2 (sixteenths that rest on every beat) |
| P2.2.06 | `tools/spec.sh 2 output` | Phase 2's output checks still pass (P2.09 to P2.12, P2.20, P2.21; P2.11 as amended). A regression check: it passes on the base too |
| P2.2.07 | `tools/spec.sh 2 engine 'P2.15*,P2.16*'` | Phase 2's engine width check still passes: at width 1 the full mix keeps the side 25 dB under the mid below mono_freq / 2. A regression check |
| P2.2.08 | `tools/spec.sh 2 engine 'P2.17*,P2.22*'` | The default patch's accented notes still peak between -12 and -6 dBFS (P2.22), now with less crest. A regression check |
| P2.2.09 | `tools/spec.sh 1 filter` | Phase 1's filter checks still pass: drive 0 linear, drive 1 an odd-harmonic saturation. A regression check |
| P2.2.10 | `tools/spec.sh 2 rumble` | Phase 2's rumble checks still pass (P2.04 to P2.08: the grid, the low band, the tail, bounded at full settings). A regression check |

## Rules every task keeps

- The rules of phases 0 to 2.1 still hold: never edit `tests/spec/`, `third_party/`, `docs/` or the given tools; `core/`
  stays plain C++17; no allocation, lock, I/O or logging in anything `process` calls.
- Change levels and topology only: the saturation curves, the filter's response, the reverb and the duck stay as they
  are, and so does every default.

## Also required, not checked automatically

- Listening: at width 0.25 a held saw sounds as before, a little wider above 120 Hz; sweeping `filter_drive` or
  `rumble_drive` from 0 to 1 adds grit at a steady level; at the defaults the rumble is heard as a pumping bed under the
  bass.

## Not now

- `sat_drive` still rounds loud peaks down (up to 12 dB at full drive), by design: phase 2 locked its blend in P2.1.09,
  and `master_gain` makes up the level.
- The duck's quietest moment comes 62 to 74 ms after each kick, because the reverb's new energy arrives through its
  delay lines. That is a short hole after the kick, not a defect.
- The clipper is not oversampled. Hot saws gain a little in-band aliasing, still well under the PRD's -50 dB.

## Notes for the planner

- Three tasks, one per test file. They share no file, so all three run in parallel. Each task's verify steps are exactly
  the checks named for it, with their commands unchanged:
  1. **Width** (`core/include/undertow/output_stage.h`, `core/src/output_stage.cpp`): P2.2.01 and P2.2.02 in one step,
     and P2.2.06 to P2.2.08. About 40 lines.
  2. **The filter's drive** (`core/include/undertow/filter.h`, `core/src/filter.cpp`): P2.2.03 and P2.2.09. About 10
     lines.
  3. **The rumble** (`core/include/undertow/rumble.h`, `core/src/rumble.cpp`): P2.2.04 and P2.2.05 in one step, and
     P2.2.10. About 15 lines.
- P2.2.06 to P2.2.10 are regression checks (`expect = "pass_on_base"`). They keep each task inside phase 1 and 2's
  checks: width must still keep the low end mono and decorrelate the highs, the drives must stay linear at 0 and
  distorting at 1, and the default patch must stay inside P2.22's window.
- Headers live in `core/include/undertow/`, sources in `core/src/`: list each task's files with those paths, and create
  no new file.
- Every task builds C++: its agent needs `bash` and `edit_file`, to run `cmake` and its own `tools/spec.sh` check
  before it hands off. Leave `allowed_tools` out, or include `bash`, `edit_file`, `read_file`, `write_file`, `grep`
  and `glob`.
- `tools/check.sh` (every present spec test, phases 0 to 2.2) is the plan's final check, not any one task's.
