---
plan: p2-undertow-rumble-3
title: "P2 · Undertow rumble"
---

# P2 · Undertow rumble

roko wrote this overview from the plan's `tasks.toml`, because the planner wrote no `plan.md`. Where the two differ, `tasks.toml` decides.

## Request

A chat host asked for this plan. Its request is not repeated here: it is untrusted data, and later prompts read this file.

## Tasks

4 task(s) in 3 wave(s). The tasks of a wave do not depend on each other; each task waits for the tasks it depends on.

### Wave 1

- **T1: Add the tempo-synced LFO and globbed core sources** (implementer, architectural)
  - Goal: The core library builds from a globbed source list and exposes a stateless LFO with the requested rates and shapes.
  - Files: `core/CMakeLists.txt`, `core/include/undertow/lfo.h`, `core/src/lfo.cpp`
  - Check (test): `grep -q 'file(GLOB' core/CMakeLists.txt && tools/spec.sh 2 lfo 'P2.01*'`
  - Check (test): `grep -q 'class Lfo' core/include/undertow/lfo.h && tools/spec.sh 2 lfo 'P2.02*'`
  - Check (test): `grep -q 'valueAt' core/src/lfo.cpp && tools/spec.sh 2 lfo 'P2.03*'`

### Wave 2

- **T2: Implement the rumble generator** (implementer, architectural)
  - Goal: The new rumble module produces the ghost kick, ducking, reverb tail, and bounded output described by the phase 2 spec.
  - After: T1
  - Files: `core/include/undertow/rumble.h`, `core/src/rumble.cpp`, `core/CMakeLists.txt`
  - Check (test): `grep -q 'class Rumble' core/include/undertow/rumble.h && tools/spec.sh 2 rumble 'P2.04*'`
  - Check (test): `grep -q 'duckGain' core/src/rumble.cpp && tools/spec.sh 2 rumble 'P2.05*'`
  - Check (test): `grep -q 'prepare' core/src/rumble.cpp && tools/spec.sh 2 rumble 'P2.06*'`
  - Check (test): `grep -q 'process' core/src/rumble.cpp && tools/spec.sh 2 rumble 'P2.07*'`
  - Check (test): `grep -q 'flush' core/src/rumble.cpp && tools/spec.sh 2 rumble 'P2.08*'`

- **T3: Implement the output stage** (implementer, architectural)
  - Goal: The output stage soft-clips, smooths master gain, applies the mono/width crossover, and reports clipping correctly.
  - After: T1
  - Files: `core/include/undertow/output_stage.h`, `core/src/output_stage.cpp`, `core/CMakeLists.txt`
  - Check (test): `grep -q 'softClip' core/include/undertow/output_stage.h && tools/spec.sh 2 output 'P2.09*,P2.10*'`
  - Check (test): `grep -q 'process' core/src/output_stage.cpp && tools/spec.sh 2 output 'P2.11*,P2.12*'`
  - Check (test): `grep -q 'master_gain' core/src/output_stage.cpp && tools/spec.sh 2 output 'P2.20*,P2.21*'`

### Wave 3

- **T4: Add voice modulation and wire the engine** (implementer, architectural)
  - Goal: The engine accepts VoiceModulation, drives both LFO destinations, routes rumble and output stage processing, and updates the master gain default.
  - After: T1, T2, T3
  - Files: `core/include/undertow/voice.h`, `core/src/voice.cpp`, `core/src/engine.cpp`, `core/src/params.cpp`
  - Check (test): `grep -q 'VoiceModulation' core/include/undertow/voice.h && tools/spec.sh 2 engine 'P2.13*,P2.14*,P2.23*'`
  - Check (test): `grep -q 'master_gain' core/src/params.cpp && tools/spec.sh 2 engine 'P2.15*,P2.16*'`
  - Check (test): `grep -q 'meters()' core/src/engine.cpp && tools/spec.sh 2 engine 'P2.17*,P2.22*'`
  - Check (test): `grep -q 'AllocationCounter' core/src/engine.cpp && tools/spec.sh 2 engine 'P2.18*,P2.19*'`

