---
plan: p2-undertow-rumble-4
title: "P2 · Undertow rumble"
---

# P2 · Undertow rumble

roko wrote this overview from the plan's `tasks.toml`, because the planner wrote no `plan.md`. Where the two differ, `tasks.toml` decides.

## Request

A chat host asked for this plan. Its request is not repeated here: it is untrusted data, and later prompts read this file.

## Tasks

4 task(s) in 3 wave(s). The tasks of a wave do not depend on each other; each task waits for the tasks it depends on.

### Wave 1

- **T1: Add the LFO module and switch core sources to glob** (implementer, integrative)
  - Goal: The core library builds from a globbed source list, and the new LFO API exists with the specified beat-locked shapes.
  - Files: `core/CMakeLists.txt`, `core/include/undertow/lfo.h`, `core/src/lfo.cpp`
  - Check (compile): `grep -q 'file(GLOB' core/CMakeLists.txt && cmake --preset core && cmake --build --preset core`
  - Check (test): `tools/spec.sh 2 lfo 'P2.01*'`
  - Check (test): `tools/spec.sh 2 lfo 'P2.02*'`
  - Check (test): `tools/spec.sh 2 lfo 'P2.03*'`

### Wave 2

- **T2: Implement the rumble generator and ducked reverb** (implementer, architectural)
  - Goal: The rumble module produces the beat-synced ghost kick, reverb tail, ducking, and safe bounded output.
  - After: T1
  - Files: `core/include/undertow/rumble.h`, `core/src/rumble.cpp`, `core/CMakeLists.txt`
  - Check (test): `tools/spec.sh 2 rumble 'P2.04*'`
  - Check (test): `tools/spec.sh 2 rumble 'P2.05*'`
  - Check (test): `tools/spec.sh 2 rumble 'P2.07*' && tools/spec.sh 2 rumble 'P2.08*'`
  - Check (test): `tools/spec.sh 2 rumble 'P2.06*'`

- **T3: Add the output stage and its soft clipper** (implementer, architectural)
  - Goal: The output stage performs DC blocking, saturation, crossover width processing, master gain smoothing, and soft clipping.
  - After: T1
  - Files: `core/include/undertow/output_stage.h`, `core/src/output_stage.cpp`, `core/CMakeLists.txt`
  - Check (test): `tools/spec.sh 2 output 'P2.09*,P2.10*'`
  - Check (test): `tools/spec.sh 2 output 'P2.11*,P2.12*'`
  - Check (test): `tools/spec.sh 2 output 'P2.20*,P2.21*'`

### Wave 3

- **T4: Wire voice modulation and the engine signal flow** (implementer, integrative)
  - Goal: The engine applies voice modulation, routes LFOs, rumble, and output-stage processing, and updates meters and defaults.
  - After: T1, T2, T3
  - Files: `core/include/undertow/voice.h`, `core/src/voice.cpp`, `core/src/engine.cpp`, `core/src/params.cpp`
  - Check (test): `tools/spec.sh 2 engine 'P2.13*,P2.14*,P2.23*'`
  - Check (test): `tools/spec.sh 2 engine 'P2.15*,P2.16*'`
  - Check (test): `tools/spec.sh 2 engine 'P2.17*,P2.22*'`
  - Check (test): `tools/spec.sh 2 engine 'P2.18*,P2.19*'`

