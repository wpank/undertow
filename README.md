# Metakosmia Undertow 2

A sub-bass instrument for hypnotic techno: a rolling, sub-heavy bass that sits under a 4/4 kick and changes only a
little over minutes. Two oscillators and noise feed a 24 dB/octave ladder low-pass, a clean sine sub sits under them,
three decays shape every hit, and an 8-step pitch and velocity sequencer plays the line, drifting a little and coming
home. A dark rumble, fed by a ghost kick, sits under it all. VST3, AU and Standalone for macOS 11+ on Apple silicon,
built with JUCE 8.0.12.

## How this repository is built

The specification comes first, and the code is written to it:

- `docs/PRD.md`: the product, its sound as measurable behaviours, the architecture, the parameters and the phases.
- `docs/specs/phase-N-*.md`: one spec per phase, with the exact C++ API and numbered acceptance checks that run as
  commands.
- `tests/spec/`: the acceptance tests, written before the code. A phase is done when its tests pass; the code
  changes, never the tests.

The code is written phase by phase by roko, an agent harness, from these specs.

## Build and check

Needs CMake 3.22+, the Xcode command-line tools, and the network once (JUCE and pluginval are cached in
`~/Library/Caches/undertow`).

```sh
tools/spec.sh 1 params             # one spec test executable (phase 1, params_test.cpp)
tools/spec.sh 1 engine 'P1.06*'    # only some of its test cases
tools/check.sh                     # the core: configure, build, every spec test whose code is in place
tools/check.sh --plugin            # also the plugin (VST3, AU, Standalone), its spec tests and pluginval
tools/pluginval.sh 5               # pluginval 1.0.4 on the VST3
```

The `core` preset (`build/core`) builds the plain C++ DSP library and its tests in seconds, without JUCE. The
`plugin` preset (`build/plugin`) adds the JUCE plugin; the first build in a new checkout compiles the JUCE modules
(about a minute).

## Layout

| Path | What |
|---|---|
| `core/` | the DSP core, plain C++17, no JUCE (`Undertow::core`) |
| `plugin/` | the JUCE plugin `Undertow`: processor and window |
| `tools/` | fetch scripts, the spec runner, the offline renderer, pluginval, auval, the checks |
| `tests/spec/` | the spec tests, one executable per file |
| `third_party/doctest/` | doctest 2.5.3 (MIT) |

## Licence

Undertow is free software, licensed under the GNU Affero General Public License, version 3 only (`AGPL-3.0-only`):
see `LICENSE`. It is built with JUCE 8, used under JUCE's AGPLv3 licence option, and its tests use doctest, which is
MIT-licensed (`third_party/doctest/LICENSE.txt`).
