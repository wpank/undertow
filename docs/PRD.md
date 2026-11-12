# Metakosmia Undertow 2: product requirements

Undertow 2 is a monophonic sub-bass instrument for hypnotic techno: a rolling, sub-heavy bass that sits under a 4/4
kick and repeats for minutes, changing only a little. It is a percussive Moog-style voice in the manner of a DFAM: two
oscillators and noise into a 24 dB/octave ladder low-pass, a clean sine sub, three decays and an 8-step pitch and
velocity sequencer whose pattern drifts and comes home. A dark, kick-fed rumble sits under it. It is a VST3, AU and
Standalone instrument for macOS 11+ on Apple silicon, drawn in the ROSEDUST phosphor language.

This repository is a specification first. Each phase in `docs/specs/` states what to build, the exact C++ API and
numbered acceptance checks that run as commands. The spec tests in `tests/spec/` are written before the code; they
fail until the phase is built, and the code changes, never the tests.

## 1. Sound-design goals

Each goal lists reference behaviours: measurable things the phase specs check, with tolerances. A **render** is 8 bars
at 130 BPM of a patch's own pattern and root, at 48 kHz, the voice alone (no kick, the rumble off unless the goal is
about the rumble). Its long-term spectrum is a Welch average of 32768-point Blackman-Harris segments with 50% overlap,
and a band's **share** is its power over the total.

### 1.1 The range: the energy sits between 30 and 150 Hz

- At least 90% of a render's power is below 150 Hz, at least 98.5% below 300 Hz, at least 88% between 30 and 150 Hz,
  at most 0.25% below 20 Hz and at most 2% below 30 Hz. Its spectral centroid is at most 90 Hz.
- It stays dark: the band above 500 Hz is at least 25 dB under the total, above 1 kHz at least 40 dB under, and above
  2 kHz at least 55 dB under. Even its brightest moments stay low: the 95th percentile of the centroid of 4096-point
  frames is at most 150 Hz.
- Drive adds just enough harmonics to be heard on small speakers: 150 to 500 Hz holds 1% to 8% of the power.

### 1.2 The voice: two oscillators, noise and a sine sub, into a mixer

- **VCO 1 and VCO 2**, each a triangle or a square, in tune within 0.1%, their aliases at least 60 dB under the
  fundamental at C5: the square is band-limited, and the triangle's harmonics fall fast enough on their own. VCO 1
  plays the root plus the step's pitch; VCO 2 is `vco2_tune` semitones from it.
- **1 to 2 FM**: VCO 1's triangle modulates VCO 2's frequency linearly, `f2 * (1 + vco_fm * tri1)`, so VCO 2 never
  goes below 0 Hz.
- **Noise**: white, flat within 2 dB across the octave bands from 100 Hz to 10 kHz, and the same after every `reset()`.
- **The sub**: a pure sine an octave under VCO 1 (`-1 oct`, the default) or at its pitch (`0 oct`), phase-locked to it,
  so the two always add the same way. Its harmonics 2 to 10 are at least 60 dB under it.
- **The mixer** sums VCO 1, VCO 2, noise and the sub into the ladder, so the drive warms the sub too.
- **Phase**: a hit that finds the level under -20 dB restarts VCO 1, VCO 2 and the sub at phase 0, so a pattern's hits
  meet the kick the same way on every repeat. Into a louder note the oscillators keep running. The ladder smooths the
  restart, so it never clicks.

### 1.3 The ladder

- A Moog-style 24 dB/octave ladder low-pass: four saturating one-pole stages in a feedback loop. The cutoff is the
  stages' frequency. At resonance 0 the gain is -12 dB at the cutoff (within 0.5 dB), -28 dB an octave above (within
  1 dB) and -49 dB two octaves above (within 1.5 dB), and falls 23 to 25 dB per octave from 4 to 8 times the cutoff.
- **Resonance** raises a peak just under the cutoff: +11 dB (within 1 dB) at the top of the knob, and never thins the
  bass: below a quarter of the cutoff the gain stays within 1 dB of resonance 0's at every setting.
- **It never self-oscillates.** At the top of the knob an impulse response stays 60 dB under its peak after 12 cycles
  of the cutoff, and silence in is exact silence out within a second.
- **Drive** sets the gain into the saturating stages, from 0.5 at 0 to 3 at the top (`0.5 * 6^drive`), taken back out
  after them. For a -6 dBFS 55 Hz sine at a 220 Hz cutoff (four times the note) and resonance 0, drive 0 leaves
  harmonics 2 to 10 at least 38 dB under the fundamental, the top of the knob puts the strongest of them 12 to 30 dB
  under (warm, not fuzz), and the level stays within 4 dB across the knob.

### 1.4 The decays and the VCA

- **Three decays**, each exponential, reaching -40 dB of its swing at its set time (within 5%):
  - pitch: at each hit VCO 1, VCO 2 and the sub start `pitch_env` semitones high and fall to the step's pitch (the
    thud);
  - filter: the cutoff rises to `vcf_env * velocity` octaves above `vcf_cutoff` and falls back;
  - level: the VCA rises to the hit's peak and falls towards silence.
- **Attack**, one switch for the level and the filter, always rising from where they are, each rise along half a
  cosine (smooth at both ends: the VCA comes after the ladder, so a straight line's corners would click). The times
  are 10-to-90% rises:
  - Fast: both rise in 1 ms.
  - Slow: the level rises in 100 ms and the filter in 500 ms, so a held note breathes rather than blooms.
  - A hit softer than the current level glides down to its peak over 4 ms. So no hit ever steps the level: it moves
    at most 0.042 between samples.
- **Velocity** sets each hit's peak level (linearly, within 0.3 dB) and scales its filter swing: at velocity 0.5 a hit's
  power above 200 Hz in its first 20 ms is at least 6 dB lower than at 1. A velocity of 0 is a rest, and a ringing note
  carries on through it.
- **Short decays roll, long ones hold**, judged on one note at velocity 1, 115 ms after its peak (one 1/16 at 130 BPM):
  - a rolling patch (level decay up to 0.5 s) has fallen at least 10 dB, falls 20 dB within 60 to 250 ms and 40 dB
    within 600 ms, and in a render drops at least 8 dB before the next hit;
  - a held patch (level decay 2 s or more) has fallen at most 3 dB.

### 1.5 The sequencer and its drift

- **8 steps**, each with a pitch (`-12` to `+12` semitones from the root) and a velocity (0 to 1). `seq_length` plays
  the first 1 to 8 of them, so 5 or 7 steps can turn slowly against the 4/4.
- **Rate**: one step per 1 bar, 1/2, 1/4, 1/8, 1/16, 1/32, 1/8T or 1/16T of the clock.
- **On the grid**: while the host plays, step `n` starts at beat `n * step length` of the host's position, within one
  sample at every rate and from 60 to 200 BPM, so a looped bar replays exactly, drift included.
- **Modes**:
  - Key (the default): the pattern plays while a MIDI key is held, rooted on the latest held key. A key pressed from
    silence sounds its step at once.
  - Free: the pattern plays whenever the clock runs, rooted on the last key pressed, or on `seq_root` before any.
  - Notes: no pattern. Each MIDI note is one hit, at its own pitch and velocity.
- Each hit takes its root and its step's pitch when it starts; the pitch holds until the next hit.
- **Drift** makes the pattern wander a little and come home, as a fixed function of the step count:
  - The pattern's passes are grouped into arcs of 16. In each arc, up to `round(3 * drift)` steps change, the count
    rising from none at the arc's start to its most in the middle and back, by at most one step per pass.
  - Only sounding steps drift: rests stay rests, so the beats a pattern leaves to the kick stay empty. A changed step
    goes up an octave, up a fifth or down a fourth, or becomes a ghost hit at half its velocity. A move that would
    leave the steps' range of 12 semitones either side of the root, or take the sub under E1 (41.2 Hz, the bottom of
    its range), takes the same note an octave the other way. Each arc picks its own steps and changes.
  - At drift 0 every pass is the home pattern; at the default (0.7) two steps wander through the middle of each arc.

### 1.6 The rumble: the room under the kick

- A ghost kick (a 150 to 45 Hz drop) fires on the `rumble_rate` grid while the clock runs and feeds a dark mono reverb
  whose delays drift slowly, then saturation, a 24 dB/octave low-pass at `rumble_tone` (after the saturation, so its
  harmonics never reach the room) and the duck, which falls at every point of the same grid. It is never heard dry,
  and `rumble_send` feeds the voice into the same reverb.
- **Dark**: at the default tone, at least 97% of its power is below 150 Hz, its centroid is at most 85 Hz, and the band
  above 500 Hz is at least 60 dB under the total.
- **It breathes**: 12 ms after each kick it is at least 10 dB under its level just before the kick, and it is back
  within 3 dB by 400 ms. The duck's 8 ms attack never splatters: no 20 ms frame puts more than -25 dB of its power
  above 300 Hz.
- **It moves**: the reverb's delays drift slowly, so the bed never repeats exactly from beat to beat (a spread of at
  least 1 dB, 150 to 450 ms after each kick).
- **It never fills the roll**: with the default rumble in, the default patch's whole output still falls at least 8 dB
  before each next hit, as the voice alone does (1.4).

### 1.7 Never acid

- By design: no accent, slide or glide, no saw, no modulated resonance, a filter swing of at most 4 octaves.
- No preset raises the resonance peak above +5 dB (resonance 0.6), and every preset meets the darkness goals of 1.1.

### 1.8 Level and safety

- **Mono**: both channels carry the same signal, sample for sample.
- **Clean**: no clicks (around every hit, a render high-passed at 6 kHz stays under -60 dBFS) and no DC (a render's mean
  is at most -80 dBFS). A DC blocker and a fourth-order 20 Hz high-pass lead the output stage: each hit restarts the
  sub on the same half-cycle, which leaves a low rumble under it that the high-pass takes out.
- **Bounded**: a soft clipper holds every sample within -0.3 dBFS (0.966) and is exactly transparent below -6 dBFS
  (0.5). Going over full scale lights the clip meter.
- **Level**: `master_gain`'s default puts the default pattern's peaks between -12 and -6 dBFS, and every preset plays
  within 2 LU (integrated loudness, BS.1770) of the default pattern, with true peaks at most -1 dBTP.
- **Realtime-safe**: no allocation, lock, file or log in an audio callback, never a NaN, an infinity or a denormal out,
  and a silent voice outputs exact zeros.

### 1.9 Under the kick

A plain 4/4 kick (a sine falling from 190 to 49 Hz) plays over the render.
- A rolling patch meets the kick the same way on every repeat: over bars where the pattern is home, the mix's 20 ms
  envelope spreads at most 0.5 dB from one repeat to the next. The restart rule of 1.2 gives this.
- A held patch starts on the kick's pitch class, so the kick's tail never digs deep holes in it: below its own 80 ms
  running maximum, 60 to 440 ms after each kick, the kick deepens the 20 ms envelope's median dip by at most 6 dB
  over the voice's own, and the worst dip is at most 18 dB.

### 1.10 The presets

- Most of them rolling sub pulses (variations of the decays, drive, pattern and drift) or beds led by the rumble, with
  at most two driving or held ones between them.
- Eight: `Init`, `Roll`, `Hold`, `Drift`, `Rumble`, `Pressure`, `Offbeat` and `Deep`. `Roll` and `Hold` share their
  pattern and pitches and differ only in their decays and attack (and the master level that keeps them equally loud),
  so they compare short and long decays directly.
- Each meets 1.1 to 1.9. On 1/16 lines the thud stays at or under +9 semitones: higher, a dense line's sweeps push its
  energy past 150 Hz.

## 2. Signal flow

```text
 MIDI keys ─► root ──┐   ┌────────────── sequencer (phase 4) ─────────────┐
 host or own clock ──┴──►│ 8 steps: pitch, velocity · rate · length       │──► hits
                         │ Key · Free · Notes · drift                     │
                         └────────────────────────────────────────────────┘
                                       │ pitch, velocity
 ┌────────────────────────────── voice (phases 2, 3) ─────────────────────────────┐
 │ VCO 1 tri|sq ──┬────────────────┐                                              │
 │                └─ FM ─► VCO 2 ──┤                                              │
 │ noise ──────────────────────────┼─► mix ─► drive ─► ladder ─► VCA ─►           │
 │ sine sub, an octave under ──────┘                     ▲          ▲             │
 │ pitch decay ─► VCOs and sub          filter decay ─► cutoff    level decay     │
 └────────────────────────────────────────────────────────────────────────────────┘
          │
          ├────► rumble_send ─► dark reverb ◄── ghost kick on the beat grid
          │                      └─► low-pass ─► drive ─► duck ──┐   rumble (phases 1, 5)
          ▼                                                     │
          + ◄───────────────────────────────────────────────────┘
          └─► DC block ─► master ─► clip ─► L = R                   output (phase 1)
```

## 3. Architecture

### 3.1 Layout

| Path | What | Phases |
|---|---|---|
| `core/` | `undertow_core` (alias `Undertow::core`): the plain C++17 DSP core, headers in `core/include/undertow/`, namespace `undertow`. No JUCE, no host, no UI. | 1 to 6 |
| `plugin/` | The JUCE plugin target `Undertow`: `UndertowProcessor` (a thin wrapper around `undertow::Engine`) and `UndertowEditor` (the window). | 1, 6, 7 |
| `tools/` | `fetch-juce.sh`, `fetch-pluginval.sh`, `spec.sh`, `pluginval.sh`, `auval.sh` (given); `check.sh`, the offline renderer, `render-demos.sh` and `install.sh` (kept from Undertow 1, updated by phases 1, 5 and 7). | |
| `tests/spec/` | The spec tests, given: one executable per file. | given |
| `third_party/doctest/` | doctest 2.5.3, MIT, vendored. | given |
| `third_party/fonts/` | The window's typefaces, Michroma and Geist Mono (Regular and Light), under the SIL Open Font License 1.1, with their licence texts. | given |
| `docs/` | This PRD, `design/window.md`, `specs/phase-N-*.md`. | given |

### 3.2 Build

- CMake 3.22 or newer with presets and the Unix Makefiles generator, C++17, `CMAKE_OSX_DEPLOYMENT_TARGET` 11.0, arm64.
- Two presets: `core` (`build/core`, no JUCE, `UNDERTOW_BUILD_PLUGIN=OFF`) and `plugin` (`build/plugin`,
  `UNDERTOW_BUILD_PLUGIN=ON`), both `RelWithDebInfo`. The core's sources are globbed (`core/src/*.cpp`), so a task
  adds a file without editing `core/CMakeLists.txt`.
- JUCE 8.0.12 comes from the shared dependency cache (`tools/fetch-juce.sh --prefix`), never `FetchContent`.
- The plugin target: `juce_add_plugin(Undertow ...)`, company `Metakosmia`, manufacturer code `Mtks`, plugin code
  `MkU2`, bundle id `audio.metakosmia.undertow2`, product name `Metakosmia Undertow 2`, version 2.0.0, formats VST3 AU
  Standalone, `IS_SYNTH TRUE`, `NEEDS_MIDI_INPUT TRUE`. Its own codes let it install beside Undertow 1, whose
  sessions keep loading their own plugin.
- Validation: pluginval 1.0.4 (`tools/pluginval.sh`) and `auval` for the AU.

### 3.3 Threads and realtime rules

- The audio thread owns `undertow::Engine`. `UndertowProcessor::processBlock` copies the parameter values into it
  (`setParam`), converts the MIDI buffer into a preallocated `undertow::MidiEvent` array and the host position into
  an `undertow::Transport`, and calls `Engine::process`.
- The UI thread never calls into the engine's processing. It reads `Engine::meters()` and the scope through
  lock-free storage. The sequencer's steps are ordinary parameters, so the window edits them as it edits any knob.
- In `process`, `setParam`, `getParam`, `meters` and `reset`: no heap allocation (the spec tests count them), no
  locks, no I/O, no logging, no exceptions. Everything is sized in `prepare`.
- Outputs are always finite; state that decays flushes to exact zero.

### 3.4 The clock

- With a host position (`Transport::valid`), the engine's beat position is the host's ppq, advanced through the
  block at the host's tempo while it plays. Without one (the Standalone app), the engine runs its own clock at
  `seq_bpm` from 0 after `reset()`. The rumble's ghost kick and duck read this clock.
- The sequencer reads it too, with one addition (phase 4): while the host is stopped, a held key still plays,
  counting steps from the key press at the host's tempo.

## 4. Parameters

The host-facing parameters, in host order. Ids are stable once released; the index is the position in this table
and the `undertow::param` enumerator (the id in CamelCase: `vcf_cutoff` is `undertow::param::VcfCutoff`). "Centre"
is the value at half knob travel (JUCE `setSkewForCentre`); 0 means a linear range. Choice parameters run from 0 to
their number of choices minus one. `tests/spec/p1/params_test.cpp` holds the same table.

| # | id | name | kind | min | max | default | centre | unit |
|---|---|---|---|---|---|---|---|---|
| 0 | `vco1_wave` | VCO 1 Wave | Choice | 0 | 1 | 0 | 0 |  |
| 1 | `vco1_level` | VCO 1 Level | Float | 0 | 1 | 0.45 | 0 |  |
| 2 | `vco2_wave` | VCO 2 Wave | Choice | 0 | 1 | 1 | 0 |  |
| 3 | `vco2_tune` | VCO 2 Tune | Float | -24 | 24 | -12 | 0 | st |
| 4 | `vco2_level` | VCO 2 Level | Float | 0 | 1 | 0.28 | 0 |  |
| 5 | `vco_fm` | 1 to 2 FM | Float | 0 | 1 | 0.15 | 0 |  |
| 6 | `noise_level` | Noise | Float | 0 | 1 | 0.05 | 0 |  |
| 7 | `sub_level` | Sub | Float | 0 | 1 | 1 | 0 |  |
| 8 | `sub_octave` | Sub Octave | Choice | 0 | 1 | 1 | 0 |  |
| 9 | `pitch_env` | Pitch Env | Float | 0 | 24 | 7 | 0 | st |
| 10 | `pitch_decay` | Pitch Decay | Float | 5 | 1000 | 75 | 60 | ms |
| 11 | `vcf_cutoff` | Cutoff | Float | 20 | 8000 | 140 | 300 | Hz |
| 12 | `vcf_res` | Resonance | Float | 0 | 1 | 0.32 | 0 |  |
| 13 | `vcf_drive` | Drive | Float | 0 | 1 | 0.4 | 0 |  |
| 14 | `vcf_env` | VCF Env | Float | 0 | 4 | 2 | 0 | oct |
| 15 | `vcf_decay` | VCF Decay | Float | 10 | 10000 | 275 | 300 | ms |
| 16 | `vca_attack` | Attack | Choice | 0 | 1 | 0 | 0 |  |
| 17 | `vca_decay` | VCA Decay | Float | 10 | 20000 | 390 | 300 | ms |
| 18 | `seq_mode` | Mode | Choice | 0 | 2 | 0 | 0 |  |
| 19 | `seq_rate` | Rate | Choice | 0 | 7 | 4 | 0 |  |
| 20 | `seq_length` | Length | Int | 1 | 8 | 8 | 0 |  |
| 21 | `seq_drift` | Drift | Float | 0 | 1 | 0.7 | 0 |  |
| 22 | `seq_root` | Root | Int | 24 | 60 | 43 | 0 |  |
| 23 | `seq_bpm` | Tempo | Float | 60 | 200 | 130 | 0 | bpm |
| 24 | `step1_pitch` | Step 1 Pitch | Int | -12 | 12 | 0 | 0 | st |
| 25 | `step2_pitch` | Step 2 Pitch | Int | -12 | 12 | 0 | 0 | st |
| 26 | `step3_pitch` | Step 3 Pitch | Int | -12 | 12 | 0 | 0 | st |
| 27 | `step4_pitch` | Step 4 Pitch | Int | -12 | 12 | 0 | 0 | st |
| 28 | `step5_pitch` | Step 5 Pitch | Int | -12 | 12 | 0 | 0 | st |
| 29 | `step6_pitch` | Step 6 Pitch | Int | -12 | 12 | 0 | 0 | st |
| 30 | `step7_pitch` | Step 7 Pitch | Int | -12 | 12 | 0 | 0 | st |
| 31 | `step8_pitch` | Step 8 Pitch | Int | -12 | 12 | 0 | 0 | st |
| 32 | `step1_vel` | Step 1 Velocity | Float | 0 | 1 | 0 | 0 |  |
| 33 | `step2_vel` | Step 2 Velocity | Float | 0 | 1 | 0.22 | 0 |  |
| 34 | `step3_vel` | Step 3 Velocity | Float | 0 | 1 | 1 | 0 |  |
| 35 | `step4_vel` | Step 4 Velocity | Float | 0 | 1 | 0.55 | 0 |  |
| 36 | `step5_vel` | Step 5 Velocity | Float | 0 | 1 | 0 | 0 |  |
| 37 | `step6_vel` | Step 6 Velocity | Float | 0 | 1 | 0.25 | 0 |  |
| 38 | `step7_vel` | Step 7 Velocity | Float | 0 | 1 | 0.9 | 0 |  |
| 39 | `step8_vel` | Step 8 Velocity | Float | 0 | 1 | 0.6 | 0 |  |
| 40 | `rumble_level` | Rumble | Float | 0 | 1 | 0.3 | 0 |  |
| 41 | `rumble_send` | Rumble Send | Float | 0 | 1 | 0.2 | 0 |  |
| 42 | `rumble_kick` | Ghost Kick | Float | 0 | 1 | 0.6 | 0 |  |
| 43 | `rumble_rate` | Kick Rate | Choice | 0 | 3 | 0 | 0 |  |
| 44 | `rumble_decay` | Rumble Decay | Float | 0.2 | 8 | 4.5 | 2 | s |
| 45 | `rumble_tone` | Rumble Tone | Float | 40 | 250 | 100 | 100 | Hz |
| 46 | `rumble_drive` | Rumble Drive | Float | 0 | 1 | 0.5 | 0 |  |
| 47 | `rumble_duck` | Duck | Float | 0 | 1 | 0.88 | 0 |  |
| 48 | `master_gain` | Master | Float | -24 | 6 | -12 | 0 | dB |

The choices, in order:

- `vco1_wave`, `vco2_wave`: Triangle, Square
- `sub_octave`: 0 oct, -1 oct
- `vca_attack`: Fast, Slow
- `seq_mode`: Key, Free, Notes
- `seq_rate`: 1 bar, 1/2, 1/4, 1/8, 1/16, 1/32, 1/8T, 1/16T (a step of 4, 2, 1, 0.5, 0.25, 0.125, 1/3 and 1/6
  beats)
- `rumble_rate`: 1/4, 1/2, 1 bar, 1/8

The defaults are the init sound: a rolling sub at 1/16 in G (VCO 1 at G2, the sub at G1, 49 Hz), the beats left to
the kick (steps 1 and 5 are rests), a short thud at each hit, and two steps drifting through the middle of every arc.

## 5. The window

ROSEDUST phosphor, laid out with the DFAM's panel logic: the sequencer's 8 steps in columns across the top, each a
glass cell (a small scope of its last hit, lit while it plays) over its pitch and velocity knobs, with the pattern's
settings and the drift arc beside them; then the VCO, VCF and VCA sections left to right, each ending in its decay,
with the spectrum and the sub's pitch beside them; then the rumble and the output. One look, readable at a glance:
from across a room you see whether it is silent, rolling or clipping; up close you read every value.
`docs/design/window.md` (phase 6) gives the layout.

## 6. Phases

| Phase | Spec | Builds | Depends on | Size |
|---|---|---|---|---|
| 1 | `specs/phase-1-foundation.md` | The parameter table, Undertow 2's identity, the engine skeleton with the kept rumble and output stage (mono), the parts it replaces retired, the plugin with a placeholder window, pluginval | nothing | M: 3 tasks |
| 2 | `specs/phase-2-sources.md` | The two VCOs (triangle and square, clear of aliases, linear FM), noise and the sine sub; the ladder | 1 | M: 2 tasks |
| 3 | `specs/phase-3-voice.md` | The three decays and the VCA, the voice assembled, played by MIDI notes (Notes mode) | 2 | M: 3 tasks |
| 4 | `specs/phase-4-sequencer.md` | The 8-step sequencer: the grid, rates, length, Key and Free, the root, drift | 3 | M: 3 tasks |
| 5 | `specs/phase-5-sound.md` | The darker rumble, the presets, a render of each | 4 | M: 3 tasks |
| 6 | `specs/phase-6-window.md` | The window | 5 | L: 6 tasks |
| 7 | `specs/phase-7-ship.md` | Strict pluginval and the install; auval, run by hand after a real install | 6 | S: 2 tasks |

Run them in order, 1 to 7.

## 7. How the checks work

- `tools/spec.sh <phase> <name> [<filter>]` builds and runs one spec executable, `tests/spec/p<phase>/<name>_test.cpp`,
  optionally only the doctest test cases matching a filter (`'P2.03*'`). Core tests build with the `core` preset in
  seconds; tests marked `// spec-preset: plugin` build with the `plugin` preset.
- One executable per test file means a task's check compiles only what that task touches: two tasks running in
  parallel never break each other's check.
- A test file names the files it needs (`// spec-needs: core/include/undertow/ladder.h`). Once they all exist the test
  joins the default build and `ctest`, so `tools/check.sh` runs every spec test whose code is in place, and a later
  phase can never quietly break an earlier one.
- Failure messages say what to change ("the ladder is -6.1 dB at the cutoff at resonance 0; a 4-pole ladder is
  -12 dB there"), because the fixing model reads them.
- Timing budgets count the thread's CPU time and keep a 10x margin over the reference implementation.

## 8. Out of scope

- Polyphony, glide, accent, slide, LFOs and pitch bend.
- Stereo width (the instrument is mono), a sidechain input (the ghost kick replaces it).
- Windows and Linux builds; AAX; CLAP.
- Preset browsing beyond the host's program list; cloud anything.
- Code signing with a Developer ID and notarization (local ad-hoc signing only).

## 9. Licensing

Undertow is licensed under the GNU Affero General Public License, version 3 only (`AGPL-3.0-only`, `LICENSE`). It is
built with JUCE 8, used under JUCE's AGPLv3 licence option. doctest is MIT (`third_party/doctest/LICENSE.txt`). The
window's typefaces, Michroma and Geist Mono, are under the SIL Open Font License 1.1 (`third_party/fonts/`).
