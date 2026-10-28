# Metakosmia Undertow: product requirements

Undertow is a monophonic bass synthesizer for hypnotic techno: a clean sub, a filtered bassline voice and a
tempo-locked rumble, with a 16-step sequencer that evolves while it plays. It is a VST3, AU and Standalone
instrument for macOS 11+ on Apple silicon, made for FL Studio 20 and any other host, and drawn in the ROSEDUST
phosphor language of `bardo/prd/18-interfaces` (see `docs/design/ui-spec.md`).

This repository starts as a specification. Each phase in `docs/specs/` states what to build, the exact C++ API,
and numbered acceptance checks that run as commands. The spec tests in `tests/spec/` were written before any
code; they fail until the phase is built, and the code changes, never the tests.

## 1. Sound-design goals

Each goal lists reference behaviours: concrete, measurable things the phase specs check with tolerances.

### 1.1 Sub: a clean sine foundation

The sub is what the floor feels. It must be a pure tone that is the same on every note.

- A sine one or two octaves below the played note (`sub_octave`). Note 45 (A2, 110 Hz) with `-1 oct` sounds
  55.0 Hz within 0.3%.
- Clean: with the filter open and no drive, its harmonics 2 to 10 sum to at most -60 dB (THD below 0.1%).
- Every note that starts from silence starts the sub at phase 0, so each note's first cycle hits the same way.
  Notes that overlap keep the phase running (no click).
- Mono: the low band leaves both channels identical (section 1.5).

### 1.2 Bassline: a filtered saw or square that moves

- Band-limited saw and square (PolyBLEP). A saw at 1.234 kHz keeps the energy that falls between its harmonics
  (aliasing) below 4 kHz at least 50 dB under the fundamental, where a naive saw manages about 20 dB.
- A 24 dB/octave low-pass with resonance and drive. At resonance 0 it is -3 dB at the cutoff, -24 dB an octave
  above and -48 dB two octaves above (with tolerances); resonance raises a peak at the cutoff; drive adds odd
  harmonics; nothing it is given makes it blow up.
- Envelopes with exact timing: a linear attack, then exponential decay and release that fall to 1% of their
  distance in the set time.
- A last-note-priority mono voice with glide: in `Legato` mode a note played over a held note slides to it in
  `glide_ms` (linear in pitch) without restarting the envelopes.
- Accent: a note with velocity 100 or more (or an accented sequencer step) is louder and brighter.

### 1.3 Rumble: the room under the kick

The techno rumble: a kick-triggered dark reverb, low-passed, saturated and ducked by the kick, locked to tempo.
Undertow makes its own ghost kick on the beat grid, so the rumble pumps with the track without routing a sidechain.

- A ghost kick (a 150 to 45 Hz pitch drop) fires on the `rumble_rate` grid while the transport plays. It feeds the
  reverb only; it is never heard dry.
- A dark mono reverb with a decay time of `rumble_decay`, then a 24 dB/octave low-pass at `rumble_tone`,
  saturation (`rumble_drive`) and a second low-pass, then the duck.
- At least 90% of the rumble's energy is below 200 Hz at the default tone (110 Hz).
- The duck pulls the rumble down at every kick and lets it swell back before the next one, so the rumble's
  quietest moments sit on the beat.
- At the defaults the rumble sits 9 to 15 dB under the bass below 200 Hz: a pumping bed you hear under the line.
  `rumble_drive` adds grit at a steady level.
- `rumble_send` feeds the voice into the same reverb, so the bassline gets its own tail.

### 1.4 Hypnotic motion

- Two tempo-synced LFOs whose phase follows the host's beat position exactly (a loop in the host replays the same
  motion). Rates include straight, triplet and dotted values and polyrhythms against 4/4: 3/16, 5/16, 7/16, 3/4,
  5/4 and 7/4.
- A 16-step sequencer locked to the host transport: per step a note, rest, accent, slide and probability; swing;
  a scale for mutation. Probability is decided by position and seed, so a looped bar repeats exactly. Mutate
  changes a set share of steps while staying in the scale, and Evolve mutates every N bars.

### 1.5 Level and safety

- The low end is mono: `width` adds a side signal, a 12 ms delayed copy high-passed steeply at `mono_freq` (default
  120 Hz), to one channel and takes it from the other, so everything under `mono_freq` stays in the middle (the side
  under `mono_freq / 2` stays at least 25 dB below the mid). The mono signal itself passes through no filter, so width
  never reshapes its peaks. At width 0 both channels carry the same signal, sample for sample.
- A soft clipper holds every output sample within -0.3 dBFS (0.966) and is exactly transparent below -6 dBFS (0.5).
  When the signal goes over full scale, the meters say so and the editor shows the NERV hazard stripe.
- Headroom: `master_gain` defaults to -14 dB, so the default patch's accented notes peak between -12 and -6 dBFS,
  at or under the clipper's knee. A DC blocker at the head of the output stage keeps the output centred, whatever the
  pulse width.
- The factory presets are loudness-matched: each renders within 3 dB of -18 dBFS RMS, the init patch's level, so
  stepping through them never drops or jumps in level. A SUB preset may be a short note, so it is read over its
  loudest 300 ms (phase 5.4).
- Every audio callback is realtime-safe: no allocation, lock, file or log, and no NaN, infinity or denormal out.

## 2. Signal flow

```text
 MIDI ──┐   ┌────────────── Sequencer (phase 3) ───────────────┐
        └──►│ notes, accent, slide          host ppq / internal │
            └──────────────┬────────────────────────────────────┘
                           ▼
   ┌──────────── Mono voice (phase 1) ─────────────┐
   │ OSC saw|square (PolyBLEP) ──┐                  │
   │ SUB sine −1|−2 oct ─────────┼─► LP 24 dB/oct ─►│ × amp env ─► saturation ─┬──────────────► + ─► crossover ─► clipper ─► L R
   │ glide · legato · accent     │   res · drive    │                          │                ▲   mono < mono_freq
   │ filter env · key track      │   ▲ LFO 1/2      │                          └─ rumble_send ─┐ │   width above
   └─────────────────────────────┴───┴──────────────┘                                          ▼ │
                                          ghost kick on the beat grid ─► dark reverb ─► LP ─► drive ─► LP ─► duck ─► rumble
                                                                                      (phase 2)
```

## 3. Architecture

### 3.1 Layout

| Path | What | Phase |
|---|---|---|
| `core/` | `undertow_core` (alias `Undertow::core`): the plain C++17 DSP core, headers in `core/include/undertow/`, namespace `undertow`. No JUCE, no host, no UI. | 0 to 3, 5 |
| `plugin/` | The JUCE plugin target `Undertow`: `UndertowProcessor` (a thin wrapper around `undertow::Engine`) and `UndertowEditor` (the ROSEDUST UI). | 0, 4, 5 |
| `tools/` | `fetch-juce.sh`, `fetch-pluginval.sh`, `spec.sh`, `pluginval.sh`, `auval.sh` (given); `check.sh` (phase 0); the offline renderer and install script (phase 5). | |
| `tests/spec/` | The spec tests, given: one executable per file. | given |
| `third_party/doctest/` | doctest 2.5.3, MIT, vendored. | given |
| `docs/` | This PRD, `design/ui-spec.md`, `specs/phase-N-*.md`. | given |

### 3.2 Build

- CMake 3.22 or newer with presets and the Unix Makefiles generator (this machine has no Ninja), C++17,
  `CMAKE_OSX_DEPLOYMENT_TARGET` 11.0, arm64.
- Two presets, as in Metakosmia: `core` (binary dir `build/core`, no JUCE, `UNDERTOW_BUILD_PLUGIN=OFF`) and `plugin`
  (`build/plugin`, `UNDERTOW_BUILD_PLUGIN=ON`). Both `RelWithDebInfo`, so the timing checks measure optimized code.
- JUCE 8.0.12, the same tarball and SHA256 as Metakosmia, comes from the shared dependency cache
  (`~/Library/Caches/undertow`, or `$UNDERTOW_DEPS_DIR`): `tools/fetch-juce.sh --prefix` downloads, checks, builds
  and installs it once (about 30 s), and `cmake/Juce.cmake` uses `find_package(JUCE CONFIG)` on that prefix. A
  new checkout or worktree therefore never downloads JUCE or rebuilds juceaide; it compiles only the JUCE modules
  its plugin uses.
- The plugin target: `juce_add_plugin(Undertow ...)` with company `Metakosmia`, manufacturer code `Mtks`, plugin code
  `MkUt`, bundle id `audio.metakosmia.undertow`, product name `Metakosmia Undertow`, formats VST3 AU Standalone,
  `IS_SYNTH TRUE`, `NEEDS_MIDI_INPUT TRUE`.
- Validation: pluginval 1.0.4 (`tools/pluginval.sh`) and `auval` for the AU.

### 3.3 Threads and realtime rules

- The audio thread owns `undertow::Engine`. `UndertowProcessor::processBlock` copies the parameter values into it
  (`setParam`), converts the MIDI buffer into a preallocated `undertow::MidiEvent` array and the host position into
  an `undertow::Transport`, and calls `Engine::process`.
- The UI thread never calls into the engine's processing. It reads `Engine::meters()` and the scope (phase 4) through
  lock-free storage, and changes the sequencer pattern through a lock-free handoff that reads back at once what it
  set (phase 3).
- In `process`, `setParam`, `meters` and `reset`: no heap allocation (the spec tests count them with an
  `operator new` override), no locks, no I/O, no logging, no exceptions. Everything is sized in `prepare`.
- Outputs are always finite; state that decays flushes to exact zero instead of drifting into denormals.

### 3.4 Beat clock

With a host position (`Transport::valid`), the engine's beat position is the host's ppq, advanced through the
block at the host's tempo while it plays. Without one (the Standalone app) the engine runs its own clock at
`seq_bpm` from 0 after `reset()`. LFOs, the ghost kick, the duck and the sequencer all read this one clock.

## 4. Parameters

The host-facing parameters, in host order. Ids are stable once released; the index is the position in this table
and the `undertow::param` enumerator (the id in CamelCase: `filter_cutoff` is `undertow::param::FilterCutoff`).
"Centre" is the value at half knob travel (JUCE `setSkewForCentre`); 0 means a linear range. Choice parameters
run from 0 to their number of choices minus one. `tests/spec/p0/params_test.cpp` holds the same table.

| # | id | name | kind | min | max | default | centre | unit |
|---|---|---|---|---|---|---|---|---|
| 0 | `osc_wave` | Wave | Choice | 0 | 1 | 0 | 0 | |
| 1 | `osc_pw` | Pulse Width | Float | 0.05 | 0.95 | 0.5 | 0 | |
| 2 | `osc_level` | Osc Level | Float | 0 | 1 | 0.6 | 0 | |
| 3 | `osc_octave` | Octave | Int | -2 | 1 | 0 | 0 | oct |
| 4 | `osc_fine` | Fine | Float | -50 | 50 | 0 | 0 | ct |
| 5 | `sub_level` | Sub Level | Float | 0 | 1 | 0.7 | 0 | |
| 6 | `sub_octave` | Sub Octave | Choice | 0 | 1 | 0 | 0 | |
| 7 | `filter_cutoff` | Cutoff | Float | 20 | 18000 | 600 | 600 | Hz |
| 8 | `filter_res` | Resonance | Float | 0 | 1 | 0.35 | 0 | |
| 9 | `filter_drive` | Drive | Float | 0 | 1 | 0.2 | 0 | |
| 10 | `filter_env` | Env Amount | Float | -6 | 6 | 3 | 0 | oct |
| 11 | `filter_track` | Key Track | Float | 0 | 1 | 0.5 | 0 | |
| 12 | `amp_attack` | Amp Attack | Float | 0.5 | 2000 | 1 | 30 | ms |
| 13 | `amp_decay` | Amp Decay | Float | 1 | 4000 | 300 | 200 | ms |
| 14 | `amp_sustain` | Amp Sustain | Float | 0 | 1 | 0.8 | 0 | |
| 15 | `amp_release` | Amp Release | Float | 1 | 4000 | 60 | 200 | ms |
| 16 | `filt_attack` | Filter Attack | Float | 0.5 | 2000 | 1 | 30 | ms |
| 17 | `filt_decay` | Filter Decay | Float | 1 | 4000 | 220 | 200 | ms |
| 18 | `filt_sustain` | Filter Sustain | Float | 0 | 1 | 0 | 0 | |
| 19 | `filt_release` | Filter Release | Float | 1 | 4000 | 60 | 200 | ms |
| 20 | `glide_ms` | Glide | Float | 0 | 1000 | 50 | 100 | ms |
| 21 | `glide_mode` | Glide Mode | Choice | 0 | 2 | 1 | 0 | |
| 22 | `accent` | Accent | Float | 0 | 1 | 0.5 | 0 | |
| 23 | `bend_range` | Bend Range | Int | 0 | 12 | 2 | 0 | st |
| 24 | `lfo1_rate` | LFO 1 Rate | Choice | 0 | 16 | 2 | 0 | |
| 25 | `lfo1_shape` | LFO 1 Shape | Choice | 0 | 5 | 0 | 0 | |
| 26 | `lfo1_cutoff` | LFO 1 to Cutoff | Float | -4 | 4 | 0.5 | 0 | oct |
| 27 | `lfo1_phase` | LFO 1 Phase | Float | 0 | 1 | 0 | 0 | |
| 28 | `lfo2_rate` | LFO 2 Rate | Choice | 0 | 16 | 11 | 0 | |
| 29 | `lfo2_shape` | LFO 2 Shape | Choice | 0 | 5 | 1 | 0 | |
| 30 | `lfo2_dest` | LFO 2 Dest | Choice | 0 | 5 | 0 | 0 | |
| 31 | `lfo2_depth` | LFO 2 Depth | Float | 0 | 1 | 0 | 0 | |
| 32 | `lfo2_phase` | LFO 2 Phase | Float | 0 | 1 | 0 | 0 | |
| 33 | `rumble_level` | Rumble | Float | 0 | 1 | 0.3 | 0 | |
| 34 | `rumble_send` | Rumble Send | Float | 0 | 1 | 0.25 | 0 | |
| 35 | `rumble_kick` | Ghost Kick | Float | 0 | 1 | 0.6 | 0 | |
| 36 | `rumble_rate` | Kick Rate | Choice | 0 | 3 | 0 | 0 | |
| 37 | `rumble_decay` | Rumble Decay | Float | 0.2 | 8 | 2.5 | 2 | s |
| 38 | `rumble_tone` | Rumble Tone | Float | 40 | 250 | 110 | 100 | Hz |
| 39 | `rumble_drive` | Rumble Drive | Float | 0 | 1 | 0.4 | 0 | |
| 40 | `rumble_duck` | Duck | Float | 0 | 1 | 0.8 | 0 | |
| 41 | `sat_drive` | Saturation | Float | 0 | 1 | 0.2 | 0 | |
| 42 | `width` | Width | Float | 0 | 1 | 0.25 | 0 | |
| 43 | `mono_freq` | Mono Below | Float | 60 | 250 | 120 | 0 | Hz |
| 44 | `master_gain` | Master | Float | -24 | 6 | -14 | 0 | dB |
| 45 | `seq_on` | Sequencer | Bool | 0 | 1 | 0 | 0 | |
| 46 | `seq_sync` | Seq Sync | Choice | 0 | 1 | 0 | 0 | |
| 47 | `seq_bpm` | Seq BPM | Float | 60 | 200 | 128 | 0 | bpm |
| 48 | `seq_rate` | Seq Rate | Choice | 0 | 3 | 1 | 0 | |
| 49 | `seq_length` | Seq Length | Int | 1 | 16 | 16 | 0 | |
| 50 | `seq_gate` | Seq Gate | Float | 0.05 | 1 | 0.5 | 0 | |
| 51 | `seq_swing` | Swing | Float | 0 | 1 | 0 | 0 | |
| 52 | `seq_root` | Root | Int | 24 | 60 | 33 | 0 | |
| 53 | `seq_scale` | Scale | Choice | 0 | 4 | 1 | 0 | |
| 54 | `seq_mutate` | Mutate | Float | 0 | 1 | 0.25 | 0 | |
| 55 | `seq_evolve` | Evolve | Choice | 0 | 5 | 0 | 0 | |
| 56 | `seq_seed` | Seed | Int | 0 | 9999 | 1 | 0 | |

The choices, in order:

- `osc_wave`: Saw, Square
- `sub_octave`: -1 oct, -2 oct
- `glide_mode`: Off, Legato, Always
- `lfo1_rate`, `lfo2_rate`: 4 bars, 2 bars, 1 bar, 1/2, 1/4, 1/8, 1/16, 1/32, 1/4T, 1/8T, 1/16T, 3/16, 5/16, 7/16,
  3/4, 5/4, 7/4 (cycle lengths in beats: 16, 8, 4, 2, 1, 0.5, 0.25, 0.125, 2/3, 1/3, 1/6, 0.75, 1.25, 1.75, 3, 5, 7)
- `lfo1_shape`, `lfo2_shape`: Sine, Triangle, Saw Down, Saw Up, Square, Sample & Hold
- `lfo2_dest`: Cutoff, Resonance, Pulse Width, Rumble Send, Pitch, Amp
- `rumble_rate`: 1/4, 1/2, 1 bar, 1/8
- `seq_sync`: Host, Internal
- `seq_rate`: 1/8, 1/16, 1/32, 1/16T
- `seq_scale`: Chromatic, Minor, Phrygian, Dorian, Minor Pent
- `seq_evolve`: Off, 1 bar, 2 bars, 4 bars, 8 bars, 16 bars

The sequencer's 16 steps (note, gate, accent, slide, probability) are plugin state, saved with the session, not
host parameters (phase 3).

## 5. Phases

| Phase | Spec | Builds | Depends on | Size |
|---|---|---|---|---|
| 0 | `specs/phase-0-foundation.md` | CMake and presets, the core library with the parameter table and the engine skeleton, the JUCE plugin skeleton (VST3, AU, Standalone) through pluginval, `tools/check.sh` | nothing | M: 3 tasks, about 600 lines |
| 1 | `specs/phase-1-voice.md` | PolyBLEP oscillators, the sub, the 24 dB/oct filter with drive, envelopes, the mono voice (glide, legato, accent, bend), MIDI, smoothing, realtime safety | 0 | L: 5 tasks, about 900 lines |
| 2 | `specs/phase-2-rumble.md` | Tempo-synced LFOs, the rumble engine, saturation, the mono-sub crossover and width, the output clipper | 1 | L: 4 tasks, about 900 lines |
| 2.1 | `specs/phase-2.1-voice-fixes.md` | Fixes from a listening test of the voice: a sustain change glides, glides keep their time and pitch, an empty block leaves the meters alone, the filter keeps its resonance at full level, the oscillator stops at Nyquist | 2 | S: 5 tasks, about 50 lines |
| 2.2 | `specs/phase-2.2-gain-staging.md` | Gain staging from a listening test of phase 2: width as a side signal (no crossover on the mono path), drives with makeup gain, the rumble audible at the defaults | 2.1 | S: 3 tasks, about 60 lines |
| 3 | `specs/phase-3-sequencer.md` | The 16-step sequencer: transport sync, accent, slide, rest, probability, swing, mutate and evolve; pattern state | 1 (2 for the full mix) | M: 2 tasks, about 700 lines |
| 4 | `specs/phase-4-ui.md` | The ROSEDUST phosphor editor (`docs/design/ui-spec.md`): knobs, the bone number, the scope, the rumble field, the tunnel, the hazard stripe, the step array; snapshot and frame-budget checks | 2, 3 | L: 7 tasks, about 1000 lines |
| 4.1 | `specs/phase-4.1-bone.md` | Bone only for the one number: phase 4's knob ticks, knob thumb, tooltips, clock readout and rumble label back to the ROSEDUST tokens, and the review images made again | 4 | S: 1 task, about 5 lines and three images |
| 4.2 | `specs/phase-4.2-layout.md` | The editor laid out as designed: the eight panels with labelled 40 px knobs, formatted readouts on hover, the stamps, the HEAD, the pitch as the bone number in the CHEST's lower third; a hook listing every drawn word; a new instance plays the default bassline again | 4.1 | M: 3 tasks, about 400 lines |
| 4.3 | `specs/phase-4.3-finish.md` | The editor's finish: the limbs' knobs where the mock puts them, MOD/05's nine fitting it; the HEAD's words in Unicode on one line; 16 px clip bands with their legend, the clock and the values readable while clipping; `Hz` on the number's baseline below the scope; the rumble pump field back; the review images made again | 4.2 | S: 2 tasks, about 150 lines and three images |
| 5 | `specs/phase-5-ship.md` | Factory presets as programs, strict pluginval, the offline renderer and demo WAVs, an install script (the real install and auval by hand), the top of the resonance knob, probability 1 that always plays, the one number drawn whole at full size | 4.3 | M: 8 tasks, about 735 lines |
| 5.1 | `specs/phase-5.1-renders.md` | Phase 5's finish: RUMBLE Dark Room retuned on the final resonance curve; the offline renderer holds the note and plays the whole pattern, and the demo renders are its files as written (no rescaling, no made-up audio); texts::draw never cuts a word (Hz drew as H) | 5 | S: 3 tasks, about 40 lines changed, about 70 removed, four renders and three images |
| 5.2 | `specs/phase-5.2-presets.md` | The presets as named: SUB Pure Foundation the clean sine sub alone and every SUB preset the sub (F17), no preset's energy below 20 Hz (F18), by retuning six presets, not the engine | 5.1 | S: 1 task, about 10 lines and one showcase render |
| 5.3 | `specs/phase-5.3-presets.md` | Low Pressure as named, and the Roller at level: SUB Low Pressure as "sub at -2 oct with a little saturation, a slow swell", apart from Pure Foundation and still the clean sub; BASS Hypnotic Roller's showcase at the reel's level | 5.2 | S: 1 task, about 5 lines and one showcase render |
| 5.4 | `specs/phase-5.4-presets.md` | Kick Companion as named: SUB Kick Companion as "a short, round sub note that sits after a kick", a decaying note apart from the two held SUB presets and still the clean sub; the level check reads a SUB preset over its loudest 300 ms | 5.3 | S: 1 task, about 5 lines |

```text
 phase 0 ──► phase 1 ──► phase 2 ──► phase 2.1 ──► phase 2.2 ──┬──► phase 4 ──► phase 4.1 ──► phase 4.2 ──► phase 4.3 ──► phase 5 ──► phase 5.1 ──► phase 5.2 ──► phase 5.3 ──► phase 5.4
                    └──► phase 3 ──────────────────────────────┘
```

Run the phases in order: 0, 1, 2, 2.1, 2.2, 3, 4, 4.1, 4.2, 4.3, 5, 5.1, 5.2, 5.3, 5.4. Phases 2 and 3 are independent in
their DSP and could run in parallel once phase 1 is done, but both edit `core/src/engine.cpp` and `core/CMakeLists.txt`,
so in parallel they end in a merge there.

## 6. How the checks work

- `tools/spec.sh <phase> <name> [<filter>]` builds and runs one spec executable, `tests/spec/p<phase>/<name>_test.cpp`,
  optionally only the doctest test cases matching a filter (`'P1.03*'`). Core tests build with the `core` preset in
  seconds; tests marked `// spec-preset: plugin` build with the `plugin` preset.
- One executable per test file means a task's check compiles only what that task touches: two tasks running in
  parallel never break each other's check.
- A test file names the files it needs (`// spec-needs: core/include/undertow/oscillator.h`). Once they all exist
  the test joins the default build and `ctest`, so `tools/check.sh` runs every spec test whose code is in place,
  and a later phase can never quietly break an earlier one.
- Failure messages say what to change ("sub_octave -1 on note 45 must sound 55 Hz; measured 110 Hz"), because the
  fixing model reads them.
- Timing budgets have at least a 10x margin over the reference implementation on this machine, so a loaded
  machine does not fail them.

## 7. Out of scope

- Polyphony, unison and chords; MPE; oversampling beyond what the clipper and drive need.
- Windows and Linux builds; AAX; CLAP.
- A sidechain input bus (the ghost kick replaces it).
- Preset browsing beyond the host's program list; cloud anything.
- Code signing with a Developer ID and notarization (local ad-hoc signing only).

## 8. Licensing

JUCE 8 is dual-licensed, AGPLv3 or commercial. Undertow is built for local use; distributing it needs a licence
decision first. doctest is MIT (`third_party/doctest/LICENSE.txt`).
