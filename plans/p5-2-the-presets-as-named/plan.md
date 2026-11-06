---
plan: p5-2-the-presets-as-named
title: "P5.2 · the presets as named"
---

# P5.2 · the presets as named

roko wrote this overview from the plan's `tasks.toml`, because the planner wrote no `plan.md`. Where the two differ, `tasks.toml` decides.

## Request

A chat host asked for this plan. Its request is not repeated here: it is untrusted data, and later prompts read this file.

## Tasks

1 task(s) in 1 wave(s). The tasks of a wave do not depend on each other; each task waits for the tasks it depends on.

### Wave 1

- **T1: Retune the six named presets and refresh the demos** (implementer, integrative)
  - Goal: The six named presets satisfy the phase 5.2 audio rules, and the regenerated showcase renders stay in sync.
  - Files: `core/src/presets.cpp`, `artifacts/audio/sub-pure-foundation.wav`, `artifacts/audio/bass-hypnotic-roller.wav`, `artifacts/audio/rumble-dark-room.wav`, `artifacts/audio/init.wav`
  - Check (test): `tools/spec.sh 5_2 presets`
  - Check (test): `tools/spec.sh 5 presets`
  - Check (test): `tools/spec.sh 5_1 render`
  - Check (test): `tools/render-demos.sh && tools/spec.sh 5_1 demos`
  - Check (test): `tools/render-demos.sh && tools/spec.sh 5 demos`


## Repairs

roko changed the planner's draft before it validated it:

- T1: `1-140` of `docs/specs/phase-5.2-presets.md` (139 lines) became the whole file
- `[meta] sources` names `docs/specs/phase-5.2-presets.md`, the documents the plan is written from
- T1: `covers` SC1 became AC6
- T1: `covers` SC2 became AC7
- T1: `covers` SC3 became AC8
- T1: `covers` SC4 became AC9
- T1: `covers` SC5 became AC10
- T1: `covers` SC6 became AC11
- T1: `covers` SC7 became AC12
- T1: `covers` SC8 became AC13

## Review edits (person)

Made by hermes-pilot on 2026-10-26 at 22:5x, as the person reviewing the plan, on the lead's go (option (a)).
`tasks.toml` is otherwise as roko wrote it. The originals are in `tmp/hermes-roko-v7/pilot/phase5-2/` in the roko
checkout (`tasks.toml.as-written`, `plan.md.as-written`).

1. **T1's acceptance criteria AC1 to AC5 are covered by the steps whose commands they name.**
   - The planner wrote AC1 to AC5 as its own restatements of the five commands, ahead of the spec's checks AC6 to
     AC13. For example, "AC2: `tools/spec.sh 5 presets` still passes on the base preset suite."
   - No step covered them, so `roko plan validate --strict` stopped on five PLAN_044 warnings.
   - Each now sits in the covers of the step running the command it quotes:
     - AC1 in step 1 (`tools/spec.sh 5_2 presets`, with AC6 to AC9);
     - AC2 in step 2 (`tools/spec.sh 5 presets`, AC10);
     - AC3 in step 3 (`tools/spec.sh 5_1 render`, AC11);
     - AC4 in step 4 (`tools/render-demos.sh && tools/spec.sh 5_1 demos`, AC12);
     - AC5 in step 5 (`tools/render-demos.sh && tools/spec.sh 5 demos`, AC13).
   - No command, expectation, file or tier changed. The agent keeps the planner's "still passes on the base" wording.
