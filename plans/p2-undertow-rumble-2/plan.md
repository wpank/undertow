---
plan: p2-undertow-rumble-2
title: "P2 · Undertow rumble"
---

# P2 · Undertow rumble

roko wrote this overview from the plan's `tasks.toml`, because the planner wrote no `plan.md`. Where the two differ, `tasks.toml` decides.

## Request

A chat host asked for this plan. Its request is not repeated here: it is untrusted data, and later prompts read this file.

## Tasks

4 task(s) in 3 wave(s). The tasks of a wave do not depend on each other; each task waits for the tasks it depends on.

### Wave 1

- **T1: Add stateless LFO utilities and glob core sources** (implementer, integrative)
  - Goal: The core library exports the LFO API and builds it from a globbed source list.
  - Files: `core/CMakeLists.txt`, `core/include/undertow/lfo.h`, `core/src/lfo.cpp`
  - Check (compile): `test -f core/include/undertow/lfo.h && test -f core/src/lfo.cpp && grep -q 'file(GLOB' core/CMakeLists.txt && grep -q 'CONFIGURE_DEPENDS' core/CMakeLists.txt && cargo check -p undertow_core`

### Wave 2

- **T2: Implement rumble generation and ducking** (implementer, integrative)
  - Goal: The core library exposes a working rumble stage with beat-synced ducking and bounded output.
  - After: T1
  - Files: `core/include/undertow/rumble.h`, `core/src/rumble.cpp`
  - Check (test): `test -f core/include/undertow/rumble.h && test -f core/src/rumble.cpp && grep -q 'class Rumble' core/include/undertow/rumble.h && cargo test -p undertow_core rumble`

- **T3: Implement output stage and clip meter** (implementer, integrative)
  - Goal: The core library exposes a complete output stage with crossover, widening, soft clipping, and clipping state.
  - After: T1
  - Files: `core/include/undertow/output_stage.h`, `core/src/output_stage.cpp`
  - Check (test): `test -f core/include/undertow/output_stage.h && test -f core/src/output_stage.cpp && grep -q 'softClip' core/include/undertow/output_stage.h && cargo test -p undertow_core output_stage`

### Wave 3

- **T4: Add voice modulation and wire the engine** (implementer, architectural)
  - Goal: The voice accepts modulation input and the engine routes LFOs, rumble, and output-stage processing per sample.
  - After: T1, T2, T3
  - Files: `core/include/undertow/voice.h`, `core/src/voice.cpp`, `core/src/engine.cpp`
  - Check (compile): `cargo check --workspace`

