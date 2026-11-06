---
plan: p5-3-low-pressure-as-named-and-the-roller-at-lev
title: "P5.3 · Low Pressure as named, and the Roller at level"
---

# P5.3 · Low Pressure as named, and the Roller at level

roko wrote this overview from the plan's `tasks.toml`, because the planner wrote no `plan.md`. Where the two differ, `tasks.toml` decides.

## Request

A chat host asked for this plan. Its request is not repeated here: it is untrusted data, and later prompts read this file.

## Tasks

1 task(s) in 1 wave(s). The tasks of a wave do not depend on each other; each task waits for the tasks it depends on.

### Wave 1

- **T1: Retune Low Pressure and lift the Roller render** (implementer, integrative)
  - Goal: SUB Low Pressure matches the phase 5.3 named sound, and BASS Hypnotic Roller renders at the reel's level.
  - Files: `core/src/presets.cpp`, `artifacts/audio/bass-hypnotic-roller.wav`
  - Check (test): `tools/spec.sh 5_3 presets`
  - Check (test): `tools/render-demos.sh && tools/spec.sh 5_3 demos`
  - Check (test): `tools/spec.sh 5_2 presets`
  - Check (test): `tools/spec.sh 5 presets`
  - Check (test): `tools/render-demos.sh && tools/spec.sh 5_1 demos`
  - Check (test): `tools/render-demos.sh && tools/spec.sh 5 demos`


## Repairs

roko changed the planner's draft before it validated it:

- T1: `1-145` of `docs/specs/phase-5.3-presets.md` (142 lines) became the whole file
- `[meta] sources` names `docs/specs/phase-5.3-presets.md`, the documents the plan is written from
