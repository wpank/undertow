---
plan: p4-undertow-phosphor-editor
title: "P4 · Undertow phosphor editor"
---

# P4 · Undertow phosphor editor

roko wrote this overview from the plan's `tasks.toml`, because the planner wrote no `plan.md`. Where the two differ, `tasks.toml` decides.

## Request

A chat host asked for this plan. Its request is not repeated here: it is untrusted data, and later prompts read this file.

## Tasks

7 task(s) in 5 wave(s). The tasks of a wave do not depend on each other; each task waits for the tasks it depends on.

### Wave 1

- **T1: Add scope, clock, and pattern-state plumbing** (implementer, integrative)
  - Goal: The engine exposes lock-free scope samples and clock fields, and the processor persists patterns without losing them across prepare or reset.
  - Files: `core/include/undertow/engine.h`, `core/src/engine.cpp`, `plugin/PluginProcessor.h`, `plugin/PluginProcessor.cpp`, `plugin/CMakeLists.txt`
  - Check (test): `tools/spec.sh 4 scope`
  - Check (test): `tools/spec.sh 4 state`

### Wave 2

- **T2: Build the editor frame, capture pipeline, and widget stubs** (implementer, architectural)
  - Goal: The plugin editor exposes the full phase 4 API, captures frames from the processor, and compiles with stub widget headers that later tasks replace.
  - After: T1
  - Files: `plugin/ui/UiFrame.h`, `plugin/ui/Theme.h`, `plugin/PluginEditor.h`, `plugin/PluginEditor.cpp`, `plugin/ui/Look.h`, `plugin/ui/UnitArray.h`, `plugin/ui/Displays.h`, `plugin/ui/Status.h`
  - Check (test): `tools/spec.sh 4 editor 'P4.03*'`

### Wave 3

- **T3: Add and attach all parameter controls** (implementer, integrative)
  - Goal: The editor owns one attached control per parameter and `controlFor` returns each control by parameter id.
  - After: T2
  - Files: `plugin/ui/Theme.h`, `plugin/PluginEditor.h`, `plugin/PluginEditor.cpp`
  - Check (test): `tools/spec.sh 4 editor 'P4.04*'`

- **T4: Implement the editable unit array** (implementer, integrative)
  - Goal: The GUT unit array draws and edits all 16 sequencer steps with correct bounds, hit-testing, highlight, and pattern mutation.
  - After: T2
  - Files: `plugin/ui/UnitArray.h`, `plugin/ui/UnitArray.cpp`, `plugin/CMakeLists.txt`
  - Check (test): `tools/spec.sh 4 steps`

- **T6: Render the CHEST tunnel, scope, AT field, and bone number** (implementer, architectural)
  - Goal: The CHEST display animates the tunnel, scope, AT field, and pitch readout so note, energy, and waveform appear in the required place and style.
  - After: T2
  - Files: `plugin/ui/Displays.h`, `plugin/ui/Displays.cpp`, `plugin/CMakeLists.txt`
  - Check (test): `tools/spec.sh 4 display 'P4.06*,P4.08*,P4.09*'`

### Wave 4

- **T5: Implement the ROSEDUST look and cached static layer** (implementer, integrative)
  - Goal: The editor installs a custom ROSEDUST LookAndFeel and paints the static layer so the panel has the required palette, void ratio, and calm idle CHEST.
  - After: T3
  - Files: `plugin/ui/Look.h`, `plugin/ui/Look.cpp`, `plugin/CMakeLists.txt`
  - Check (test): `tools/spec.sh 4 display 'P4.05*'`

### Wave 5

- **T7: Implement status displays, clipping hazards, and commit review images** (implementer, integrative)
  - Goal: The editor renders the HEAD and rumble status displays, shows hazard stripes only while clipping, and produces the committed review images within the frame budget.
  - After: T3, T4, T5, T6
  - Files: `plugin/ui/Status.h`, `plugin/ui/Status.cpp`, `artifacts/ui/editor-idle.png`, `artifacts/ui/editor-playing.png`, `artifacts/ui/editor-clipping.png`, `plugin/CMakeLists.txt`
  - Check (test): `tools/spec.sh 4 display 'P4.07*'`
  - Check (test): `tools/spec.sh 4 budget`


## Repairs

roko changed the planner's draft before it validated it:

- T2: `1-13` of `plugin/PluginEditor.h` (12 lines) became `1-12`
- T1: `covers` SC1 became AC3
- T1: `covers` SC2 became AC4
- T2: `covers` SC3 became AC2
- T3: `covers` SC4 became AC2
- T4: `covers` SC10 became AC2
- T5: `covers` SC5 became AC2
- T6: `covers` SC6 became AC2
- T6: `covers` SC8 became AC3
- T6: `covers` SC9 became AC4
- T7: `covers` SC7 became AC3
- T7: `covers` SC11 became AC4
- T4: added `plugin/CMakeLists.txt` to its files: it creates `plugin/ui/UnitArray.cpp`, which that CMake file lists
- T5: added `plugin/CMakeLists.txt` to its files: it creates `plugin/ui/Look.cpp`, which that CMake file lists
- T6: added `plugin/CMakeLists.txt` to its files: it creates `plugin/ui/Displays.cpp`, which that CMake file lists
- T7: added `plugin/CMakeLists.txt` to its files: it creates `plugin/ui/Status.cpp`, which that CMake file lists

## Review edits (person)

Made by hermes-pilot on 2026-10-21 at 07:2x, as the person reviewing the plan, on the lead's go (option (a)).
`tasks.toml` is otherwise as roko wrote it. The original is in
`tmp/hermes-roko-v7/pilot/phase4/tasks.toml.as-written` in the roko checkout.

1. **`plugin/CMakeLists.txt` dropped from the files of T4, T5, T6 and T7.**
   - roko's repair added it to each of them (the four "added `plugin/CMakeLists.txt` to its files" lines under
     Repairs). It read the base file, which names its sources one by one.
   - T1 owns that file and adds the `file(GLOB … "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.cpp")` the spec asks for, so a
     widget's `.cpp` needs no entry.
   - The spec's notes say no file has more than two owners, and give this one to task 1 alone. T4 and T6 run side by
     side, so sharing it risked an accept clash.
   - undertow-spec's reference passes with T4–T7 never touching the file.
2. **Every task reads the whole spec:** `docs/specs/phase-4-ui.md` with `lines = "1-264"`, for T1–T7.
   - The planner gave T4–T7 ranges that left out their own "What to build" item entirely:

     | Task | Range | Its item |
     |---|---|---|
     | T4 | 86-179 | 81-85 |
     | T5 | 97-179 | 87-92 |
     | T6 | 105-179 | 94-96 |
     | T7 | 129-223 | 98-106 |

   - T3 (63-179) also missed the rules (212-219), and T1 and T2 (1-223) missed the planner notes.
   - The range is explicit rather than left out because a read without a range is inlined only to line 100
     (`context_provider.rs`, `add_inline_files`).

plancheck (`tmp/hermes-roko-v7/pilot/tooling/plancheck.py`) went from **6 findings to 0**:
- Before: `plugin/CMakeLists.txt` had 5 owners (T1, T4, T5, T6, T7), and T4–T7's spec reads left out their own item.
- After: every file's owners are the notes' (20 files, 7 with two owners), and every task reads its own item.
`roko plan validate` reports 0 diagnostics before and after, and `--strict` 0 after.
