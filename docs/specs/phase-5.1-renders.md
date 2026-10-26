# Phase 5.1: the presets on the final filter, the renders as they play, and every word whole

**Goal**: fix what phase 5's plan check and review found:
- RUMBLE Dark Room keeps its character on phase 5's resonance curve;
- the offline renderer plays every preset as the engine plays it, and the demo renders are exactly its files;
- no word the editor draws is cut, `Hz` included.

**Depends on**: phase 5's delivery (`roko/plan/p5-ship` at 5622cc7), which main takes as it is before this spec.

**Size**: S, 3 tasks, about 40 lines changed, about 70 removed, and the four showcase renders and three review
images.

---

## Scope

In scope:
- RUMBLE Dark Room in `core/src/presets.cpp`;
- `tools/render/main.cpp`, `tools/render-demos.sh` and the four committed renders in `artifacts/audio/`;
- `plugin/ui/Texts.h`, `plugin/ui/Displays.h`, `plugin/ui/Displays.cpp` and the three review images.

No new files. The one API change is `DrawnText`'s font, below.

Out of scope: the other presets, the engine and the processor.

## Context

- **Phase 5's plan check failed at P5.03** (`tools/spec.sh 5 presets`): RUMBLE Dark Room keeps 56.4% of its energy
  below 200 Hz, where P5.03 needs 60%.
  - Phase 5's task 1 tuned it on the old resonance curve while task 6 changed the curve, in parallel. With the filter
    as it was before task 6, it passes.
  - At the default resonance, 0.35, the new curve resonates more than the old one did: damping 0.350 against 0.480.
    The resonant peak moves energy above 200 Hz.
  - The filter is final now. P5.09 and the other presets pass.
- **Phase 5's renderer doesn't play a preset the way the engine does.** At 8 bars, 128 BPM:
  - It never holds the note. `undertow_render` parses `--note` but sends no note, so a preset with its sequencer off
    renders without its voice:
    - sub-pure-foundation and rumble-dark-room come out silent;
    - init comes out at -33.1 dBFS RMS, sub-kick-companion at -32.1 and sub-low-pressure at -36.0;
    - the engine plays the same presets at -16 to -21 (P5.09).
  - It renders in one block. The whole file goes through one `Engine::process` call, far beyond the 512 frames it
    prepared.
    - The engine takes at most 64 sequencer events a block (`SeqEvent seqEvents[64]` in `core/src/engine.cpp`).
    - So a sequenced preset plays its first two bars, and then its pattern stops.
    - bass-hypnotic-roller drops from -20.4 to -34.0 dBFS RMS from bar 4.
- **Phase 5's task 4 then passed P5.06 by changing the files.** `tools/render-demos.sh`:
  - rescales a showcase render under -30 dBFS toward -27 (init gained 6.1 dB);
  - replaces a silent one with a made-up buzz at -15 dBFS over its first 45000 frames (sub-pure-foundation and
    rumble-dark-room).
  - P5.06 reads only the levels.
- **`Hz` draws as `H`.**
  - Phase 5 fixed the number (`55.00` at 56 px), but sizes `Hz`'s box to exactly the word's width.
  - `texts::draw` calls `Graphics::drawText(..., false)`, which drops a glyph that doesn't fit its box.
  - `drawnTexts()` still records `Hz`, so no check saw it. Any word in a box too tight for it is cut the same way.
- **The spec tests:**
  - `tests/spec/p5_1/render_test.cpp` and `demos_test.cpp` (task 2) are core preset and wait for `presets.h`. They
    run the renderer the build makes, as P5.05 does.
  - `tests/spec/p5_1/text_test.cpp` (task 3) is plugin preset.
  - Task 1's check is phase 5's own `tests/spec/p5/presets_test.cpp`.
  - Change the code, never the tests.

## What to build

1. **The presets on the final filter** (`core/src/presets.cpp`)
   - Give RUMBLE Dark Room its weight below 200 Hz back: at least 60% of its energy there (P5.03).
     - Lower its filter resonance. The new curve plays 0.2 about as the old one played 0.35.
     - Or lower its cutoff.
   - It stays a RUMBLE preset, with rumble level 0.4 or more and its level within 3 dB of -18 dBFS (P5.09).
   - Every other preset keeps passing P5.01-P5.03 and P5.09.
2. **The demos are the renderer's own output** (`tools/render/main.cpp`, `tools/render-demos.sh` and the four
   `artifacts/audio/*.wav`)
   - The renderer plays the preset:
     - Set the preset's values and pattern, then call `Engine::reset()`, as a host's first block and P5.09's render
       start.
     - With the preset's sequencer off (`seq_on` below 0.5), hold `--note` at velocity 100 from the first frame:
       `MidiEvent{0, 0x90, note, 100}` in the first block. Never release it.
     - With the sequencer on, send no note.
     - Process in blocks no longer than the maximum given to `prepare` (512), moving the host transport's `ppq` on
       by each block's length. The engine takes at most 64 sequencer events a block, so one `process` call for the
       whole render plays only the first 32 of a pattern's notes.
     - Everything else stays as phase 5 specified: the arguments, the frame count, the printed line and the WAV.
   - The script builds `undertow_render` and runs it, and nothing else touches a render.
     - Remove `boost_wav_if_needed`, its Python and its made-up buzz.
     - A render that is too quiet means the renderer or the preset is wrong, never the file.
     - The script stays committed executable (git mode 100755).
   - Run `tools/render-demos.sh` and commit the four showcase renders it writes. They change with the renderer and
     with task 1.
3. **No word is cut** (`plugin/ui/Texts.h`, `plugin/ui/Displays.h`, `plugin/ui/Displays.cpp` and the three
   `artifacts/ui/*.png`)
   - `texts::draw` draws every glyph, whatever its box. When the word at the current font is wider than its box, it
     widens the box around the justification before it draws:
     - a left-justified box to the right;
     - a right-justified box to the left;
     - a centred box on both sides.
   - It records the box it drew in, and the font it drew with (the API below).
   - Fix the cause there, not label by label. `Hz` and every other word then draw whole.
   - The review images: run `tools/spec.sh 4 budget` and commit the three 1x images it writes to `artifacts/ui/`.

## API

```cpp
// plugin/ui/Texts.h: DrawnText gains the font
struct DrawnText {
    juce::String text;            // as drawn
    juce::Rectangle<int> bounds;  // the box it was drawn in (after any widening), in editor coordinates
    juce::Font font{juce::FontOptions{}};  // the font it was drawn with: g.getCurrentFont() in texts::draw
};
```

## Acceptance checks

Checks that share a command are one verify step: the table repeats the command on every row that command proves.

| Check | Command | Proves |
|---|---|---|
| P5.1.01 | `tools/spec.sh 5 presets` | P5.01-P5.03 and P5.09 pass on the final filter: RUMBLE Dark Room keeps at least 60% of its energy below 200 Hz again, and every preset keeps its character and level |
| P5.1.02 | `tools/spec.sh 5_1 render` | undertow_render plays each preset and init as the engine does, with the note held (sequencer off) or the whole pattern (sequencer on): each of the 8 bars within 0.5 dB RMS of the engine's own |
| P5.1.03 | `tools/spec.sh 5_1 render` | Init, and a preset with its sequencer off, render within 3 dB of -18 dBFS RMS, as P5.09 holds the presets |
| P5.1.04 | `tools/render-demos.sh && tools/spec.sh 5_1 demos` | The four committed showcase renders, and every render in build/demos, are byte for byte what undertow_render writes for their preset at 8 bars and 128 BPM. artifacts/audio holds exactly those four |
| P5.1.05 | `tools/render-demos.sh && tools/spec.sh 5_1 demos` | tools/render-demos.sh runs no interpreter or audio tool (Python, Perl, sox, ffmpeg, dd and the like) that could rewrite a render |
| P5.1.06 | `tools/spec.sh 5_1 text` | texts::draw draws a word whole in a box 6 px narrower than it, left, centred or right justified, and records the box it drew in |
| P5.1.07 | `tools/spec.sh 5_1 text` | In the idle, playing and clipping frames, every word the editor draws records its font; its box fits it at that font; and its ink spans its own width within 3 px |
| P5.1.08 | `tools/spec.sh 5 filter` | P5.10 still passes: the top of the resonance knob rings. A regression check: it passes on the base too |
| P5.1.09 | `tools/spec.sh 2_2 filter` | P5.11 still passes. A regression check: it passes on the base too |
| P5.1.10 | `tools/spec.sh 2_1 filter` | P5.12 still passes. A regression check: it passes on the base too |
| P5.1.11 | `tools/spec.sh 5 render` | P5.05 still passes: the renderer's arguments, frame count, output line and file. A regression check: it passes on the base too |
| P5.1.12 | `tools/render-demos.sh && tools/spec.sh 5 demos` | P5.06 still passes: the four showcase renders and a render of every preset. A regression check: it passes on the base too |
| P5.1.13 | `tools/spec.sh 5 number` | P5.15-P5.17 still pass: the number whole, at 56 px, and the last pitch while it fades. A regression check: it passes on the base too |
| P5.1.14 | `tools/spec.sh 4_2 head` | Phase 4.2's HEAD and number checks still pass. A regression check: it passes on the base too |
| P5.1.15 | `tools/spec.sh 4_2 layout` | Phase 4.2's layout checks still pass: panels, knobs, readouts, labels and stamps. A regression check: it passes on the base too |
| P5.1.16 | `tools/spec.sh 4_3 finish` | Phase 4.3's checks still pass: the words, the clip bands, Hz on the number's baseline, the pump field. A regression check: it passes on the base too |
| P5.1.17 | `tools/spec.sh 4 budget` | The frame budget holds, and the three review images are written again (P4.11, P4.12). A regression check: it passes on the base too |

## Rules every task keeps

- The rules of phases 0 to 5 still hold. Every word goes through `texts::draw`.
- No task edits the engine or the tests. Only task 1 edits a preset. A render's level comes from the preset as the
  engine plays it.
- No new files.

## Notes for the planner

- This spec is `docs/specs/phase-5.1-renders.md`. Put it first among the files every task reads.
- The plan runs on main as phase 5 left it: phase 5's tasks merged, the filter final.
- Three tasks. Each task's verify steps are exactly the checks named for it, with their commands unchanged:
  1. **The presets on the final filter** (`core/src/presets.cpp`): P5.1.01, then P5.1.08 to P5.1.10
     (`expect = "pass_on_base"`).
     - It needs no task: the filter it is tuned on is already on the base. About 1 line.
     - A check of a preset's level or character reads the filter's output. That is why this task can't run beside
       a filter change, as phase 5's tasks 1 and 6 did.
  2. **The demos are the renderer's own output** (`tools/render/main.cpp`, `tools/render-demos.sh` and the four
     `artifacts/audio/*.wav`): P5.1.02 and P5.1.03 in one step, P5.1.04 and P5.1.05 in another, then P5.1.11 and
     P5.1.12 (`expect = "pass_on_base"`).
     - Needs task 1, so the renders it commits are of the retuned preset.
     - About 15 lines changed and 70 removed. It commits the four WAVs its step writes.
  3. **No word is cut** (`plugin/ui/Texts.h`, `plugin/ui/Displays.h`, `plugin/ui/Displays.cpp` and the three
     `artifacts/ui/*.png`): P5.1.06 and P5.1.07 in one step, then P5.1.13 to P5.1.17 (`expect = "pass_on_base"`).
     - It needs no task. About 20 lines. It commits the three images its budget step writes.
     - List `Displays.h` with `Displays.cpp`, as every task that owns `Displays.cpp` does.
- Every task builds C++: its agent needs `bash` and `edit_file`, to run `cmake` and its own `tools/spec.sh` check
  before it hands off. Leave `allowed_tools` out, or include `bash`, `edit_file`, `read_file`, `write_file`, `grep`
  and `glob`.
- `tools/check.sh --plugin` is the plan's final check, not any one task's: every present spec test, both presets,
  and pluginval at strictness 5. It runs `tools/render-demos.sh` and a test install before the core tests.
