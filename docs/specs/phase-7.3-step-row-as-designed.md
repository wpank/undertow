# Phase 7.3: the playing step row as designed

**Goal**: the step row reads as `docs/design/window.md` draws it while the pattern plays. After phase 7.2 the window
matches the design within 1 to 3% of its pixels everywhere but there, where a reader notices:
- the lit cell filled flat in a bright rose with a thick frame, twice the design's strength, where the design has a
  dark ember fill fading downwards;
- the traces a thin line, where the design draws a filled band from the middle line to the wave;
- the lit column's knobs glowing as if hovered;
- a dot at 7:30 on a rest's velocity knob, and a lit dot at 12 o'clock on the lit pitch knobs;
- rests outlined in 2 px dashes, and cells that haven't played yet empty, where the design draws a flat line.

Last, the review images and pluginval's strict log are made again from the final build.

**Depends on**: phase 7.2.

**Size**: S, 3 tasks: the step cells and the knobs in parallel (about 45 and 10 lines), then the review images and the
log.

---

## Scope

In scope:
- the window's drawing:
  - `plugin/ui/Steps.cpp` (the step cells);
  - `plugin/ui/Controls.cpp` (the knobs);
- the review images `artifacts/ui/window-idle.png`, `artifacts/ui/window-playing.png` and
  `artifacts/ui/window-clipping.png`, and `artifacts/reports/pluginval-strict.txt`, made again.

Given by this phase's seed, never edited by a task:
- `tests/spec/p7_3/cells_test.cpp` and `knobs_test.cpp`;
- `docs/design/window.md`, which now says in so many words that each trace column's min and max are taken with 0 (the
  band stands on the middle line), that the band's edges are its brightest part, that the lit cell has no fill or frame
  but its ember fill, that a lit knob never glows, and that a knob at its arc's start draws nothing (sections 3 and
  4.1).

Out of scope:
- everything else in the window, which phase 7.2's checks hold: this phase changes how the cells and the knobs are
  drawn, never where or what;
- the sweep head, the traces' fade with age and the cells' faces, which stay as phase 7.2 drew them;
- the spectrum's axis line (`plugin/ui/Console.cpp`);
- `plugin/ui/Controls.h`, `plugin/ui/Steps.h`, `plugin/ui/Theme.h` and the engine.

## Context

- **Why.** Phase 7.2's checks bound the lit cell's fill from below only, and its trace check reads half-tones, which a
  thin line has too. Rendered beside the design, the playing step row differs, each from the code at 565765d:
  - **The lit cell** (`Steps::drawCell`): two `rose_bright` glows are filled across the whole cell, under an ember
    gradient that stays above 30% to the bottom, inside a 2 px `rose_bright` frame at 90%. Its upper strip reads luma
    0.17 against the design's 0.10, and its fill is as strong low in the cell as at the top.
  - **The trace** (`Steps::updateCurrentTrace`): each column's min and max start at 1 and -1, so a column spans only the
    samples it holds. At a sub's pitch a column holds a sliver of the wave, so the band is a thin line. Its edges are
    never drawn.
  - **The knobs** (`Knob::paint`): the glow is drawn while lit as well as while hovered. At value 0 the arc has no
    length but is still stroked with round ends: a dot at 7:30 on a rest's velocity and, from the glow, a dot at 12
    o'clock on a lit pitch at 0.
  - **The rests** (`Steps::drawRestBorder`): the dashed path, already the dashes' outline, is stroked again, so the
    dashes are 2 px.
  - **A cell that hasn't played** draws nothing where the design draws a flat line.
- **The design** (`docs/design/window.md`) holds every number this phase checks. Read sections 3 and 4.1 before
  drawing.
- **The spec tests:** `tests/spec/p7_3/cells_test.cpp` and `knobs_test.cpp` (plugin preset). They snapshot the window
  at 2x. Change the code, never the tests.

## What to build

1. **The step cells** (`plugin/ui/Steps.cpp`), as design section 4.1 draws them:
   - **The lit cell:** a soft outer glow, `rose_bright` at 16% of the light, a 5 px stroke 3 px outside the cell
     (radius 6), never a fill; an ember fill on the face, `rose_ember` at 55% of the light at the top to 12% at the
     bottom; a 1.2 px border blending from `rule` to `rose_bright` with the light (1 px `rule` at rest). Nothing else
     inside the cell: no second glow, no frame.
   - **The trace:** each column's min and max are taken with 0 (start both at 0, not at 1 and -1), so every column's
     band reaches from the middle line to the wave. Draw the band filled, `rose` at 20% of the trace's alpha
     (`rose_bright` at 30% while the cell is lit past half), and its upper and lower edges as 1 px strokes along the
     columns' maxima and minima, at the trace's alpha and 70% of it. The sweep head and the fade with age stay as they
     are.
   - **A sounding cell that hasn't played yet** shows a 1 px `rose_deep` line (80%) across its trace's middle. A rest
     shows none.
   - **A rest's dashes:** fill the dashed path in `rule2`; never stroke it again.

2. **The knobs** (`plugin/ui/Controls.cpp`), as design section 3 draws them:
   - **The glow is for the hand:** draw it only while the knob is hovered or dragged. A lit knob's arc, pointer and
     readout turn `rose_bright` with the light, and nothing glows.
   - **No arc of no length:** a knob at its arc's start (a value of 0, or a bipolar knob at 0) draws neither the arc
     nor its glow, lit or hovered, so no round end leaves a dot. The bipolar tick at 12 o'clock stays `text_ghost`.

3. **The review images and the strict validation's log, again** (`artifacts/ui/window-idle.png`,
   `artifacts/ui/window-playing.png`, `artifacts/ui/window-clipping.png`, `artifacts/reports/pluginval-strict.txt`),
   once tasks 1 and 2 are in:
   - Run `tools/spec.sh 6 budget` and commit the three 2x review images it writes to `artifacts/ui/`: they now show
     the step row as designed.
   - Run `tools/pluginval.sh 10 artifacts/reports/pluginval-strict.txt` and commit the log it keeps. Never write or
     edit either by hand. The log's one line that can change is the plugin's identifier, whose first hash comes from
     the bundle's path.

## API

Unchanged. `Steps.h` and `Controls.h` stay as they are.

## Acceptance checks

Each check is one command, run from the repository root; the checks that share a command are one verify step. All
build the `plugin` preset.

| Check | Command | Proves |
|---|---|---|
| P7.3.01 | `tools/spec.sh 7_3 cells` | The lit cell's ember fill at the design's strength (its upper strip at most luma 0.13), fading downwards (low in the cell the light adds at most 0.6 times what it adds at the top) |
| P7.3.02 | `tools/spec.sh 7_3 cells` | A trace is a band standing on the middle line in at least 90% of its columns, filled from it to the wave, its far edge brighter than its middle in at least 80% of its tall columns |
| P7.3.03 | `tools/spec.sh 7_3 cells` | A rest's dashes are 1 px (at most 700 of the 2016 pixels of the 2 px ring inside its edge, at 2x); a sounding cell that hasn't played shows a 1 px line across its middle; a rest shows none |
| P7.3.04 | `tools/spec.sh 7_3 knobs` | A lit knob's arc turns `rose_bright` with its column but grows no glow (2.5 px either side of the arc, within luma 0.02 of the knob unlit); a hovered knob glows |
| P7.3.05 | `tools/spec.sh 7_3 knobs` | A knob at its start draws no arc: a rest's velocity shows the bare track at 7:30, lit, hovered or neither |
| P7.3.06 | `tools/spec.sh 7_3 knobs` | A bipolar knob at its centre, lit or hovered: the 12 o'clock tick stays `text_ghost`, and nothing lights there |
| P7.3.07 | `tools/spec.sh 7_2 cells` | The cells as phase 7.2 checked them (P7.2.05). A regression check: it passes on the base too |
| P7.3.08 | `tools/spec.sh 6 steps` | The cells' light, traces, dimming, drift marks, clicks and Notes stamp (P6.20 to P6.25). A regression check: it passes on the base too |
| P7.3.09 | `tools/spec.sh 7_2 knobs` | The knobs and switches as phase 7.2 checked them (P7.2.01 to P7.2.04). A regression check: it passes on the base too |
| P7.3.10 | `tools/spec.sh 6 controls` | The controls' places, kinds, attachments, readouts, gestures, legends and values (P6.13 to P6.19). A regression check: it passes on the base too |
| P7.3.11 | `tools/spec.sh 6 budget` | The window as one, its colour rules, the frame budget (P6.37: a median of 9 ms and a 95th percentile of 16 ms at 1x, a median of 20 ms at 2x) and the review images (P6.35 to P6.38). A regression check: it passes on the base too |
| P7.3.12 | `tools/spec.sh 7 ship` | The strict validation's log, made again: strictness 10, every test passed on Metakosmia Undertow 2 2.0.0, no folder outside the checkout. A regression check: it passes on the base too |
| P7.3.13 | `tools/pluginval.sh 10` | pluginval passes at strictness 10 (without its GUI tests). A regression check: it passes on the base too |
| P7.3.14 | `tools/check.sh --plugin` | The whole local check still passes: phases 1 to 7.3, both presets, pluginval |

## Rules every task keeps

- The rules of phases 1 to 7.2 still hold: never edit `tests/spec/`, `third_party/`, `docs/` or the given tools; every
  word goes through `texts::draw`; every angle clockwise from 12 o'clock; no token outside design section 6, and no
  white.
- Phase 6's and 7.2's window checks keep passing: the window's places, words and readouts don't move.

## Also required, not checked automatically

- The playing step row reads as the design's renders do: a dark lit cell fading downwards under a thin bright border,
  phosphor bands on the middle line, knobs that light without glowing.

## Notes for the planner

- This spec is `docs/specs/phase-7.3-step-row-as-designed.md`. Put it first among the files every task reads, and
  `docs/design/window.md` second.
- Three tasks. Tasks 1 and 2 can run in parallel: they share no file. Task 3 needs both: its review images show both
  tasks' work. Each task's verify steps are exactly its checks' commands, unchanged:
  1. **The step cells.** Focused: about 45 lines in one file, most of them replacing the lit cell's fills and the
     trace's per-column gradient with shorter code.
     - Edits: `plugin/ui/Steps.cpp`.
     - Its files are that 1 path; it creates and deletes nothing.
     - Checks: P7.3.01 to P7.3.03 (one step), then the regression checks P7.3.07 and P7.3.08
       (`expect = "pass_on_base"`).
  2. **The knobs.** Mechanical: about 10 lines in `Knob::paint`.
     - Edits: `plugin/ui/Controls.cpp`.
     - Its files are that 1 path; it creates and deletes nothing.
     - Checks: P7.3.04 to P7.3.06 (one step), then the regression checks P7.3.09 and P7.3.10
       (`expect = "pass_on_base"`).
  3. **The review images and the log, again.** Needs tasks 1 and 2. Focused: no code; it runs two scripts and commits
     the four files they write.
     - Edits: `artifacts/ui/window-idle.png`, `artifacts/ui/window-playing.png`, `artifacts/ui/window-clipping.png`,
       `artifacts/reports/pluginval-strict.txt`.
     - Its files are those 4 paths; it creates and deletes nothing.
     - Checks: the regression checks P7.3.11, P7.3.12 and P7.3.13 (`expect = "pass_on_base"`). It commits the three
       images P7.3.11 writes and the log `tools/pluginval.sh 10 artifacts/reports/pluginval-strict.txt` keeps.
- `tools/check.sh --plugin` (P7.3.14) is the plan's final check, not a task's.
- Every task runs commands: each agent needs `bash` and `edit_file`. Leave `allowed_tools` out, or include `bash`,
  `edit_file`, `read_file`, `write_file`, `grep` and `glob`.
- Every task builds the plugin preset; a fresh worktree's first build compiles JUCE (about a minute at 4 jobs).
