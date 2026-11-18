# Phase 7.4: the step cells' outlines

**Goal**: in the step row, only a rest is dashed. Phase 7.3 left every step cell that isn't lit with a rest's dashed
outline. In a new instance all eight cells read as rests, and the one distinction the row exists to show is gone.
Also, the traces' edges are drawn as a 1 px tick per column, so a steep wave's outline breaks into steps. This phase
gives sounding cells their solid `rule` border back, and draws the trace's edges as joined strokes, as
`docs/design/window.md` section 4.1 draws them.

**Depends on**: phase 7.3.

**Size**: XS, 2 tasks: the outlines (about 18 lines in one file), then the review images and the log made again.

---

## Scope

In scope:
- `plugin/ui/Steps.cpp` (the step cells);
- the review images `artifacts/ui/window-idle.png`, `artifacts/ui/window-playing.png` and
  `artifacts/ui/window-clipping.png`, and `artifacts/reports/pluginval-strict.txt`, made again.

Given by this phase's seed, never edited by a task:
- `tests/spec/p7_4/cells_test.cpp`;
- `docs/design/window.md`, which now says in so many words that a sounding cell's border is solid whether or not it is
  lit or has played, with the dashes marking a rest alone, and that each trace edge is one stroke joining the columns'
  extremes (section 4.1).

Out of scope:
- everything else in the window, which the checks of phases 6 to 7.3 hold: the lit cell, the band's fill, the sweep
  head, the knobs, the console;
- `plugin/ui/Steps.h`, `plugin/ui/Theme.h` and the engine.

## Context

- **Why.** Phase 7.3's `Steps::drawCell` draws a lit cell in one branch and everything else in the other:
  `if (sounding && lightAmount > 0.0f) { ... } else { glass; drawRestBorder(...); }`. So every unlit cell, sounding or
  not, gets the rest's dashed outline. No check read a sounding cell's border, so it passed. Its `drawTrace` draws
  each edge as a 1 px horizontal tick per column (`g.drawLine(x, top + 0.5f, x + 1.0f, top + 0.5f, 1.0f)`), so where
  the wave moves more than a pixel between columns, its outline breaks.
- **The design** (`docs/design/window.md`, section 4.1) holds every number this phase checks.
- **The spec tests:** `tests/spec/p7_4/cells_test.cpp` (plugin preset). It snapshots the window at 2x. Change the code,
  never the tests.

## What to build

1. **The outlines** (`plugin/ui/Steps.cpp`):
   - **A sounding cell that isn't lit** keeps its glass face and gets a solid 1 px `rule` border
     (`drawRoundedRectangle` on the cell reduced by 0.5 px, radius 3, at the column's opacity). Only a rest
     (velocity 0) gets `drawRestBorder`'s dashes. The lit cell's border stays as it is: `rule` blending to
     `rose_bright` with the light, 1.2 px.
   - **The trace's edges:** build one `juce::Path` through the columns' maxima and one through their minima (each
     point at the column's centre, `x + 0.5`), and stroke each once at 1 px after the columns' fills: the upper edge
     at the trace's alpha, the lower at 70% of it. Keep the fill, the colours and the sweep head as they are.

2. **The review images and the log, again** (`artifacts/ui/window-idle.png`, `artifacts/ui/window-playing.png`,
   `artifacts/ui/window-clipping.png`, `artifacts/reports/pluginval-strict.txt`), after task 1:
   - Run `tools/spec.sh 6 budget` and commit the three 2x review images it writes to `artifacts/ui/`.
   - Run `tools/pluginval.sh 10 artifacts/reports/pluginval-strict.txt` and commit the log it keeps. Never write or
     edit the log by hand.

## API

Unchanged.

## Acceptance checks

Each check is one command, run from the repository root; the checks that share a command are one verify step. All
build the `plugin` preset.

| Check | Command | Proves |
|---|---|---|
| P7.4.01 | `tools/spec.sh 7_4 cells` | A sounding cell's border is solid, lit or not (at most 5% of its top edge left unlit, at 2x), and only a rest's is dashed (at least 30%): in a new instance and while the pattern plays |
| P7.4.02 | `tools/spec.sh 7_4 cells` | A trace's edges are strokes joining its columns' extremes: on a 49 Hz sine, the outline breaks between at most 5% of neighbouring pixel columns |
| P7.4.03 | `tools/spec.sh 7_3 cells` | The lit cell's fill, the band on the middle line, a rest's 1 px dashes and the unplayed cells' line (P7.3.01 to P7.3.03). A regression check: it passes on the base too |
| P7.4.04 | `tools/spec.sh 6 steps` | The cells' light, traces, dimming, drift marks, clicks and Notes stamp (P6.20 to P6.25). A regression check: it passes on the base too |
| P7.4.05 | `tools/spec.sh 6 budget` | The window as one, its colour rules, the frame budget and the review images (P6.35 to P6.38). A regression check: it passes on the base too |
| P7.4.06 | `tools/spec.sh 7 ship` | The strict validation's log, made again: strictness 10, every test passed, no folder outside the checkout. A regression check: it passes on the base too |
| P7.4.07 | `tools/pluginval.sh 10` | pluginval passes at strictness 10 (without its GUI tests). A regression check: it passes on the base too |
| P7.4.08 | `tools/check.sh --plugin` | The whole local check still passes: phases 1 to 7.4, both presets, pluginval |

## Rules every task keeps

- The rules of phases 1 to 7.3 still hold: never edit `tests/spec/`, `third_party/`, `docs/` or the given tools; every
  word goes through `texts::draw`; no token outside design section 6, and no white.
- The checks of phases 6 to 7.3 keep passing.

## Also required, not checked automatically

- In a new instance, the step row reads as the pattern: six solid cells with their flat lines and two dashed rests.

## Notes for the planner

- This spec is `docs/specs/phase-7.4-step-outlines.md`. Put it first among the files every task reads, and
  `docs/design/window.md` second.
- Two tasks, in order. Each task's verify steps are exactly its checks' commands, unchanged:
  1. **The outlines.** Mechanical: about 18 lines in `Steps::drawCell` and `Steps::drawTrace`.
     - Edits: `plugin/ui/Steps.cpp`.
     - Its files are that 1 path; it creates and deletes nothing.
     - Checks: P7.4.01 and P7.4.02 (one step), then the regression checks P7.4.03 and P7.4.04
       (`expect = "pass_on_base"`).
  2. **The review images and the log, again.** Needs task 1. Focused: no code; it runs two scripts and commits the
     four files they write.
     - Edits: `artifacts/ui/window-idle.png`, `artifacts/ui/window-playing.png`, `artifacts/ui/window-clipping.png`,
       `artifacts/reports/pluginval-strict.txt`.
     - Its files are those 4 paths; it creates and deletes nothing.
     - Checks: the regression checks P7.4.05, P7.4.06 and P7.4.07 (`expect = "pass_on_base"`). It commits the three
       images P7.4.05 writes and the log `tools/pluginval.sh 10 artifacts/reports/pluginval-strict.txt` keeps.
- `tools/check.sh --plugin` (P7.4.08) is the plan's final check, not a task's.
- Every task runs commands: each agent needs `bash` and `edit_file`. Leave `allowed_tools` out, or include `bash`,
  `edit_file`, `read_file`, `write_file`, `grep` and `glob`.
- Every task builds the plugin preset; a fresh worktree's first build compiles JUCE (about a minute at 4 jobs).
