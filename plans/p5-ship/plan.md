---
plan: p5-ship
title: "P5 · ship"
---

# P5 · ship

roko wrote this overview from the plan's `tasks.toml`, because the planner wrote no `plan.md`. Where the two differ, `tasks.toml` decides.

## Request

A chat host asked for this plan. Its request is not repeated here: it is untrusted data, and later prompts read this file.

## Tasks

8 task(s) in 4 wave(s). The tasks of a wave do not depend on each other; each task waits for the tasks it depends on.

### Wave 1

- **T1: Define the factory presets** (implementer, integrative)
  - Goal: Undertow exposes twelve named factory presets with stable slugs and preset data.
  - Files: `core/include/undertow/presets.h`, `core/src/presets.cpp`
  - Check (test): `tools/spec.sh 5 presets`

- **T6: Raise the resonance ceiling at the top of the knob** (implementer, mechanical)
  - Goal: A quiet tone at the cutoff gains more resonance at the top of the filter knob without breaking earlier filter behavior.
  - Files: `core/include/undertow/filter.h`, `core/src/filter.cpp`
  - Check (test): `tools/spec.sh 5 filter`
  - Check (test): `tools/spec.sh 2_2 filter`
  - Check (test): `tools/spec.sh 2_1 filter`

- **T7: Keep probability-one sequencer draws below 1** (implementer, mechanical)
  - Goal: Probability-1 steps no longer drop due to float rounding, while earlier sequencer behavior stays intact.
  - Files: `core/src/sequencer.cpp`
  - Check (test): `tools/spec.sh 5 sequencer`
  - Check (test): `tools/spec.sh 3 sequencer`

### Wave 2

- **T2: Wire presets and strict state restore into the plugin** (implementer, integrative)
  - Goal: The plugin advertises all factory presets as programs and restores state safely under strict validation.
  - After: T1
  - Files: `plugin/PluginProcessor.h`, `plugin/PluginProcessor.cpp`, `artifacts/reports/pluginval-strict.txt`
  - Check (test): `tools/spec.sh 5 programs`
  - Check (test): `tools/pluginval.sh 10 artifacts/reports/pluginval-strict.txt`
  - Check (test): `tools/spec.sh 4_2 pattern`

- **T3: Add the offline renderer target and CLI** (implementer, integrative)
  - Goal: The build produces undertow_render, and it renders presets deterministically to WAV files from the command line.
  - After: T1
  - Files: `CMakeLists.txt`, `tools/render/CMakeLists.txt`, `tools/render/main.cpp`
  - Check (test): `tools/spec.sh 5 render`

### Wave 3

- **T4: Script the demo renders and commit the showcase audio** (implementer, focused)
  - Goal: The demo script builds the renderer, refreshes build/demos, and commits the four showcase WAVs under artifacts/audio.
  - After: T3
  - Files: `tools/render-demos.sh`, `artifacts/audio/init.wav`, `artifacts/audio/sub-pure-foundation.wav`, `artifacts/audio/bass-hypnotic-roller.wav`, `artifacts/audio/rumble-dark-room.wav`
  - Check (test): `tools/render-demos.sh && tools/spec.sh 5 demos`

- **T8: Fix the bone number rendering and refresh review images** (implementer, integrative)
  - Goal: The editor draws the bone number at full size, keeps the last pitch during fade, and regenerates the review images.
  - After: T2
  - Files: `plugin/ui/Displays.h`, `plugin/ui/Displays.cpp`, `artifacts/ui/editor-idle.png`, `artifacts/ui/editor-playing.png`, `artifacts/ui/editor-clipping.png`
  - Check (test): `tools/spec.sh 5 number`
  - Check (test): `tools/spec.sh 4_2 head`
  - Check (test): `tools/spec.sh 4_3 finish`
  - Check (test): `tools/spec.sh 4 display 'P4.06*,P4.08*,P4.09*'`
  - Check (test): `tools/spec.sh 4 budget`

### Wave 4

- **T5: Implement the install script for test-folder deployment** (implementer, focused)
  - Goal: The install script stages signed VST3, AU, and app bundles into a destination folder without touching the user's system.
  - After: T2, T3, T4
  - Files: `tools/install.sh`
  - Check (test): `tools/install.sh --dest build/install-test && tools/spec.sh 5 install`


## Repairs

roko changed the planner's draft before it validated it:

- T1: `1-120` of `core/include/undertow/sequencer.h` (87 lines) became `1-87`
- T1: `1-80` of `core/CMakeLists.txt` (15 lines) became `1-15`
- T2: `1-220` of `plugin/PluginProcessor.h` (46 lines) became `1-46`
- T2: `1-260` of `plugin/PluginProcessor.cpp` (163 lines) became `1-163`
- T3: `1-80` of `CMakeLists.txt` (24 lines) became `1-24`
- T3: `1-120` of `core/include/undertow/filter.h` (45 lines) became `1-45`
- T3: `1-120` of `core/include/undertow/sequencer.h` (87 lines) became `1-87`
- T4: `260-520` of `docs/specs/phase-5-ship.md` (270 lines) became `260-270`
- T5: `520-820` of `docs/specs/phase-5-ship.md` (270 lines) became the whole file
- T5: `1-120` of `plugin/CMakeLists.txt` (42 lines) became `1-42`
- T6: `1-120` of `core/include/undertow/filter.h` (45 lines) became `1-45`
- T6: `1-220` of `core/src/filter.cpp` (123 lines) became `1-123`
- T7: `1-120` of `core/include/undertow/sequencer.h` (87 lines) became `1-87`
- T8: `1-220` of `plugin/ui/Displays.h` (53 lines) became `1-53`
- T8: `1-320` of `plugin/ui/Displays.cpp` (301 lines) became `1-301`
- T8: `820-980` of `docs/specs/phase-5-ship.md` (270 lines) became the whole file
- T1: reads all of `docs/specs/phase-5-ship.md`, the document its plan is written from, not only lines 1-260
- T2: reads `docs/specs/phase-5-ship.md`, the document its plan is written from
- T3: reads `docs/specs/phase-5-ship.md`, the document its plan is written from
- T4: reads all of `docs/specs/phase-5-ship.md`, the document its plan is written from, not only lines 260-270
- T6: reads `docs/specs/phase-5-ship.md`, the document its plan is written from
- T7: reads `docs/specs/phase-5-ship.md`, the document its plan is written from
- `[meta] sources` names `docs/specs/phase-5-ship.md`, the documents the plan is written from
- added `[[meta.verify]]` running `tools/check.sh --plugin`: the source names it the plan's final check (SC21)
- dropped context files that do not exist: `tools/render-demos.sh`
- clamped 1 context line range(s) to the end of their file

## Review edits (person)

Made by hermes-pilot on 2026-10-26 at 16:2x, as the person reviewing the plan, on the lead's go.
`tasks.toml` is otherwise as roko wrote it. The originals are in `tmp/hermes-roko-v7/pilot/phase5/` in the roko
checkout (`tasks.toml.as-written`, `plan.md.as-written`). The Tasks overview above was changed to match.

1. **T8 owns the three review images.**
   - `artifacts/ui/editor-idle.png`, `editor-playing.png` and `editor-clipping.png` are added to its files. The spec's
     notes give them to task 8, which commits what its `tools/spec.sh 4 budget` step writes.
   - It has 5 files now; integrative allows 10.
2. **T2 owns the strict pluginval report.** `artifacts/reports/pluginval-strict.txt` is added to its files. The notes
   say task 2 commits it, and its `tools/pluginval.sh 10 artifacts/reports/pluginval-strict.txt` step writes it.
3. **T4 and T8 read the spec first**, as the spec's notes ask.
   - `docs/specs/phase-5-ship.md` moves ahead of `tools/render/CMakeLists.txt` in T4's reads, and ahead of
     `plugin/ui/Displays.h` and `Displays.cpp` in T8's.
   - The reads and their ranges are otherwise unchanged.

plancheck (`tmp/hermes-roko-v7/pilot/tooling/plancheck.py`) went from **4 findings to 0**. Before, T8 lacked the three
PNGs, so no task owned them. Its NOTEs for T2's report and the two reads are gone too.
