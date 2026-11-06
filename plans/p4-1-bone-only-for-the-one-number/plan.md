---
plan: p4-1-bone-only-for-the-one-number
title: "P4.1 · bone only for the one number"
---

# P4.1 · bone only for the one number

roko wrote this overview from the plan's `tasks.toml`, because the planner wrote no `plan.md`. Where the two differ, `tasks.toml` decides.

## Request

A chat host asked for this plan. Its request is not repeated here: it is untrusted data, and later prompts read this file.

## Tasks

2 task(s) in 2 wave(s). The tasks of a wave do not depend on each other; each task waits for the tasks it depends on.

### Wave 1

- **T1: Restrict bone tokens to the CHEST pitch display** (implementer, focused)
  - Goal: Only the CHEST pitch display keeps bone and bone_dim; the knob tick, knob thumb, tooltip text, BPM readout, and RUMBLE label use the ROSEDUST tokens named by the phase 4.1 spec.
  - Files: `plugin/ui/Look.cpp`, `plugin/ui/Status.cpp`
  - Check (test): `tools/spec.sh 4 display 'P4.06*,P4.08*,P4.09*'`
  - Check (test): `tools/spec.sh 4 display 'P4.05*'`
  - Check (test): `tools/spec.sh 4 display 'P4.07*'`

### Wave 2

- **T2: Refresh and commit the phase 4.1 review PNGs** (implementer, mechanical)
  - Goal: The committed 1x review PNGs in artifacts/ui are regenerated from the corrected editor colours by the phase 4 budget run.
  - After: T1
  - Files: `artifacts/ui/editor-idle.png`, `artifacts/ui/editor-playing.png`, `artifacts/ui/editor-clipping.png`
  - Check (test): `tools/spec.sh 4 budget`


## Repairs

roko changed the planner's draft before it validated it:

- T1: `1-95` of `docs/specs/phase-4.1-bone.md` (89 lines) became `1-89`
- T2: `33-95` of `docs/specs/phase-4.1-bone.md` (89 lines) became `33-89`
- T2: reads all of `docs/specs/phase-4.1-bone.md`, the document its plan is written from, not only lines 33-89
- `[meta] sources` names `docs/specs/phase-4.1-bone.md`, the documents the plan is written from
