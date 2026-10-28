# Phase 5.5: auval clean

**Goal**: the person's install passes auval with no warning.
- Today auval succeeds with one WARNING, on Seq Gate: "Parameter did not retain default value when set".
- A host that sets a float parameter to its default reads that default back exactly, Seq Gate's included.
- Nothing a host sees changes: every parameter keeps its id, its range and its default.

**Depends on**: phase 5.4.

**Size**: S, 1 task, about 4 lines in `plugin/PluginProcessor.cpp`.

---

## Scope

In scope: the float case of `createParameterLayout` in `plugin/PluginProcessor.cpp`.

No new files, no API change, and no change to the engine, the core, the presets, the parameter table or the build.
No committed render or image changes.

Out of scope: everything else.

## Context

- **auval's warning** (`artifacts/reports/auval.txt`, the person's install of main `88391a5`): "retrievedValue =
  0.473684 (was 0.473684), Parameter did not retain default value when set", on Seq Gate (0.05 to 1, default 0.5).
  The AU validation still succeeded, and the parameter's own test passed.
- **What a host sees:** JUCE's AU wrapper hands the host every continuous parameter as 0 to 1. auval prints each range
  through the parameter's own text. Seq Gate's default, 0.5, is 0.473684192 (`0x3ef286bc`) to the host.
- **The cause**, found on 2026-10-28 with a read-only probe of the installed AU, a plugin-side test and the compiler:
  - JUCE's `NormalisableRange<float>::convertFrom0to1` computes `start + (end - start) * proportion`. clang builds that
    as one fused multiply-add on this arm64 Mac (`fsub`, then `fmadd`), so it is rounded once.
  - 0.473684192 becomes 0.49999997 rather than 0.5. That converts back to 0.473684162 (`0x3ef286bb`), one unit in
    the last place under the default.
  - A host that sets Seq Gate to 1 and then to its default reads back 0.473684162. auval does exactly that and
    compares exactly.
  - Opening the AU leaves Seq Gate at its default. A state round trip and initialising the AU move it further, to
    `0x3ef286b9` and `0x3ef286b6`.
  - Of the 38 float parameters, only Seq Gate's default fails to come back. Every other default survives the
    conversion exactly: Pulse Width's 0.5, Cutoff's 0.5, Master's 0.333333343.
  - It doesn't change the sound: the gate is 0.49999997 of a step instead of 0.5.
- **The fix, measured:** a 0.001 grid on Seq Gate's range (`range.interval = 0.001f`).
  - JUCE's `RangedAudioParameter` snaps a value to the grid on each conversion, both ways. So 0.49999997 lands on 0.5,
    which comes back as `0x3ef286bc`. Set to 1 and then to the default, it reads the default exactly.
  - **What the grid changes:**
    - A host's value of Seq Gate snaps to the nearest 0.001: 951 values from 0.05 to 1.
    - Its text, as a host shows it, has 3 decimals ("0.500") instead of 7 ("0.5000000"): JUCE takes the decimals
      from the grid. The editor's GATE readout keeps its own format.
    - A state saved with Seq Gate off the grid loads as the nearest 0.001: 0.4567891 loads as 0.457. That moves it
      0.0002 of a step, and at most 0.0005.
  - **What it keeps:**
    - the parameter id `seq_gate`, its range ends (0.05, 1), its default (0.5) and every other parameter;
    - the presets' Seq Gate values (0.45, 0.55, 0.62), which are on the grid.
- **Fixes that don't fit:**
  - Moving the default (to 0.525) or the range (to 0 to 1) makes the arithmetic exact but changes what a host sees:
    P5.5.03.
  - A coarse grid (0.05) moves saved values by up to 0.025: P5.5.04.
  - A grid on every float parameter changes every parameter's steps and text: P5.5.03.
  - Moving the parameter table's default changes the core: P0.01 (the table matches the PRD row for row).
  - Building without FP contraction changes all floating-point code in the plugin target, the editor's drawing
    included, against byte-identical review images. It's out of scope.
- **The spec tests:**
  - `tests/spec/p5_5/auval_test.cpp` (plugin preset). Main's `tools/check.sh --plugin` fails on P5.5.01 and P5.5.02
    until this phase lands.
  - Change the code, never the tests.

## What to build

1. **auval clean** (`plugin/PluginProcessor.cpp`)
   - In `createParameterLayout`'s float case, after the skew, give Seq Gate's range a 0.001 grid, with a comment
     saying why: `if (i == undertow::param::SeqGate) range.interval = 0.001f;`.
   - A host that sets Seq Gate to 1 and then to its default reads its default exactly (P5.5.01). So does every float
     parameter, from either end of its range (P5.5.02).
   - Every float parameter keeps its id, its range ends and its default as a host sees them, and only Seq Gate's
     range has a grid (P5.5.03).
   - A saved state with Seq Gate off the grid still loads, within 0.0005, with every other parameter as saved, and
     plays (P5.5.04).
   - **Over-fixes the checks hold against:**
     - a moved default or range: P5.5.03;
     - a coarse grid: P5.5.04;
     - a grid on every parameter: P5.5.03;
     - a renamed parameter: P5.5.01 to P5.5.04;
     - the table's default moved: P0.01.
   - **Nothing else changes.**

## API

No change. A host shows Seq Gate's value with 3 decimals.

## Acceptance checks

Checks that share a command are one verify step: the table repeats the command on every row that command proves.

| Check | Command | Proves |
|---|---|---|
| P5.5.01 | `tools/spec.sh 5_5 auval` | Seq Gate comes back as its default: set by a host to 1 and then to its default, it reads its default exactly |
| P5.5.02 | `tools/spec.sh 5_5 auval` | Every float parameter comes back as its default, set by a host from either end of its range |
| P5.5.03 | `tools/spec.sh 5_5 auval` | Every float parameter keeps its id, its range ends and its default as a host sees them; only Seq Gate's range has a grid. It passes on the base too: a guard |
| P5.5.04 | `tools/spec.sh 5_5 auval` | A saved state with Seq Gate off the 0.001 grid still loads, Seq Gate within 0.0005 of it and the rest as saved, and plays. It passes on the base too: a guard |
| P5.5.05 | `tools/spec.sh 0 params` | P0.01 and P0.02 still pass: the parameter table matches the PRD row for row. A regression check: it passes on the base too |
| P5.5.06 | `tools/spec.sh 0 plugin` | P0.08-P0.13 still pass: the plugin's 57 parameters with the table's ranges, its state, its bundles. A regression check: it passes on the base too |
| P5.5.07 | `tools/spec.sh 5 programs` | P5.04 still passes: the presets as programs set every parameter, Seq Gate's on the grid. A regression check: it passes on the base too |
| P5.5.08 | `tools/spec.sh 4 state` | P4.02 still passes: the pattern in the plugin's state. A regression check: it passes on the base too |
| P5.5.09 | `tools/spec.sh 4_2 pattern` | P4.2.14-P4.2.16 still pass: a new instance plays and saves the default pattern. A regression check: it passes on the base too |
| P5.5.10 | `tools/spec.sh 4_2 layout` | P4.2.01-P4.2.05 still pass: the knobs and their readouts, GATE's included. A regression check: it passes on the base too |

## Rules every task keeps

- The rules of phases 0 to 5.4 still hold.
- Only `plugin/PluginProcessor.cpp` changes. No engine, core, preset, parameter-table or build change.
- No new files.
- The person's own install is in place (`~/Library/Audio/Plug-Ins`, `~/Applications`). No task installs, runs `auval`
  or `tools/auval.sh` (which validates the installed AU), or calls `tools/install.sh` without `--dest`.

## Also required, not checked automatically

- **The person's step after the merge.** The person has OK'd installs and auval, and the lead runs it:
  - reinstall from main with `tools/install.sh` in its real mode (no `--dest`);
  - run `auval -v aumu MkUt Mtks` (`tools/auval.sh`), expecting "AU VALIDATION SUCCEEDED" with no WARNING;
  - keep its log as `artifacts/reports/auval.txt`, in place of the one from `88391a5`.
- Listening: nothing changes.

## Notes for the planner

- This spec is `docs/specs/phase-5.5-auval.md`. Put it first among the files the task reads.
- One task: **auval clean** (`plugin/PluginProcessor.cpp`).
  - Its verify steps, with their commands unchanged:
    - P5.5.01 to P5.5.04 in one step;
    - then P5.5.05 to P5.5.10 (`expect = "pass_on_base"`).
  - About 4 lines.
- The task builds the plugin preset (JUCE): its agent needs `bash` and `edit_file`, to run `cmake` and its own
  `tools/spec.sh` check before it hands off. Leave `allowed_tools` out, or include `bash`, `edit_file`, `read_file`,
  `write_file`, `grep` and `glob`.
- `tools/check.sh --plugin` is the plan's final check, not the task's: every present spec test, both presets, and
  pluginval at strictness 5. It runs `tools/render-demos.sh` and a test install into `build/install-test` before the
  core tests.
