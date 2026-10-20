# Phase 4: the ROSEDUST phosphor editor

**Goal**: the editor of `docs/design/ui-spec.md`. A fixed 1200 x 760 instrument in the ROSEDUST language: 57 knobs in
dense margins, and in the sparse centre the sound made visible (a tunnel that follows the energy, a phosphor scope
inside an AT field, the playing pitch as the one bone number), a rumble pump field, a unit-array step display you
can edit, and NERV hazard stripes when the output clips. Plus the plumbing it needs: a scope tap in the engine, the
clock in the meters, and the pattern saved with the plugin's state.

**Depends on**: phases 2 and 3 (the meters, the rumble and the sequencer's pattern).

**Size**: L, 4 tasks, about 1200 lines.

---

## Scope

In scope: `Engine::copyScope` and two clock fields in `Meters`; `UndertowProcessor::setPattern`/`pattern` and the
pattern in the plugin state; `plugin/ui/UiFrame.h` and the editor (`plugin/PluginEditor.h/.cpp`, with its widgets in
`plugin/ui/`); the review images under `artifacts/ui/`.

Out of scope: presets and the program list (phase 5); resizing (the editor has one size); OpenGL (JUCE's software
renderer is fast enough at this size).

## Context

- The design is `docs/design/ui-spec.md`: zones and their rectangles, tokens (exact RGB), type, every widget's
  drivers and motion, the frame budget, and which properties are checked here and which a person judges.
- The spec tests: `tests/spec/p4/scope_test.cpp` (core) and `state_test.cpp`, `editor_test.cpp`, `display_test.cpp`,
  `steps_test.cpp`, `budget_test.cpp` (plugin preset), with helpers in `tests/spec/support/spec_ui.h` (fixed
  `UiFrame`s and pixel statistics). They drive a frozen editor with fixed frames and measure offscreen snapshots.
  Change the code, never the tests.
- Phases 0 to 3 must keep passing (`tools/check.sh --plugin`).

## What to build

1. **The engine's outputs for the editor** (`core/include/undertow/engine.h`, `core/src/engine.cpp`)
   - `copyScope(destination, count)`: the latest `count` mono output samples (`(left + right) / 2`), oldest first, at
     most `kScopeSamples` (4096); returns how many it copied (fewer only before that much audio). A ring buffer the
     audio thread writes and an atomic count, so the UI thread reads without locks; no allocation.
   - `Meters::hostClock` (the host's transport drives the clock, phase 3's rule) and `Meters::bpm` (the tempo in use).

2. **The pattern in the plugin** (`plugin/PluginProcessor.h/.cpp`)
   - `setPattern(pattern)` hands the pattern to the engine (`Engine::setPattern`); `pattern()` returns
     `engine().pattern()`.
   - `getStateInformation` writes the 16 steps (semitones, gate, accent, slide, probability) with the parameters;
     `setStateInformation` restores them through `Engine::setPattern`. Old states without steps keep the default.

3. **The editor skeleton** (`plugin/ui/UiFrame.h`, `plugin/ui/Theme.h`, `plugin/PluginEditor.h/.cpp`)
   - `UiFrame` as below. `captureFrame()` reads it from the processor: `meters()` (peak, rms, clipping, note,
     noteHz, step, rumble, ppq, hostClock, bpm), `filter_res` and `seq_length` from the engine, the latest 2048
     scope samples, and `energy` = the RMS in dB mapped from -48 .. -6 to 0 .. 1.
   - The fixed 1200 x 760 layout of the design's zones; the static layer drawn once into a cached image (void,
     panels, hairlines, corner stamps, labels, crosshair grid, scanlines, seeded noise floor).
   - One control per parameter (`juce::Slider` knobs drawn by a ROSEDUST `LookAndFeel`; `seq_on` a `juce::Button`),
     attached with `AudioProcessorValueTreeState::SliderAttachment`/`ButtonAttachment`; `controlFor(id)` finds it.
     `LookAndFeel_V4` draws toggle text, ticks and tooltips in white: the `LookAndFeel` sets each of those colours to a
     token.
   - A 60 Hz timer: each tick `advance(captureFrame(), elapsed)` and repaint. `freeze()` stops the timer, so tests
     and previews drive the editor only through `advance`. Everything drawn depends only on the frames advanced
     and the pattern, never on wall time, so a frozen editor draws the same picture every time.
   - `plugin/CMakeLists.txt` adds every `plugin/ui/*.cpp` to the plugin's sources by
     `file(GLOB undertow_ui_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.cpp")` (in `target_sources`
     of `Undertow`), so widget files join without editing it.

4. **The displays** (`plugin/ui/`, wired into the editor)
   - The tunnel, the scope with its phosphor history, the AT field, the bone number, the HEAD clock and meter, the
     rumble pump field and the hazard stripes, as `docs/design/ui-spec.md` section 4 describes them.
   - `chestBounds()` is the CHEST zone; `scopeBounds()` the scope's rectangle inside it.

5. **The unit array** (`plugin/ui/`, wired into the editor)
   - 16 cells in the GUT; `stepBounds(i)` and `stepAt(point)`; the playing step lit and fading back over 250 ms;
     rests dashed; steps past `seq_length` dim; accent, slide and probability marks.
   - `editStep(index, edit)` changes the processor's pattern (`setPattern`) at once; mouse clicks map to it (click
     gate, shift-click accent, alt-click slide, drag up or down a semitone per 8 px).

6. **The budget and the review images**
   - Keep 60 Hz with headroom (`docs/design/ui-spec.md` section 5): cache the static layer at the display's scale
     (`g.getInternalContext().getPhysicalPixelScaleFactor()`) and draw it 1:1, render the tunnel at a quarter of the
     CHEST's resolution from precomputed tables, keep per-pixel trigonometry and blurs out of full-resolution layers,
     and let each tick repaint only the dynamic regions.
   - Run `tools/spec.sh 4 budget` and commit the three images it writes: `artifacts/ui/editor-idle.png`,
     `artifacts/ui/editor-playing.png` and `artifacts/ui/editor-clipping.png` (1200 x 760, each under 2 MB). The 2x
     copies it writes to `build/previews/` stay uncommitted.

## API

```cpp
// core/include/undertow/engine.h, added
namespace undertow {
constexpr int kScopeSamples = 4096;
// in struct Meters:
bool hostClock = false;   // the host's transport drives the clock
double bpm = 120.0;       // the tempo in use
// in class Engine:
int copyScope(float* destination, int count) const;  // latest samples, oldest first; any thread; no allocation
}  // namespace undertow

// plugin/PluginProcessor.h, added to UndertowProcessor
void setPattern(const undertow::Pattern& pattern);
undertow::Pattern pattern() const;

// plugin/ui/UiFrame.h
#include <array>
struct UiFrame {               // everything the editor draws that changes over time
    float energy = 0.0f;       // 0 .. 1
    float peak = 0.0f;
    float rms = 0.0f;
    bool clipping = false;
    int note = -1;
    float noteHz = 0.0f;
    int step = -1;
    int length = 16;
    float resonance = 0.0f;
    float rumble = 0.0f;
    double ppq = 0.0;
    double bpm = 120.0;
    bool hostClock = false;
    std::array<float, 2048> scope{};   // the latest mono output, oldest first
};

// plugin/PluginEditor.h
#include "ui/UiFrame.h"
enum class StepEdit { ToggleGate, ToggleAccent, ToggleSlide, Up, Down };
class UndertowEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit UndertowEditor(UndertowProcessor&);
    UiFrame captureFrame() const;                         // read the processor, as each tick does
    void freeze();                                        // stop the timer: only advance() changes the picture
    void advance(const UiFrame& frame, double seconds);   // take this state, move every animation on by `seconds`
    juce::Rectangle<int> chestBounds() const;
    juce::Rectangle<int> scopeBounds() const;
    juce::Rectangle<int> stepBounds(int index) const;     // the cell of step 0 .. 15
    int stepAt(juce::Point<int> point) const;             // the cell under a point, or -1
    void editStep(int index, StepEdit edit);
    juce::Component* controlFor(const juce::String& parameterId) const;
};
```

## Acceptance checks

| Check | Command | Proves |
|---|---|---|
| P4.01 | `tools/spec.sh 4 scope` | copyScope returns the latest mono output samples in order across odd blocks, clamps to kScopeSamples, never allocates; meters() names the clock in use and its tempo |
| P4.02 | `tools/spec.sh 4 state` | The processor passes patterns to the engine, and the 16 steps are saved with the plugin's state and restored |
| P4.03-P4.04 | `tools/spec.sh 4 editor` | captureFrame reads the processor (note, pitch, RMS, energy, scope, clock, length); every parameter has its attached knob or button on the editor |
| P4.05-P4.09 | `tools/spec.sh 4 display` | Snapshots: half the panel void, no white, rose dominant, bone only for the pitch in the CHEST; hazard stripes only when clipping; the tunnel brighter with energy and moving; the scope trace across its width |
| P4.10 | `tools/spec.sh 4 steps` | 16 cells left to right in the GUT, hit-testing, the playing step the brightest cell, and gate, accent, slide and note edits reaching the pattern |
| P4.11-P4.12 | `tools/spec.sh 4 budget` | A full repaint keeps 60 Hz with headroom (CPU time: median 11 ms, 95th percentile 16 ms at 1x; median 27 ms at 2x), and the three review images written: artifacts/ui at 1x under 2 MB, build/previews at 2x |
| P4.13 | `tools/check.sh --plugin` | Everything still passes: every spec test of phases 0 to 4, both presets, pluginval at strictness 5 |

## Rules every task keeps

- The rules of phases 0 to 3 still hold: never edit `tests/spec/`, `third_party/`, `docs/` or the given tools; `core/`
  stays plain C++17; nothing the audio thread calls allocates, locks, does I/O or logs.
- The UI thread reads the engine only through `meters()`, `copyScope()`, `getParam()` and `pattern()`, and changes
  it only through parameters and `setPattern`.
- No white, no colour as decoration, bone only for the one number (`docs/design/ui-spec.md` section 1).

## Also required, not checked automatically

- The judged list in `docs/design/ui-spec.md` section 7: one instrument, calm at rest and alive when playing; the
  bone number first; the hazard stripe reads as an alarm; dense margins, sparse centre; legible at 100% and on Retina.
- Mouse editing of the unit array works in the Standalone app and in FL Studio; knob values show on hover.
- The editor opens and closes cleanly many times (no leaks: JUCE's leak detector stays quiet).

## Notes for the planner

- Four tasks. Each task's verify steps are exactly the checks named for it, with their commands unchanged, and each
  passes once that task (and the ones it needs) is done:
  1. **The engine's outputs for the editor** (`engine.h`, `engine.cpp`): P4.01.
  2. **The pattern in the plugin** (`PluginProcessor.h/.cpp`): P4.02.
  3. **The editor skeleton and the unit array** (`plugin/ui/UiFrame.h`, `plugin/ui/Theme.h`, `PluginEditor.h/.cpp`,
     `plugin/CMakeLists.txt`): items 3 and 5 above, P4.03-P4.04 and P4.10. Needs tasks 1 and 2. It declares every
     hook in the API, even ones task 4 fills in, so every phase 4 test file compiles from here on, and it adds the
     `plugin/ui/*.cpp` glob.
  4. **The displays, the budget and the review images** (display files in `plugin/ui/`, `PluginEditor.h/.cpp`):
     items 4 and 6 above, P4.05-P4.09, P4.11-P4.12 and P4.13. Needs task 3. It commits the three `artifacts/ui/*.png`
     files its check writes.
- At most two tasks own any file, and only one at a time: `PluginEditor.h/.cpp` belong to tasks 3 and 4, and
  `plugin/CMakeLists.txt` to task 3 alone (task 4 adds widget files that the glob picks up). The core library's
  sources are registered by phase 2's glob in `core/CMakeLists.txt`, and no task edits it.
- A test file is one executable, so a task owns a whole file's checks. The plugin checks build the `plugin` preset:
  the first build in a fresh worktree compiles JUCE (about a minute), later ones are incremental.
- Every task builds C++: its agent needs `bash` and `edit_file`, to run `cmake` and its own `tools/spec.sh` check
  before it hands off. Leave `allowed_tools` out, or include `bash`, `edit_file`, `read_file`, `write_file`, `grep`
  and `glob`.
- The display tests read colours by hue and luminance (`tests/spec/support/spec_ui.h`); draw with the ROSEDUST tokens
  of `docs/design/ui-spec.md` section 3 and they hold.
