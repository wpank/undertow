---
plan: p3-undertow-hypnotic-sequencer-2
title: "P3 · Undertow hypnotic sequencer"
---

# P3 · Undertow hypnotic sequencer

roko wrote this overview from the plan's `tasks.toml`, because the planner wrote no `plan.md`. Where the two differ, `tasks.toml` decides.

## Request

A chat host asked for this plan. Its request is not repeated here: it is untrusted data, and later prompts read this file.

## Tasks

2 task(s) in 2 wave(s). The tasks of a wave do not depend on each other; each task waits for the tasks it depends on.

### Wave 1

- **T1: Implement the standalone sequencer** (implementer, architectural)
  - Goal: The core library provides a lock-free Sequencer with the documented steps, scales, default pattern, timing, mutation, and evolve behavior.
  - Files: `core/include/undertow/sequencer.h`, `core/src/sequencer.cpp`
  - Check (test): `tools/spec.sh 3 sequencer 'P3.01*,P3.02*'`
  - Check (test): `tools/spec.sh 3 sequencer 'P3.03*,P3.04*'`
  - Check (test): `tools/spec.sh 3 sequencer 'P3.05*,P3.06*'`
  - Check (test): `tools/spec.sh 3 sequencer 'P3.07*,P3.08*,P3.09*'`

### Wave 2

- **T2: Wire the sequencer into the engine and pattern handoff** (implementer, integrative)
  - Goal: The engine owns the sequencer, drives it from transport, and exposes a lock-free pattern handoff that returns the newest pattern without tearing.
  - After: T1
  - Files: `core/include/undertow/engine.h`, `core/src/engine.cpp`
  - Check (test): `tools/spec.sh 3 engine 'P3.10*,P3.11*'`
  - Check (test): `tools/spec.sh 3 engine 'P3.12*,P3.13*'`
  - Check (test): `tools/spec.sh 3 engine 'P3.14*,P3.15*'`


## Repairs

roko changed the planner's draft before it validated it:

- T1: `1-240` of `docs/specs/phase-3-sequencer.md` (214 lines) became `1-214`
- T1: `1-240` of `core/include/undertow/engine.h` (53 lines) became `1-53`
- T2: `1-240` of `docs/specs/phase-3-sequencer.md` (214 lines) became `1-214`
- T2: `1-260` of `tests/spec/p3/engine_test.cpp` (256 lines) became `1-256`
- T2: `1-240` of `core/include/undertow/engine.h` (53 lines) became `1-53`
- added `[[meta.verify]]` running `tools/check.sh`: the source names it the plan's final check (SC16)
- T1 step 1: `grep -q 'P3.01' tests/spec/p3/sequencer_test.cpp && tools/spec.sh 3 sequencer 'P3.01*,P3.02*'` became `tools/spec.sh 3 sequencer 'P3.01*,P3.02*'`, the source's own commands alone
- T1 step 2: `grep -q 'P3.03' tests/spec/p3/sequencer_test.cpp && tools/spec.sh 3 sequencer 'P3.03*,P3.04*'` became `tools/spec.sh 3 sequencer 'P3.03*,P3.04*'`, the source's own commands alone
- T1 step 3: `grep -q 'P3.05' tests/spec/p3/sequencer_test.cpp && tools/spec.sh 3 sequencer 'P3.05*,P3.06*'` became `tools/spec.sh 3 sequencer 'P3.05*,P3.06*'`, the source's own commands alone
- T1 step 4: `grep -q 'P3.07' tests/spec/p3/sequencer_test.cpp && tools/spec.sh 3 sequencer 'P3.07*,P3.08*,P3.09*'` became `tools/spec.sh 3 sequencer 'P3.07*,P3.08*,P3.09*'`, the source's own commands alone
- T2 step 1: `grep -q 'P3.10' tests/spec/p3/engine_test.cpp && tools/spec.sh 3 engine 'P3.10*,P3.11*'` became `tools/spec.sh 3 engine 'P3.10*,P3.11*'`, the source's own commands alone
- T2 step 2: `grep -q 'P3.12' tests/spec/p3/engine_test.cpp && tools/spec.sh 3 engine 'P3.12*,P3.13*'` became `tools/spec.sh 3 engine 'P3.12*,P3.13*'`, the source's own commands alone
- T2 step 3: `grep -q 'P3.14' tests/spec/p3/engine_test.cpp && tools/spec.sh 3 engine 'P3.14*,P3.15*'` became `tools/spec.sh 3 engine 'P3.14*,P3.15*'`, the source's own commands alone
