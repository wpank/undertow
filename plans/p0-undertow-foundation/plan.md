---
plan: p0-undertow-foundation
title: "P0 · Undertow foundation"
---

# P0 · Undertow foundation

roko wrote this overview from the plan's `tasks.toml`, because the planner wrote no `plan.md`. Where the two differ, `tasks.toml` decides.

## Request

A chat host asked for this plan. Its request is not repeated here: it is untrusted data, and later prompts read this file.

## Tasks

3 task(s) in 3 wave(s). The tasks of a wave do not depend on each other; each task waits for the tasks it depends on.

### Wave 1

- **T1: Build system and parameter table** (implementer, integrative)
  - Goal: The core build config exists and the parameter API returns the exact 57-row table plus exact id lookup behavior.
  - Files: `CMakeLists.txt`, `CMakePresets.json`, `cmake/Juce.cmake`, `core/CMakeLists.txt`, `core/include/undertow/params.h`, `core/src/params.cpp`
  - Check (test): `tools/spec.sh 0 params 'P0.01*'`
  - Check (test): `tools/spec.sh 0 params 'P0.02*'`

### Wave 2

- **T2: Implement the engine skeleton** (implementer, integrative)
  - Goal: The core engine builds and satisfies the default, clamping, silence, allocation, and beat-clock checks.
  - After: T1
  - Files: `core/CMakeLists.txt`, `core/include/undertow/engine.h`, `core/src/engine.cpp`
  - Check (test): `tools/spec.sh 0 engine 'P0.03*'`
  - Check (test): `tools/spec.sh 0 engine 'P0.04*'`
  - Check (test): `tools/spec.sh 0 engine 'P0.05*'`
  - Check (test): `tools/spec.sh 0 engine 'P0.06*'`
  - Check (test): `tools/spec.sh 0 engine 'P0.07*'`

### Wave 3

- **T3: Implement the plugin skeleton and check script** (implementer, integrative)
  - Goal: The plugin target builds, exposes the expected interface and placeholder editor, and the local check script runs both presets plus pluginval.
  - After: T1, T2
  - Files: `plugin/CMakeLists.txt`, `plugin/PluginProcessor.h`, `plugin/PluginProcessor.cpp`, `plugin/PluginEditor.h`, `plugin/PluginEditor.cpp`, `tools/check.sh`
  - Check (test): `tools/spec.sh 0 plugin`
  - Check (test): `tools/pluginval.sh 5`
  - Check (test): `tools/check.sh --plugin`

