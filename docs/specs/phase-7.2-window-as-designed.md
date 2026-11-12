# Phase 7.2: the window as designed

**Goal**: Undertow 2's window is drawn as `docs/design/window.md` draws it. Phase 6's window passes its checks, but put
beside the design it shows what a reader notices at once:
- every knob's pointer a quarter turn off its value arc;
- a dimmed knob drawn as a black square (TEMPO under any host);
- the spectrum's words off their baselines, one line running over another;
- the chosen segment of a switch unreadable, and the wave switches showing shapes, not waves;
- full grid lines across the spectrum;
- detent ticks on knobs that have none;
- rests outshining the cells that play;
- a frame on the edge of its budget.

Last, pluginval's strict log is made again from the final build.

**Depends on**: phase 7.1.

**Size**: M, 5 tasks: the knobs and switches, the console and the step cells, in parallel (about 170, 105 and 90
lines); then the ground drawn once, the budget and the review images; then the log.

---

## Scope

In scope:
- the window's drawing:
  - `plugin/ui/Controls.h` and `plugin/ui/Controls.cpp` (the knobs, the switches and the readouts);
  - `plugin/ui/Console.h` and `plugin/ui/Console.cpp` (the head, the spectrum and the number);
  - `plugin/ui/Steps.cpp` (the step cells);
  - `plugin/PluginEditor.h` and `plugin/PluginEditor.cpp` (the ground);
- the review images `artifacts/ui/window-idle.png`, `artifacts/ui/window-playing.png` and
  `artifacts/ui/window-clipping.png`, made again;
- `artifacts/reports/pluginval-strict.txt`, made again.

Given by this phase's seed, never edited by a task:
- `tests/spec/p7_2/knobs_test.cpp`, `console_test.cpp` and `cells_test.cpp`;
- P6.37 tightened, in `tests/spec/p6/budget_test.cpp`: a median of 9 ms at 1x (it was 11) and 20 ms at 2x (it was 27);
- `docs/design/window.md`, which now gives how a knob and a switch are drawn (section 3), the cells' faces and traces
  (4.1), the spectrum's crosshair, axis, stamp and share in full (4.2), the head's and the meter's places (4.5, 4.7)
  and the budget (9).

Out of scope:
- the window's layout, places, words and readouts, which phase 6's checks already hold: this phase changes how things
  are drawn, never where or what;
- the cap's radial gradient (section 3 draws it; the cap may stay as phase 6 drew it), the meter's peak at rest and the
  clock's badge, which stay as phase 6 drew them;
- `plugin/ui/Theme.h`, `plugin/ui/Steps.h`, `plugin/ui/Frame.h` and `plugin/ui/Texts.h`, kept as they are;
- the engine.

## Context

- **Why.** Phase 6's checks read places, words, colours in the large and the budget, and the window passes them all.
  Rendered beside the design, it differs where those checks did not look:
  - **The angles.** The arcs use `Path::addCentredArc`, whose 0 is 12 o'clock, but the pointers and the detent ticks
    use `(cos a, sin a)`, whose 0 is 3 o'clock, so every pointer sits a quarter turn clockwise of its arc. The bipolar
    arcs start at `1.5π`, which in the arcs' count is 9 o'clock. The arcs use a linear proportion where the slider is
    skewed.
  - **The dimmed knob.** It is `g.fillAll(black)`: a black square.
  - **The words of the head and the console.** Each is centred in a box that ends at its baseline, so each stands above
    it, by up to 14 px for the sub's number. The spectrum's range is drawn on a line of its own, over the share.
  - **The switches.** The chosen segment is filled `rose_bright` under a `rose_bright` word. The wave glyphs are a
    triangle and a square.
  - **The spectrum.** The grid is full lines of `rose_ember` at 85%.
  - **The detents.** Every stepped knob gets 8 ticks, whatever its positions.
  - **The rests.** Their dashed path is stroked a second time, in `rose_dim`. The lit cell has no fill or glow, and a
    sweep head stays behind in any cell its step left early.
  - **The cost.** Nothing static is cached, not even the ground, so a frame sits at P6.37's old limit of 11 ms.
- **The design** (`docs/design/window.md`) holds every number this phase checks. Read sections 3, 4.1, 4.2 and 9 before
  drawing.
- **The spec tests:** `tests/spec/p7_2/knobs_test.cpp`, `console_test.cpp` and `cells_test.cpp` (plugin preset), and
  P6.37 in `tests/spec/p6/budget_test.cpp`. They snapshot the window at 2x where they read pixels, and read the words
  `texts::draw` records. Change the code, never the tests.

## What to build

1. **The knobs and switches** (`plugin/ui/Controls.h`, `plugin/ui/Controls.cpp`), as design section 3 draws them:
   - **One angle convention.** Every angle is clockwise from 12 o'clock: a point at angle `a` and radius `r` is
     `c + r (sin a, -cos a)`, for the arcs, the pointer, the dots and the tick alike. The value's angle is
     `1.25π + 1.5π p`, with `p = valueToProportionOfLength(getValue())`. A bipolar knob's arc grows from 12 o'clock
     (`2π`).
   - **The anatomy** of section 3: track, value arc, hover glow, detent dots, bipolar tick and pointer, at the radii,
     widths and colours given there.
   - **Detent dots** only on a knob of 2 to 8 positions, one per position (RATE 8, LENGTH 8, the rumble's RATE 4); none
     on the step pitches or ROOT.
   - **Dimmed** (TEMPO under a host, a column past the length or in Notes mode): the whole knob through a 32%
     transparency layer. Its column's readouts stay, at 30%, and TEMPO's `HOST` at 45%.
   - **Readouts** in `value`, turning `rose_bright` while hovered or dragged and, on a step's two knobs, with their
     column's light.
   - **The switches** as section 3 draws them: the chosen segment lit with `rose_ember` under a `rose_bright` word, the
     others unfilled with `text_dim` words, and the wave glyphs. A word a switch draws is recorded in the window's
     coordinates, and is drawn once.
   - **Drawn once per display scale:** each knob's track, dots or tick go in one image and its cap and shadow in
     another (`g.getInternalContext().getPhysicalPixelScaleFactor()`). Each image is copied 1:1 under and over the
     live arc and pointer.
   - `MODE` stands on its baseline, 127.5.

2. **The console** (`plugin/ui/Console.h`, `plugin/ui/Console.cpp`), as design section 4.2 places it:
   - **Every word on its baseline.** `texts::draw` centres a line in its box, so the console's box is the line's own
     height: from the baseline less the font's ascent to the baseline plus its descent (padding added equally above
     and below). `theme::textBox` stays as it is: the step cells use it with boxes of their own height.
   - **The stamp and the share.** `SPECTRUM` (tracking 1.6 px), then `20 Hz - 2 kHz` in `text_dim` 8 px after it, both
     on 360. The share's digits, the `%` 2 px after them and `BELOW 150 Hz` 6 px after the `%`, all on 390.
   - **The glass, quiet:** a 5 px `+` in `text_phantom` on every 32 px point from (800, 354); dotted level lines in
     `text_phantom` with their labels in `text_dim`; the axis, its ticks and labels (baseline 585).
   - **Drawn once per display scale:** the spectrum's glass, scanlines, crosshair, band, level lines, axis and stamp,
     in one image, dropped by `dropCache()` so `drawnTexts()` still records the stamp and the labels.

3. **The step cells** (`plugin/ui/Steps.cpp`), as design section 4.1 draws them:
   - a cell's glass face, scanlines, `rule` border and highlight;
   - lit: the border blending to `rose_bright` (1.2 px), the ember fill fading downwards and the soft outer glow;
   - the trace as a band with 1 px edges, its sweep head in the playing cell only;
   - a rest's dashes filled in `rule2`, never stroked again;
   - a dimmed column's cell drawn through a 30% (Notes mode: 40%) transparency layer.

4. **The ground, drawn once, the budget and the review images** (`plugin/PluginEditor.h`, `plugin/PluginEditor.cpp`,
   `artifacts/ui/window-idle.png`, `artifacts/ui/window-playing.png`, `artifacts/ui/window-clipping.png`)
   - The ground (`bg_void`, its vignette and its noise) is drawn once per display scale into an opaque image and copied
     1:1, as phase 6 asked.
   - With tasks 1 to 3 in, a frame keeps the budget of design section 9 (P6.37: a median of 9 ms and a 95th percentile
     of 16 ms at 1x, a median of 20 ms at 2x). If it doesn't, find the cost in the window's four `.cpp` files.
   - Run `tools/spec.sh 6 budget` and commit the three 2x review images it writes to `artifacts/ui/`: they now show
     the window as designed.

5. **The strict validation's log, again** (`artifacts/reports/pluginval-strict.txt`)
   - Once tasks 1 to 4 are in, run `tools/pluginval.sh 10 artifacts/reports/pluginval-strict.txt` and commit the log
     it keeps, as phase 7.1's last task did. Never write or edit the log by hand. Its one line that can change is the
     plugin's identifier, whose first hash comes from the bundle's path.

## API

The window's public API is unchanged. Its private parts (the cached images, the helpers) are free.

## Acceptance checks

Each check is one command, run from the repository root; the checks that share a command are one verify step. The
window's checks build the `plugin` preset.

| Check | Command | Proves |
|---|---|---|
| P7.2.01 | `tools/spec.sh 7_2 knobs` | A knob's pointer and its value arc both point at its value (the slider's own proportion), clockwise from 12 o'clock, on the track's radius; a bipolar arc grows from 12 o'clock |
| P7.2.02 | `tools/spec.sh 7_2 knobs` | A dimmed knob is the knob at 32%, never a box (TEMPO under a host, the columns past the length), and a dimmed column keeps its readouts at 30% |
| P7.2.03 | `tools/spec.sh 7_2 knobs` | Switches: the chosen segment lit in `rose_ember` under a readable `rose_bright` word, the others unfilled with `text_dim` words; the waves drawn as 13 x 7 glyphs |
| P7.2.04 | `tools/spec.sh 7_2 knobs` | Detent dots: one per position on RATE, LENGTH and the rumble's RATE, the current one lit; none on a knob of more positions |
| P7.2.05 | `tools/spec.sh 7_2 knobs` | The step readouts are `value` at rest and turn `rose_bright` with their column's light |
| P7.2.06 | `tools/spec.sh 7_2 console` | The head's and the console's words stand on the design's baselines within 1.5 px, where the design puts them, and no two overlap |
| P7.2.07 | `tools/spec.sh 7_2 console` | The spectrum's glass is quiet: a `+` on every 32 px point from 800, 354 and dotted level lines, both `text_phantom`, and no line across it |
| P7.2.08 | `tools/spec.sh 7_2 cells` | The cells: a rest's 1 px `rule2` dashes, the lit cell's ember fill and soft glow, and a sweep head only in the playing cell |
| P7.2.09 | `tools/spec.sh 6 budget` | The window as one, its colour rules and the review images (P6.35, P6.36, P6.38), and P6.37 tightened: a full repaint at a median of 9 ms and a 95th percentile of 16 ms at 1x, a median of 20 ms at 2x |
| P7.2.10 | `tools/spec.sh 6 controls` | The controls' places, kinds, attachments, readouts, gestures, legends and values (P6.13 to P6.19). A regression check: it passes on the base too |
| P7.2.11 | `tools/spec.sh 6 console` | The console's displays (P6.26 to P6.34). A regression check: it passes on the base too |
| P7.2.12 | `tools/spec.sh 6 steps` | The cells' light, traces, dimming, drift marks, clicks and Notes stamp (P6.20 to P6.25). A regression check: it passes on the base too |
| P7.2.13 | `tools/spec.sh 6 frame` | The frame, its tokens, fonts and hooks (P6.08 to P6.12). A regression check: it passes on the base too |
| P7.2.14 | `tools/spec.sh 7 ship` | The strict validation's log, made again: strictness 10, every test passed on Metakosmia Undertow 2 2.0.0, no folder outside the checkout. A regression check: it passes on the base too |
| P7.2.15 | `tools/pluginval.sh 10` | pluginval passes at strictness 10 (without its GUI tests). A regression check: it passes on the base too |
| P7.2.16 | `tools/check.sh --plugin` | The whole local check still passes: phases 1 to 7.2, both presets, pluginval |

## Rules every task keeps

- The rules of phases 1 to 7.1 still hold: never edit `tests/spec/`, `third_party/`, `docs/` or the given tools; the
  UI thread reads the engine only through `meters()` and `copyScope()`; every word goes through `texts::draw`.
- **The window:**
  - every angle clockwise from 12 o'clock;
  - every word through `texts::draw`, in a box that puts its glyphs on the design's baseline;
  - static layers drawn once per display scale and copied 1:1;
  - a dimmed thing drawn through a transparency layer, never replaced;
  - no token outside design section 6, and no white.
- Phase 6's window checks keep passing: the window's places, words and readouts don't move.

## Also required, not checked automatically

- The window reads as the design's renders and the approved mockup do: knobs that point where they read, words on
  their lines, quiet glass, rests quieter than steps.

## Notes for the planner

- This spec is `docs/specs/phase-7.2-window-as-designed.md`. Put it first among the files every task reads, and
  `docs/design/window.md` second: the spec holds the fixes and which task owns which file, the design every number.
- Five tasks. Tasks 1, 2 and 3 can run in parallel: no two share a file. Task 4 needs tasks 1, 2 and 3. Task 5 comes
  last. Each task's verify steps are exactly its checks' commands, unchanged:
  1. **The knobs and switches.** Architectural: about 170 lines changed, most of them `Knob::paint` and
     `Switch::paint`.
     - Edits: `plugin/ui/Controls.h`, `plugin/ui/Controls.cpp`.
     - Its files are those 2 paths; it creates and deletes nothing.
     - Checks: P7.2.01 to P7.2.05 (one step), then the regression check P7.2.10 (`expect = "pass_on_base"`).
  2. **The console.** Integrative: about 105 lines changed.
     - Edits: `plugin/ui/Console.h`, `plugin/ui/Console.cpp`.
     - Its files are those 2 paths; it creates and deletes nothing.
     - Checks: P7.2.06 and P7.2.07 (one step), then the regression check P7.2.11 (`expect = "pass_on_base"`).
  3. **The step cells.** Integrative: about 90 lines changed.
     - Edits: `plugin/ui/Steps.cpp`.
     - Its files are that 1 path; it creates and deletes nothing.
     - Checks: P7.2.08, then the regression check P7.2.12 (`expect = "pass_on_base"`).
  4. **The ground, drawn once, the budget and the review images.** Needs tasks 1, 2 and 3. Focused: about 20 lines and
     the images its check writes.
     - Edits: `plugin/PluginEditor.h`, `plugin/PluginEditor.cpp`, `artifacts/ui/window-idle.png`,
       `artifacts/ui/window-playing.png`, `artifacts/ui/window-clipping.png`.
     - Its files are those 5 paths; it creates and deletes nothing.
     - Checks: P7.2.09, then the regression check P7.2.13 (`expect = "pass_on_base"`). It commits the three images
       P7.2.09 writes.
  5. **The strict validation's log, again.** Needs task 4. Mechanical: it runs one script and commits the log it keeps.
     - Edits: `artifacts/reports/pluginval-strict.txt`.
     - Its files are that 1 path; it creates and deletes nothing.
     - Checks: the regression checks P7.2.14 and P7.2.15 (`expect = "pass_on_base"`).
- `tools/check.sh --plugin` (P7.2.16) is the plan's final check, not a task's.
- Every task runs commands: each agent needs `bash` and `edit_file`. Leave `allowed_tools` out, or include `bash`,
  `edit_file`, `read_file`, `write_file`, `grep` and `glob`.
- Tasks 1 to 4 build the plugin preset; a fresh worktree's first build compiles JUCE (about a minute at 4 jobs).
