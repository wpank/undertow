---
plan: p5-5-auval-clean
title: "P5.5 · auval clean"
---

# P5.5 · auval clean

roko wrote this overview from the plan's `tasks.toml`, because the planner wrote no `plan.md`. Where the two differ, `tasks.toml` decides.

## Request

A chat host asked for this plan. Its request is not repeated here: it is untrusted data, and later prompts read this file.

## Tasks

1 task(s) in 1 wave(s). The tasks of a wave do not depend on each other; each task waits for the tasks it depends on.

### Wave 1

- **T1: Make Seq Gate auval-clean** (implementer, architectural)
  - Goal: The plugin's float parameter layout gives Seq Gate a 0.001 grid so auval stops warning, while every host-visible id, range end, and default stays unchanged.
  - Files: `plugin/PluginProcessor.cpp`
  - Check (test): `tools/spec.sh 5_5 auval`
  - Check (test): `tools/spec.sh 0 params`
  - Check (test): `tools/spec.sh 0 plugin`
  - Check (test): `tools/spec.sh 5 programs`
  - Check (test): `tools/spec.sh 4 state`
  - Check (test): `tools/spec.sh 4_2 pattern`
  - Check (test): `tools/spec.sh 4_2 layout`


## Repairs

roko changed the planner's draft before it validated it:

- T1: reads `docs/specs/phase-5.5-auval.md`, the document its plan is written from
- `[meta] sources` names `docs/specs/phase-5.5-auval.md`, the documents the plan is written from
- T1: reads `plugin/PluginProcessor.h`, the header of `plugin/PluginProcessor.cpp`, which it changes
- T1: reads `tests/spec/p5_5/auval_test.cpp`, the test its check `tools/spec.sh 5_5 auval` runs
