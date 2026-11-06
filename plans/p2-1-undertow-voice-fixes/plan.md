---
plan: p2-1-undertow-voice-fixes
title: "P2.1 · Undertow voice fixes"
---

# P2.1 · Undertow voice fixes

roko wrote this overview from the plan's `tasks.toml`, because the planner wrote no `plan.md`. Where the two differ, `tasks.toml` decides.

## Request

A chat host asked for this plan. Its request is not repeated here: it is untrusted data, and later prompts read this file.

## Tasks

5 task(s) in 1 wave(s). The tasks of a wave do not depend on each other; each task waits for the tasks it depends on.

### Wave 1

- **T1: Smooth sustain changes in the envelope** (implementer, mechanical)
  - Goal: A held envelope no longer clicks when sustain changes, while release still reaches exact zero.
  - Files: `core/src/envelope.h`, `core/src/envelope.cpp`
  - Check (test): `tools/spec.sh 2_1 envelope 'P2.1.01*'`

- **T2: Fix legato glide completion and detachment handling** (implementer, mechanical)
  - Goal: Voice glide timing stays fixed, and detached notes land on pitch immediately even after a glide.
  - Files: `core/src/voice.h`, `core/src/voice.cpp`
  - Check (test): `tools/spec.sh 2_1 voice 'P2.1.02*'`
  - Check (test): `tools/spec.sh 2_1 voice 'P2.1.03*'`

- **T3: Guard zero-length engine processing** (implementer, mechanical)
  - Goal: The engine leaves meters and audio unchanged when asked to process an empty block.
  - Files: `core/src/engine.cpp`
  - Check (test): `tools/spec.sh 2_1 engine 'P2.1.04*'`
  - Check (test): `tools/spec.sh 2_1 engine 'P2.1.09*'`

- **T4: Restore filter linearity and the phase 1 resonance law** (implementer, mechanical)
  - Goal: The filter stays linear at voice levels and preserves the phase 1 resonance behavior.
  - Files: `core/src/filter.h`, `core/src/filter.cpp`
  - Check (test): `tools/spec.sh 2_1 filter 'P2.1.05*'`
  - Check (test): `tools/spec.sh 2_1 filter 'P2.1.06*'`
  - Check (test): `tools/spec.sh 1 filter`

- **T5: Clamp oscillator frequency at Nyquist** (implementer, mechanical)
  - Goal: Oscillator output is silent at Nyquist and never aliases from frequencies above half the sample rate.
  - Files: `core/src/oscillator.h`, `core/src/oscillator.cpp`
  - Check (test): `tools/spec.sh 2_1 oscillator 'P2.1.07*'`

