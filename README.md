# Metakosmia Undertow

A monophonic bass synthesizer for hypnotic techno: a clean sub, a filtered bassline voice and a tempo-locked rumble,
with a 16-step sequencer that evolves while it plays. VST3, AU and Standalone, macOS 11+ on Apple silicon, built
with JUCE 8.0.12, drawn in the ROSEDUST phosphor language.

This repository starts as a specification, and an agent harness builds the synth from it, phase by phase:

- `docs/PRD.md`: the product, the sound-design goals, the architecture, the parameters and the phases.
- `docs/design/ui-spec.md`: the editor, in the language of `bardo/prd/18-interfaces`.
- `docs/specs/phase-N-*.md`: one spec per phase, with the exact C++ API and numbered acceptance checks.
- `tests/spec/`: the acceptance tests, written before the code. They fail until a phase is built. Change the code,
  never the tests.

## Build and check

Needs CMake 3.22+, the Xcode command-line tools and the network once (JUCE and pluginval are cached in
`~/Library/Caches/undertow`).

```sh
tools/spec.sh 0 params            # one spec test executable (phase 0, params_test.cpp)
tools/spec.sh 1 oscillator 'P1.0*' # only some test cases
tools/check.sh                    # the core: configure, build, every spec test whose code is in place (phase 0 adds it)
tools/check.sh --plugin           # also the plugin (VST3, AU, Standalone), its spec tests and pluginval
tools/pluginval.sh 5              # pluginval 1.0.4 on the VST3
```

The `core` preset (`build/core`) builds the plain C++ DSP library and its tests in seconds, without JUCE. The
`plugin` preset (`build/plugin`) adds the JUCE plugin; the first build in a new checkout compiles the JUCE modules
(about a minute).

## Layout

| Path | What |
|---|---|
| `core/` | the DSP core, plain C++17, no JUCE (`Undertow::core`) |
| `plugin/` | the JUCE plugin `Undertow`: processor and editor |
| `tools/` | fetch scripts, the spec runner, pluginval, the checks |
| `tests/spec/` | the spec tests, one executable per file |
| `third_party/doctest/` | doctest 2.5.3 (MIT) |

## Licences

JUCE 8 is AGPLv3 or commercial: Undertow is built for local use, and distributing it needs a licence decision.
doctest is MIT (`third_party/doctest/LICENSE.txt`).
