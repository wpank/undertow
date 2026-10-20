# Phase 2.1: voice fixes

**Goal**: six faults a listening test found in phase 1's voice, fixed before the sequencer plays it. A sustain change
no longer clicks; a glide keeps its time and its pitch however notes overlap or the glide mode changes; the meters
survive an empty block; the filter keeps its resonance and stays linear at the levels the voice plays; and a note
above Nyquist is silent.

**Depends on**: phase 2 (merged). Run it before phase 3.

**Size**: S, 5 tasks, about 50 lines.

---

## Scope

In scope: fixes inside `core/src/envelope.cpp`, `voice.cpp`, `engine.cpp`, `filter.cpp` and `oscillator.cpp`, and their
headers' private members. No new files, and no change to any public interface.

Out of scope: everything listed under "Not now", the sequencer (phase 3) and the editor (phase 4).

## Context

- The listening test rendered phase 1's voice offline through `undertow::Engine`, the way the plugin's `processBlock`
  drives it: parameters pushed at every block, MIDI at sample offsets, a moving 128 BPM transport, and blocks of 512,
  97 and 1 to 1024 samples, zero-length calls included. Each check below reproduces one of its findings on the class
  the fault lives in. The numbers quoted under "What to fix" are the checks' own readings of phase 1's code.
- The spec tests: `tests/spec/p2_1/envelope_test.cpp`, `voice_test.cpp`, `engine_test.cpp`, `filter_test.cpp` and
  `oscillator_test.cpp`. Change the code, never the tests.
- These tests need no new header, so they join `tools/check.sh` as soon as they are on main: check.sh fails on the
  base until all five tasks are done. Phases 0 to 2's checks must keep passing.

## What to fix

1. **The envelope** (`envelope.h`, `envelope.cpp`): a sustain change glides.
   - In the Sustain stage the level is set to the sustain level on every sample, so turning `amp_sustain` (or
     `filt_sustain`) while a note is held moves the level at once: 0.7 in one sample for 1 to 0.3, a click.
   - Follow a sustain change with a one-pole of about 5 ms, `level += (sustain - level) * a` with
     `a = 1 - exp(-1 / (0.005 * sampleRate))`. Hold the level exactly once it is within 1e-5 of the sustain level,
     phase 1's rule for the decay.
   - Not the decay time: a 1 ms decay would still click, and a 300 ms one would lag behind the knob.
2. **The glide** (`voice.h`, `voice.cpp`): a glide keeps its time and its pitch.
   - Releasing a held note that is not the sounding one must change nothing. Now the voice restarts its glide from
     where it is, over a fresh `glide_ms`. A piano-roll slide, where the old note overlaps the new one, then lands
     late: with `glide_ms` 200 and a 50 ms overlap it lands at 250 ms, and it is 5.25 semitones up at 100 ms, where
     a straight line is at 6. Only releasing the sounding note returns to the newest held note, gliding as before.
   - A note that does not glide (`glide_mode` Off, or `glide_ms` 0) must land exactly on its pitch, also when a glide
     was under way. Now the glide's position is kept, so the new note stays part way between the old pitch and its
     own: 650 cents flat for a legato note, 300 cents off on a return to a held note, until the next detached note.
3. **The meters** (`engine.cpp`): a zero-length block changes nothing.
   - `process` with `numSamples` 0 divides the block's sum of squares by 0. The NaN then stays in the smoothed RMS for
     the rest of the session, and the peak meter drops to 0.
   - JUCE's VST3 wrapper calls `processBlock` with 0 samples when a host sends an empty block with its output bus
     attached. Return at once when there are no samples.
4. **The filter** (`filter.h`, `filter.cpp`): at drive 0, linear at the levels the voice gives it (the oscillator's
   ±1 times `osc_level`, up to 1).
   - Now each stage's state is soft-limited from 1.0. At resonance 0.8 the gain at the cutoff is 22.8 dB for a 0.1
     sine but 4.5 dB for a 1.0 sine. A full-scale saw is 32, 17 and 2.5 dB from linear at resonance 0, 0.35 (the
     default) and 0.8: the resonance a techno bass wants is flattened.
   - Limit the state only beyond those levels, and keep a resonance law that leaves room for it. Scaling both
     stages' damping by resonance, as now, gains 17x (+25 dB) at the cutoff at 0.8. A full-scale input there would
     need limits above 17, while P1.07 keeps the output within ±4.
   - Phase 1's law fits: the 0.7654 stage's damping falls towards 0.02 and the 1.8478 stage stays fixed. That gains
     3.2x (+10 dB) at the cutoff at 0.8, 13 dB over resonance 0 (P1.06 asks for 6).
   - With that law and the state limited from 8 instead of 1, P2.1.05 reads 2.1 dB, P2.1.06 reads -123 dB at worst,
     and P1.05 to P1.07 still pass.
5. **The oscillator** (`oscillator.h`, `oscillator.cpp`): clamped at Nyquist, as phase 1 asked.
   - `setFrequency` doesn't clamp. Note 127 with a 12-semitone bend at `osc_octave` +1 asks for 50.2 kHz and plays a
     2.2 kHz alias; at 26 kHz the saw's alias at 18 kHz is -41 dB. Phase 2's LFO 2 on Pitch reaches these notes more
     easily.
   - Clamp the frequency to 0 .. sampleRate / 2. At exactly Nyquist the PolyBLEP saw and a 50% square cancel to
     silence, and a narrower pulse leaves only DC (phase 2's DC blocker removes it) and the Nyquist tone. A clamp at
     20 kHz is not the same: it plays a 20 kHz tone.

## API

No change: every class keeps phase 1's interface (`docs/specs/phase-1-voice.md`, "API").

## Acceptance checks

| Check | Command | Proves |
|---|---|---|
| P2.1.01 | `tools/spec.sh 2_1 envelope 'P2.1.01*'` | A sustain change while a note is held (1 to 0.3, back to 1, to 0, to 0.6) moves the level at most 0.02 a sample, is within 0.005 of the new level after 50 ms and exact after 100 ms; the release still ends in an exact 0 |
| P2.1.02 | `tools/spec.sh 2_1 voice 'P2.1.02*'` | With glide_ms 200, a legato note whose previous note is released 50 ms into the glide lands at 200 ms (within 3 ms), on a straight line (within 0.1 semitone at 50, 100 and 150 ms) |
| P2.1.03 | `tools/spec.sh 2_1 voice 'P2.1.03*'` | After glide_mode goes Off, or glide_ms goes to 0, halfway through a glide, the next legato note and a return to the held note play their own pitch within 1 cent, at once and 0.5 s later |
| P2.1.04 | `tools/spec.sh 2_1 engine 'P2.1.04*'` | Zero-length process calls between the blocks of a moving-transport render leave meters() finite and unchanged, and the audio identical to a render without them |
| P2.1.05 | `tools/spec.sh 2_1 filter 'P2.1.05*'` | At resonance 0.8 and drive 0 the gain at the cutoff for a 1.0 sine is within 3 dB of a 0.1 sine's, and 6 dB over the gain at resonance 0 |
| P2.1.06 | `tools/spec.sh 2_1 filter 'P2.1.06*'` | At drive 0 a full-scale 110 Hz saw through cutoff 440 Hz is within -60 dB of 100 times the same saw at 0.01, at resonance 0, 0.35 and 0.8 |
| P2.1.07 | `tools/spec.sh 2_1 oscillator 'P2.1.07*'` | At 24, 26, 30, 50.2 and 96 kHz and at 1 MHz, the saw, the square and a 10% pulse have no component from 20 Hz to 20 kHz above -60 dB |
| P2.1.08 | `tools/spec.sh 1 filter` | Phase 1's filter checks (P1.05 to P1.07) still pass. A regression check: it passes on the base too |
| P2.1.09 | `tools/spec.sh 2_1 engine 'P2.1.09*'` | The saturation keeps phase 2's blend, `x + d (tanh(g x) / g - x)`: at drive 0.2 and 0.5 a sine of amplitude 2 keeps the blend's fundamental (within 0.3 dB), drive 0 passes it unchanged, and drive 0.01 moves it less than 0.1 dB. A regression check: it passes on the base too |

## Rules every task keeps

- The rules of phases 0 to 2 still hold: never edit `tests/spec/`, `third_party/`, `docs/` or the given tools;
  `core/` stays plain C++17; no allocation, lock, I/O or logging in anything `process` calls.
- Fix the fault and nothing else: every other behaviour of phases 1 and 2, and every check that passes now, stays as
  it is.

## Also required, not checked automatically

- `filt_sustain` glides the same way as `amp_sustain` (both are `Envelope`s).
- Listening: a held note with `amp_sustain` swept by hand has no zipper or click; a 303-style line at resonance 0.8
  squelches at full `osc_level` as it does at a low one.

## Not now

- **Drive costs level.** Full `filter_drive` lowers the output by 12 dB (-0.9, -3.1, -6.5 and -12.2 dB at drive 0.25,
  0.5, 0.75 and 1), because `tanh(11x) / 11` caps the signal. Phase 2's `sat_drive` is specified the same way, with
  `master_gain` making up the level. A make-up gain that follows the drive is a sound-design choice for later.
- The cutoff's `filter_env` and `filter_track` terms are not smoothed (only the base cutoff is). No click was
  measured on a held note.
- Small departures from phase 1's spec that no listening test heard: the filter keeps its state in `float` (phase 1
  asks for `double`), `setPulseWidth` clamps to 0.01 .. 0.99 (the parameter keeps it within 0.05 .. 0.95), and
  `Oscillator` has no `phase()`.
- Aliasing between 12 and 20 kHz of notes near the top of the range: inaudible on bass notes, and outside the
  PRD's measure (below 4 kHz).

## Notes for the planner

- Five tasks, one per test file. They share no file, so all five run in parallel. Each task's verify steps are
  exactly the checks named for it, with their commands unchanged:
  1. **The envelope** (`envelope.h`, `envelope.cpp`): P2.1.01. About 10 lines.
  2. **The glide** (`voice.h`, `voice.cpp`): P2.1.02 and P2.1.03. About 15 lines.
  3. **The meters** (`engine.cpp`): P2.1.04, and P2.1.09, a regression check of the saturation the engine runs. About 3
     lines.
  4. **The filter** (`filter.h`, `filter.cpp`): P2.1.05, P2.1.06 and P2.1.08. About 20 lines.
  5. **The oscillator** (`oscillator.h`, `oscillator.cpp`): P2.1.07. About 3 lines.
- P2.1.08 and P2.1.09 are regression checks (`expect = "pass_on_base"`). P2.1.08 guards the filter task against a fix
  that raises every limit instead of changing the resonance law: such a fix can pass P2.1.05 and P2.1.06, but not
  P1.07. P2.1.09 holds phase 2's saturation to the blend it was built as, which the -14 dB default rests on; phase 2's
  P2.10 checked only drives 0 and 1, where the blend and the curve alone agree.
- Phase 2 edits `engine.cpp` and `voice.cpp`. If it fixed one of these faults in passing, that check passes on the
  base, and its task has nothing to do: leave the task out of the plan.
- Every task builds C++: its agent needs `bash` and `edit_file`, to run `cmake` and its own `tools/spec.sh` check
  before it hands off. Leave `allowed_tools` out, or include `bash`, `edit_file`, `read_file`, `write_file`, `grep`
  and `glob`.
- `tools/check.sh` (every present spec test, phases 0 to 2.1) is the plan's final check, not any one task's.
