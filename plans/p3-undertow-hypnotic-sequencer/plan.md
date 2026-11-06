---
plan: p3-undertow-hypnotic-sequencer
title: "P3 · Undertow hypnotic sequencer"
---

# P3 · Undertow hypnotic sequencer

roko wrote this overview from the plan's `tasks.toml`, because the planner wrote no `plan.md`. Where the two differ, `tasks.toml` decides.

## Request

A chat host asked for this plan. Its request is not repeated here: it is untrusted data, and later prompts read this file.

## Tasks

2 task(s) in 2 wave(s). The tasks of a wave do not depend on each other; each task waits for the tasks it depends on.

### Wave 1

- **T1: Implement the sequencer core** (implementer, architectural)
  - Goal: The project exposes a working 16-step sequencer API with default pattern, scales, processing, mutation, and evolve behavior.
  - Files: `core/include/undertow/sequencer.h`, `core/src/sequencer.cpp`
  - Check (test): `tools/spec.sh 3 sequencer 'P3.01*,P3.02*'`
  - Check (test): `tools/spec.sh 3 sequencer 'P3.03*,P3.04*'`
  - Check (test): `tools/spec.sh 3 sequencer 'P3.05*,P3.06*'`
  - Check (test): `tools/spec.sh 3 sequencer 'P3.07*,P3.08*,P3.09*'`

### Wave 2

- **T2: Add sequencer state handoff and engine wiring** (implementer, integrative)
  - Goal: The engine owns a sequencer, exposes pattern handoff APIs, starts from the default pattern, and drives the sequencer from the audio thread.
  - After: T1
  - Files: `core/include/undertow/engine.h`, `core/src/engine.cpp`
  - Check (test): `tools/spec.sh 3 engine 'P3.10*,P3.11*'`
  - Check (test): `tools/spec.sh 3 engine 'P3.12*,P3.13*'`
  - Check (test): `tools/spec.sh 3 engine 'P3.14*,P3.15*'`

