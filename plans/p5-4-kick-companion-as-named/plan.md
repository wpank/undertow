---
plan: p5-4-kick-companion-as-named
title: "P5.4 · Kick Companion as named"
---

# P5.4 · Kick Companion as named

roko wrote this overview from the plan's `tasks.toml`, because the planner wrote no `plan.md`. Where the two differ, `tasks.toml` decides.

## Request

A chat host asked for this plan. Its request is not repeated here: it is untrusted data, and later prompts read this file.

## Tasks

1 task(s) in 1 wave(s). The tasks of a wave do not depend on each other; each task waits for the tasks it depends on.

### Wave 1

- **T1: Retune SUB Kick Companion** (implementer, architectural)
  - Goal: SUB Kick Companion becomes the short, clean sub note described for phase 5.4, without changing any other preset or artifact.
  - Files: `core/src/presets.cpp`
  - Check (test): `tools/spec.sh 5_4 presets`
  - Check (test): `tools/spec.sh 5 presets`
  - Check (test): `tools/spec.sh 5_1 render`
  - Check (test): `tools/spec.sh 5_3 presets`
  - Check (test): `tools/spec.sh 5_2 presets`
  - Check (test): `tools/render-demos.sh && tools/spec.sh 5_1 demos`
  - Check (test): `tools/render-demos.sh && tools/spec.sh 5 demos`


## Repairs

roko changed the planner's draft before it validated it:

- T1: reads `docs/specs/phase-5.4-presets.md`, the document its plan is written from
- `[meta] sources` names `docs/specs/phase-5.4-presets.md`, the documents the plan is written from
- T1: tier integrative became architectural, to fit its 7 verify steps (limit 6)
- T1: reads `core/include/undertow/presets.h`, the header of `core/src/presets.cpp`, which it changes
- T1: reads `tests/spec/p5_4/presets_test.cpp`, the test its check `tools/spec.sh 5_4 presets` runs
