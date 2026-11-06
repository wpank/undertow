---
plan: p2-1-undertow-voice-fixes-2
title: "P2.1 · Undertow voice fixes"
---

# P2.1 · Undertow voice fixes

roko wrote this overview from the plan's `tasks.toml`, because the planner wrote no `plan.md`. Where the two differ, `tasks.toml` decides.

## Request

A chat host asked for this plan. Its request is not repeated here: it is untrusted data, and later prompts read this file.

## Tasks

5 task(s) in 1 wave(s). The tasks of a wave do not depend on each other; each task waits for the tasks it depends on.

### Wave 1

- **T1: Smooth sustain changes in the envelopes** (implementer, mechanical)
  - Goal: Held-envelope sustain changes move smoothly, settle to the new sustain level, and still release to exact zero.
  - Files: `core/include/undertow/envelope.h`, `core/src/envelope.cpp`
  - Check (test): `tools/spec.sh 2_1 envelope 'P2.1.01*'`

- **T2: Fix legato glide state transitions** (implementer, mechanical)
  - Goal: Glide timing and pitch stay correct when held notes are released or glide is disabled mid-flight.
  - Files: `core/include/undertow/voice.h`, `core/src/voice.cpp`
  - Check (test): `tools/spec.sh 2_1 voice 'P2.1.02*'`
  - Check (test): `tools/spec.sh 2_1 voice 'P2.1.03*'`

- **T3: Guard zero-length engine blocks and preserve existing saturation** (implementer, mechanical)
  - Goal: The engine leaves meters and audio unchanged for empty blocks, while its saturation behavior still matches the existing blend.
  - Files: `core/src/engine.cpp`
  - Check (test): `tools/spec.sh 2_1 engine 'P2.1.04*'`
  - Check (test): `tools/spec.sh 2_1 engine 'P2.1.09*'`

- **T4: Restore filter resonance law and linear headroom** (implementer, mechanical)
  - Goal: The filter stays linear at voice levels for drive 0 and keeps the phase-1 resonance behavior.
  - Files: `core/include/undertow/filter.h`, `core/src/filter.cpp`
  - Check (test): `tools/spec.sh 2_1 filter 'P2.1.05*'`
  - Check (test): `tools/spec.sh 2_1 filter 'P2.1.06*'`
  - Check (test): `tools/spec.sh 1 filter`

- **T5: Clamp oscillator frequency at Nyquist** (implementer, mechanical)
  - Goal: Oscillator outputs stay silent at and above Nyquist instead of aliasing into the audible band.
  - Files: `core/include/undertow/oscillator.h`, `core/src/oscillator.cpp`
  - Check (test): `tools/spec.sh 2_1 oscillator 'P2.1.07*'`

