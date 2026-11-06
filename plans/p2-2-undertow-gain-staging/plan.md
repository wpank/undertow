---
plan: p2-2-undertow-gain-staging
title: "P2.2 · Undertow gain staging"
---

# P2.2 · Undertow gain staging

roko wrote this overview from the plan's `tasks.toml`, because the planner wrote no `plan.md`. Where the two differ, `tasks.toml` decides.

## Request

A chat host asked for this plan. Its request is not repeated here: it is untrusted data, and later prompts read this file.

## Tasks

3 task(s) in 1 wave(s). The tasks of a wave do not depend on each other; each task waits for the tasks it depends on.

### Wave 1

- **T1: Rework output-stage width staging** (implementer, focused)
  - Goal: Width preserves the mono peak shape while adding stereo decorrelation at the side.
  - Files: `core/include/undertow/output_stage.h`, `core/src/output_stage.cpp`
  - Check (test): `tools/spec.sh 2_2 output 'P2.2.01*,P2.2.02*'`
  - Check (test): `tools/spec.sh 2 output`
  - Check (test): `tools/spec.sh 2 engine 'P2.15*,P2.16*'`
  - Check (test): `tools/spec.sh 2 engine 'P2.17*,P2.22*'`

- **T2: Retune filter drive and resonance** (implementer, mechanical)
  - Goal: The filter's drive adds makeup gain and the resonance curve has a stronger middle without breaking full-level linearity.
  - Files: `core/include/undertow/filter.h`, `core/src/filter.cpp`
  - Check (test): `tools/spec.sh 2_2 filter 'P2.2.03*,P2.2.11*,P2.2.13*'`
  - Check (test): `tools/spec.sh 1 filter`
  - Check (test): `tools/spec.sh 2_1 filter`

- **T3: Boost rumble makeup and output level** (implementer, mechanical)
  - Goal: The rumble keeps its drive level while the default mix sits audibly under the bass.
  - Files: `core/include/undertow/rumble.h`, `core/src/rumble.cpp`
  - Check (test): `tools/spec.sh 2_2 rumble 'P2.2.04*,P2.2.05*'`
  - Check (test): `tools/spec.sh 2 rumble`

