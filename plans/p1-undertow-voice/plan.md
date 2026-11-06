---
plan: p1-undertow-voice
title: "P1 · Undertow voice"
---

# P1 · Undertow voice

roko wrote this overview from the plan's `tasks.toml`, because the planner wrote no `plan.md`. Where the two differ, `tasks.toml` decides.

## Request

A chat host asked for this plan. Its request is not repeated here: it is untrusted data, and later prompts read this file.

## Tasks

5 task(s) in 3 wave(s). The tasks of a wave do not depend on each other; each task waits for the tasks it depends on.

### Wave 1

- **T1: Implement oscillators and register them** (implementer, integrative)
  - Goal: The oscillator module builds and passes all four phase-1 oscillator checks.
  - Files: `core/include/undertow/oscillator.h`, `core/src/oscillator.cpp`
  - Check (test): `tools/spec.sh 1 oscillator 'P1.01*'`
  - Check (test): `tools/spec.sh 1 oscillator 'P1.02*'`
  - Check (test): `tools/spec.sh 1 oscillator 'P1.03*'`
  - Check (test): `tools/spec.sh 1 oscillator 'P1.04*'`

- **T2: Implement the low-pass filter** (implementer, integrative)
  - Goal: The filter module builds and passes all three phase-1 filter checks.
  - Files: `core/include/undertow/filter.h`, `core/src/filter.cpp`
  - Check (test): `tools/spec.sh 1 filter 'P1.05*'`
  - Check (test): `tools/spec.sh 1 filter 'P1.06*'`
  - Check (test): `tools/spec.sh 1 filter 'P1.07*'`

- **T3: Implement the envelope** (implementer, integrative)
  - Goal: The envelope module builds and passes all three phase-1 envelope checks.
  - Files: `core/include/undertow/envelope.h`, `core/src/envelope.cpp`
  - Check (test): `tools/spec.sh 1 envelope 'P1.08*'`
  - Check (test): `tools/spec.sh 1 envelope 'P1.09*'`
  - Check (test): `tools/spec.sh 1 envelope 'P1.10*'`

### Wave 2

- **T4: Implement the mono voice and wire it into the engine** (implementer, integrative)
  - Goal: The mono voice and engine-facing wiring compile together and satisfy the voice checks.
  - After: T1, T2, T3
  - Files: `core/include/undertow/voice.h`, `core/src/voice.cpp`, `core/src/engine.cpp`
  - Check (test): `tools/spec.sh 1 voice 'P1.11*'`
  - Check (test): `tools/spec.sh 1 voice 'P1.12*'`
  - Check (test): `tools/spec.sh 1 voice 'P1.13*'`
  - Check (test): `tools/spec.sh 1 voice 'P1.14*'`
  - Check (test): `tools/spec.sh 1 voice 'P1.15*'`

### Wave 3

- **T5: Wire engine MIDI, parameters, meters, and performance behavior** (implementer, integrative)
  - Goal: The engine runs the voice correctly through MIDI and parameter updates and passes the engine checks.
  - After: T4
  - Files: `core/src/engine.cpp`, `core/CMakeLists.txt`
  - Check (test): `tools/spec.sh 1 engine 'P1.16*'`
  - Check (test): `tools/spec.sh 1 engine 'P1.17*'`
  - Check (test): `tools/spec.sh 1 engine 'P1.18*'`
  - Check (test): `tools/spec.sh 1 engine 'P1.19*'`
  - Check (test): `tools/spec.sh 1 engine 'P1.20*'`

