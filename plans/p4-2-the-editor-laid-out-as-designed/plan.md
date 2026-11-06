---
plan: p4-2-the-editor-laid-out-as-designed
title: "P4.2 · the editor laid out as designed"
---

# P4.2 · the editor laid out as designed

roko wrote this overview from the plan's `tasks.toml`, because the planner wrote no `plan.md`. Where the two differ, `tasks.toml` decides.

## Request

A chat host asked for this plan. Its request is not repeated here: it is untrusted data, and later prompts read this file.

## Tasks

3 task(s) in 2 wave(s). The tasks of a wave do not depend on each other; each task waits for the tasks it depends on.

### Wave 1

- **T1: Lay out controls, labels, stamps, and text recording** (implementer, architectural)
  - Goal: The editor exposes the designed control layout, labels, stamps, and recorded text boxes so the layout spec checks pass.
  - Files: `plugin/ui/Theme.h`, `plugin/ui/Texts.h`, `plugin/PluginEditor.h`, `plugin/PluginEditor.cpp`, `plugin/ui/Look.h`, `plugin/ui/Look.cpp`
  - Check (test): `tools/spec.sh 4_2 layout`
  - Check (test): `tools/spec.sh 4 editor`
  - Check (test): `tools/spec.sh 4 display 'P4.05*'`
  - Check (test): `tools/spec.sh 4 steps`

- **T2: Restore the default sequencer pattern on new engines** (implementer, focused)
  - Goal: A new engine starts with defaultPattern in the sequencer itself so playback, pattern readback, and saved plugin state all reflect the default bassline.
  - Files: `core/src/engine.cpp`
  - Check (test): `tools/spec.sh 4_2 pattern`
  - Check (test): `tools/spec.sh 4 state`
  - Check (test): `tools/spec.sh 3 engine 'P3.14*,P3.15*'`

### Wave 2

- **T3: Redraw the HEAD, bone pitch display, and commit refreshed review images** (implementer, integrative)
  - Goal: The HEAD and CHEST number match the design, all their text is recorded through texts::draw, and the refreshed review images are committed.
  - After: T1, T2
  - Files: `plugin/ui/Status.h`, `plugin/ui/Status.cpp`, `plugin/ui/Displays.h`, `plugin/ui/Displays.cpp`, `artifacts/ui/editor-clipping.png`, `artifacts/ui/editor-idle.png`, `artifacts/ui/editor-playing.png`
  - Check (test): `tools/spec.sh 4_2 head`
  - Check (test): `tools/spec.sh 4 display 'P4.06*,P4.08*,P4.09*'`
  - Check (test): `tools/spec.sh 4 display 'P4.07*'`
  - Check (test): `tools/spec.sh 4 budget`


## Repairs

roko changed the planner's draft before it validated it:

- T1: `1-220` of `docs/specs/phase-4.2-layout.md` (211 lines) became `1-211`
- T1: `1-180` of `plugin/PluginEditor.cpp` (175 lines) became `1-175`
- T2: `1-220` of `docs/specs/phase-4.2-layout.md` (211 lines) became `1-211`
- T3: `1-220` of `docs/specs/phase-4.2-layout.md` (211 lines) became `1-211`
- T3: `1-136` of `plugin/ui/Status.cpp` (116 lines) became `1-116`
- T3: `1-278` of `plugin/ui/Displays.cpp` (241 lines) became `1-241`
- `[meta] sources` names `docs/specs/phase-4.2-layout.md`, the documents the plan is written from
