---
plan: p4-3-the-editor-s-finish
title: "P4.3 · the editor's finish"
---

# P4.3 · the editor's finish

roko wrote this overview from the plan's `tasks.toml`, because the planner wrote no `plan.md`. Where the two differ, `tasks.toml` decides.

## Request

A chat host asked for this plan. Its request is not repeated here: it is untrusted data, and later prompts read this file.

## Tasks

2 task(s) in 2 wave(s). The tasks of a wave do not depend on each other; each task waits for the tasks it depends on.

### Wave 1

- **T1: Fit the limb panels to the mock** (implementer, integrative)
  - Goal: The limb panels in Theme.h place every control and label in the documented rows and columns.
  - Files: `plugin/ui/Theme.h`
  - Check (test): `tools/spec.sh 4_3 panels`
  - Check (test): `tools/spec.sh 4_2 layout`
  - Check (test): `tools/spec.sh 4 editor`
  - Check (test): `tools/spec.sh 4 display 'P4.05*'`
  - Check (test): `tools/spec.sh 4 steps`

### Wave 2

- **T2: Repair the status head, clip bands, number, and pump field** (implementer, integrative)
  - Goal: The editor finish in Status.cpp, Displays.cpp, and PluginEditor.cpp draws the exact words, clip bands, number layout, and rumble pump field required by the finish checks.
  - After: T1
  - Files: `plugin/ui/Status.h`, `plugin/ui/Status.cpp`, `plugin/ui/Displays.cpp`, `plugin/PluginEditor.cpp`, `artifacts/ui/editor-idle.png`, `artifacts/ui/editor-playing.png`, `artifacts/ui/editor-clipping.png`
  - Check (test): `tools/spec.sh 4_3 finish`
  - Check (test): `tools/spec.sh 4_2 head`
  - Check (test): `tools/spec.sh 4 display 'P4.06*,P4.08*,P4.09*'`
  - Check (test): `tools/spec.sh 4 display 'P4.07*'`
  - Check (test): `tools/spec.sh 4 budget`


## Repairs

roko changed the planner's draft before it validated it:

- T1: `1-220` of `docs/specs/phase-4.3-finish.md` (164 lines) became `1-164`
- T1: `1-260` of `plugin/ui/Theme.h` (259 lines) became `1-259`
- T1: `1-220` of `tests/spec/p4_3/panels_test.cpp` (175 lines) became `1-175`
- T2: `1-220` of `docs/specs/phase-4.3-finish.md` (164 lines) became `1-164`
- T2: `1-220` of `plugin/ui/Status.cpp` (120 lines) became `1-120`
- T2: `1-260` of `plugin/ui/Displays.cpp` (254 lines) became `1-254`
- `[meta] sources` names `docs/specs/phase-4.3-finish.md`, the documents the plan is written from
- added `[[meta.verify]]` running `tools/check.sh --plugin`: the source names it the plan's final check (SC17)

## Review edits (person)

Made by hermes-pilot on 2026-10-26 at 15:2x, as the person reviewing the plan, on the lead's go (option (a)).
`tasks.toml` is otherwise as roko wrote it. The original is in
`tmp/hermes-roko-v7/pilot/phase4-3/tasks.toml.as-written` in the roko checkout (`plan.md.as-written` beside it).
The Tasks overview above was changed to match.

1. **T1 runs its four regression checks** and is integrative.
   - The four checks are P4.3.09 `tools/spec.sh 4_2 layout`, P4.3.10 `tools/spec.sh 4 editor`, P4.3.11
     `tools/spec.sh 4 display 'P4.05*'` and P4.3.12 `tools/spec.sh 4 steps`. Each is its own step with
     `expect = "pass_on_base"`, as the spec's notes give them to task 1, under a new AC2.
   - The draft roko wrote had left them out.
   - roko's coverage re-ask had put them back, but its regeneration was not kept: "PLAN_TIER_SIZE [T1]: focused task
     is over its tier's size limits: 5 verify steps (limit 4) … or raise its tier to integrative".
   - So T1 is raised from focused to integrative (`max_loc` 150), which allows 6 steps.
2. **T2's regression checks run one command per step, P4.07's included.**
   - roko had joined P4.3.13, P4.3.14 and P4.3.16 into one `&&` step and left P4.3.15 out.
   - The four are now separate steps, each with `expect = "pass_on_base"`: `tools/spec.sh 4_2 head`,
     `tools/spec.sh 4 display 'P4.06*,P4.08*,P4.09*'`, `tools/spec.sh 4 display 'P4.07*'` and
     `tools/spec.sh 4 budget`. Its AC2 names all four.
3. **T2 owns the three review images.** `artifacts/ui/editor-idle.png`, `editor-playing.png` and `editor-clipping.png`
   are added to its files, as the spec's notes give them to task 2, which commits what its budget step writes.

plancheck (`tmp/hermes-roko-v7/pilot/tooling/plancheck.py`) went from **5 findings to 0**. Before: spec commands
run exactly 11 of 16 (P4.3.09–P4.3.12 and P4.3.15 missing), and the three PNGs were owned by no task.
