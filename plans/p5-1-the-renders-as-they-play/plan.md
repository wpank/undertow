---
plan: p5-1-the-renders-as-they-play
title: "P5.1 · the renders as they play"
---

# P5.1 · the renders as they play

roko wrote this overview from the plan's `tasks.toml`, because the planner wrote no `plan.md`. Where the two differ, `tasks.toml` decides.

## Request

A chat host asked for this plan. Its request is not repeated here: it is untrusted data, and later prompts read this file.

## Tasks

3 task(s) in 2 wave(s). The tasks of a wave do not depend on each other; each task waits for the tasks it depends on.

### Wave 1

- **T1: Retune RUMBLE Dark Room** (implementer, focused)
  - Goal: RUMBLE Dark Room passes the preset spectrum and level checks on the final filter.
  - Files: `core/src/presets.cpp`
  - Check (test): `tools/spec.sh 5 presets`
  - Check (test): `tools/spec.sh 5 filter`
  - Check (test): `tools/spec.sh 2_2 filter`
  - Check (test): `tools/spec.sh 2_1 filter`

- **T3: Draw every glyph whole and refresh the review images** (implementer, integrative)
  - Goal: Text drawing no longer truncates glyphs, and the review images are regenerated.
  - Files: `plugin/ui/Texts.h`, `plugin/ui/Displays.h`, `plugin/ui/Displays.cpp`, `artifacts/ui/editor-idle.png`, `artifacts/ui/editor-playing.png`, `artifacts/ui/editor-clipping.png`
  - Check (test): `tools/spec.sh 5_1 text`
  - Check (test): `tools/spec.sh 5 number`
  - Check (test): `tools/spec.sh 4_2 head`
  - Check (test): `tools/spec.sh 4_2 layout`
  - Check (test): `tools/spec.sh 4_3 finish`
  - Check (test): `tools/spec.sh 4 budget`

### Wave 2

- **T2: Make the offline renderer play like the engine** (implementer, integrative)
  - Goal: The offline renderer and demo script produce the same audio the engine would play.
  - After: T1
  - Files: `tools/render/main.cpp`, `tools/render-demos.sh`, `artifacts/audio/init.wav`, `artifacts/audio/sub-pure-foundation.wav`, `artifacts/audio/bass-hypnotic-roller.wav`, `artifacts/audio/rumble-dark-room.wav`
  - Check (test): `tools/spec.sh 5_1 render`
  - Check (test): `tools/render-demos.sh && tools/spec.sh 5_1 demos`
  - Check (test): `tools/spec.sh 5 render`
  - Check (test): `tools/render-demos.sh && tools/spec.sh 5 demos`


## Repairs

roko changed the planner's draft before it validated it:

- T1: `1-260` of `docs/specs/phase-5.1-renders.md` (164 lines) became `1-164`
- T1: `1-260` of `core/src/presets.cpp` (251 lines) became `1-251`
- T2: `1-260` of `docs/specs/phase-5.1-renders.md` (164 lines) became `1-164`
- T2: `1-260` of `tools/render/main.cpp` (240 lines) became `1-240`
- T2: `1-260` of `tools/render-demos.sh` (121 lines) became `1-121`
- T3: `1-260` of `docs/specs/phase-5.1-renders.md` (164 lines) became `1-164`
- T3: `1-120` of `plugin/ui/Texts.h` (26 lines) became `1-26`
- `[meta] sources` names `docs/specs/phase-5.1-renders.md`, the documents the plan is written from
- T1: `covers` SC1 became AC1
- T2: `covers` SC2 became AC1
- T2: `covers` SC3 became AC2
- T2: `covers` SC4 became AC3
- T2: `covers` SC5 became AC4
- T2: `covers` SC12 became AC5
- T2: `covers` SC11 became AC6
- T3: `covers` SC6 became AC1
- T3: `covers` SC7 became AC2
- T3: `covers` SC17 became AC3
- added `[[meta.verify]]` running `tools/check.sh --plugin`: the source names it the plan's final check (SC18)
- T2: added a step running `tools/render-demos.sh && tools/spec.sh 5 demos`, the source's own commands for SC12, which passes on the base too: the source calls it a regression check

## Review edits (person)

Made by hermes-pilot on 2026-10-26 at 17:5x, as the person reviewing the plan, on the lead's go (option (a)).
`tasks.toml` is otherwise as roko wrote it. The originals are in `tmp/hermes-roko-v7/pilot/phase5-1/` in the roko
checkout (`tasks.toml.as-written`, `plan.md.as-written`). The Tasks overview above was changed to match.

1. **T1 runs its three filter regression checks** and is focused (`max_loc` 50).
   - The three are P5.1.08 `tools/spec.sh 5 filter`, P5.1.09 `tools/spec.sh 2_2 filter` and P5.1.10
     `tools/spec.sh 2_1 filter`, each its own step with `expect = "pass_on_base"`, under AC2 to AC4 (the spec's rows).
   - The draft roko wrote had left them out.
   - roko's coverage re-ask had put them back, but it was "not kept … PLAN_TIER_SIZE [T1]: mechanical task is over its
     tier's size limits: 4 verify steps (limit 3) … raise its tier to focused".
   - So T1 is raised from mechanical to focused, which allows 4 steps.
2. **T3 runs its four regression checks** ahead of `tools/spec.sh 4 budget`.
   - The four are P5.1.13 `tools/spec.sh 5 number`, P5.1.14 `tools/spec.sh 4_2 head`, P5.1.15
     `tools/spec.sh 4_2 layout` and P5.1.16 `tools/spec.sh 4_3 finish`, each its own step with
     `expect = "pass_on_base"`, under AC4 to AC7.
   - That makes 6 steps; integrative allows 6.

plancheck (`tmp/hermes-roko-v7/pilot/tooling/plancheck.py`) went from **1 finding to 0**. Before, spec commands ran
exactly 10 of 17.
