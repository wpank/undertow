# Phase 6: the window

**Goal**: Undertow 2's window, as `docs/design/window.md` draws it. It is a fixed 1200 x 760 panel in the ROSEDUST
phosphor language, laid out with the DFAM's logic:
- the sequencer across the top: eight step columns, each a glass cell over a pitch knob over a velocity knob, with the
  pattern's settings and the drift arc beside them;
- the VCO, VCF and VCA left to right, each ending in its envelope, with the spectrum and the sub's pitch beside them;
- the rumble and the output at the bottom.

Each step's cell is a small oscilloscope of its last hit, and the playing step's column lights. The spectrum shows
where the energy sits (20 Hz to 2 kHz, the 30 to 150 Hz band marked, the share below 150 Hz), and the one bone number
is the sub's pitch.

**Depends on**: phase 5, so phases 1 to 4 too: `meters().step`, `count` and `root`, and `undertow::Pattern`.

**Size**: L, 6 tasks, about 1,700 lines and the three review images.

---

## Scope

In scope:
- The core:
  - `core/include/undertow/spectrum.h` and `core/src/spectrum.cpp` (new): the spectrum's analysis;
  - `core/include/undertow/engine.h`: the scope grows to 16384 samples.
- The plugin:
  - `plugin/CMakeLists.txt`: the typefaces as binary data;
  - `plugin/PluginEditor.h` and `plugin/PluginEditor.cpp`: the window;
  - `plugin/ui/Theme.h`, rewritten: the tokens, the typefaces and the layout;
  - new, in `plugin/ui/`: `Frame.h`, `Controls.h` and `Controls.cpp`, `Steps.h` and `Steps.cpp`, `Console.h` and
    `Console.cpp`.
- The review images: `artifacts/ui/window-idle.png`, `window-playing.png` and `window-clipping.png`.
- Undertow 1's window parts, deleted: `plugin/ui/Displays.h`, `plugin/ui/Displays.cpp`, `plugin/ui/Look.h`,
  `plugin/ui/Look.cpp`, `plugin/ui/Status.h`, `plugin/ui/Status.cpp`, `plugin/ui/UnitArray.h`,
  `plugin/ui/UnitArray.cpp` and `plugin/ui/UiFrame.h`.

Given by this phase's seed, never edited by a task:
- `docs/design/window.md`;
- `third_party/fonts/`: `Michroma-Regular.ttf`, `GeistMono-Regular.ttf`, `GeistMono-Light.ttf` and their SIL Open Font
  License texts;
- the spec tests in `tests/spec/p6/` and `tests/spec/support/spec_window.h`.

Out of scope:
- resizing (the window has one size);
- OpenGL (JUCE's software renderer is fast enough at this size);
- a preset browser (the host's program list is the browser);
- any engine change beyond the scope's length;
- `plugin/ui/Texts.h`, kept as it is.

## Context

- **The design is part of this spec.** `docs/design/window.md` gives every number the checks measure:
  - the zones, legends and rules (section 2);
  - every control's place, kind and readout (section 3);
  - each display's geometry, drawing and data (section 4);
  - the typefaces (section 5), the colours (section 6), the motion (section 7), the gestures (section 8) and the frame
    budget (section 9).
  Read it before writing a line. The checks compare the window with it to the pixel.
- **The spec tests:**
  - `tests/spec/p6/spectrum_test.cpp` (core preset);
  - `frame_test.cpp`, `controls_test.cpp`, `steps_test.cpp`, `console_test.cpp` and `budget_test.cpp` (plugin preset).

  Their helpers are in `tests/spec/support/spec_window.h`:
  - a frozen window driven by fixed frames, and a performance that plays the default pattern into frames as a host
    would;
  - the design's places;
  - pixel statistics: near-black is luma under 0.06, the rose hue band runs 300 to 15 degrees, and bone-like pixels,
    rose_bright pixels and ink are counted.

  Change the code, never the tests.
- **How the checks see the window:** a frozen window is snapshotted at 1x and 2x, its component tree is read, and
  every word it draws is recorded with its box. That recording is `texts::draw` in `plugin/ui/Texts.h`, which
  `drawnTexts()` turns on. A word drawn any other way is invisible to the checks, and they fail.
- **Earlier phases keep passing** (`tools/check.sh --plugin`): phase 1's window checks (P1.15) still hold. The window
  is 1200 x 760, fixed and dark.

## What to build

1. **The spectrum and the scope** (`core/include/undertow/spectrum.h`, `core/src/spectrum.cpp`,
   `core/include/undertow/engine.h`)
   - `kScopeSamples` becomes 16384: 341 ms at 48 kHz, enough for the spectrum to resolve the 49 Hz sub. The scope's
     ring is the engine's own storage (sized once), so `copyScope` and `process` still never allocate.
   - `undertow::Spectrum`, plain C++17 for the UI thread, allocating only in its constructor. One `analyse(samples,
     seconds)` per frame:
     - window the newest 16384 samples with a 4-term Blackman-Harris window (0.35875, 0.48829, 0.14128, 0.01168) and
       take a 16384-point FFT: an iterative radix-2 with precomputed twiddles is enough;
     - scale the magnitudes so a full-scale sine's peak bin is 1, that is, by 2 over the window's sum, and square them;
     - each display point `i` (0 to 219) sits at `pointHz(i) = 20 * 100^(i / 219)`. Its band runs a twelfth of an
       octave either side, widened to at least 1.5 bins either side, with bin `k` spanning `k - 0.5` to `k + 0.5`.
       Sum its power with fractional bins at the edges, divide by the window's equivalent noise bandwidth (N times the
       sum of the window's squares over the square of its sum: 2.0 bins), and convert to dB, floored at -96. So a sine
       reads its own level at its own frequency, and pink noise reads flat;
     - each point's smoothed `level` moves towards its frame value by `1 - e^(-seconds / tau)`, with tau 25 ms rising
       and 300 ms falling;
     - the share is the power of the bins from 20 Hz to below 150 Hz over the power of the bins from 20 Hz up. It is
       smoothed the same way with tau 500 ms, and silence (no power) counts as 0.
   - `setSampleRate` moves the bands and resets; `reset` puts every point at -96 dB and the share at 0.

2. **The frame** (`plugin/CMakeLists.txt`, `plugin/ui/Theme.h`, `plugin/ui/Frame.h`, `plugin/PluginEditor.h`,
   `plugin/PluginEditor.cpp`, and the stub headers `plugin/ui/Controls.h`, `plugin/ui/Steps.h`, `plugin/ui/Console.h`)
   - **Undertow 1's window goes:** delete `plugin/ui/Displays.h` and `.cpp`, `Look.h` and `.cpp`, `Status.h` and
     `.cpp`, `UnitArray.h` and `.cpp`, and `UiFrame.h`. They draw Undertow 1's types, and the plugin's new `ui/*.cpp`
     glob would compile them. `plugin/ui/Texts.h` stays.
   - **The typefaces:**
     - `juce_add_binary_data(UndertowFonts HEADER_NAME UndertowFonts.h NAMESPACE UndertowFonts SOURCES ...)` over the
       three files in `${PROJECT_SOURCE_DIR}/third_party/fonts/`, linked PRIVATE to `Undertow`;
     - `Theme.h` makes each typeface once (`juce::Typeface::createSystemTypefaceFor` on its binary data) and gives
       `legendFont`, `numberFont` and `numberLightFont`;
     - a font's size is its em square in logical pixels (`juce::FontOptions(typeface).withPointHeight(px)`). Its
       tracking in pixels is `font.withExtraKerningFactor(tracking / font.getHeight())`.
   - **`Theme.h` holds the design as data** (rewritten from phase 1's):
     - rewritten in full (phase 1's lost some of its tokens): every token of section 6, as a `juce::Colour` with the
       table's value, named as the API below names it;
     - `editorWidth` and `editorHeight`;
     - the zones and the displays' rectangles of sections 2 and 4;
     - the headers, row labels, strip heads and dividers;
     - one place per parameter: a knob's centre and dial, a switch's rectangle, its legend, and whether its value
       always shows;
     - a helper that turns a baseline and an alignment into the box `texts::draw` takes.
   - **`WindowFrame`** (`Frame.h`) is everything the window draws that changes over time. `captureFrame()` fills it
     from the processor:
     - the meters: peak, rms, clipping, note, noteHz, step, count, root, rumble, ppq, bpm and hostClock;
     - the sample rate (48000 before `prepareToPlay`);
     - the newest 16384 output samples, oldest first. Copy them with `copyScope` and zero-fill the front if fewer
       exist.
   - **The editor:**
     - fixed 1200 x 760 and not resizable, painting a near-black ground. The ground is `bg_void` with a faint vignette
       (`#0A080C` at (504, 304), `bg_void` at 65% of a 900 px radius, `#040405` at its edge) and a sparse seeded noise
       of `rose_deep` at 35%. Draw it once per display scale (`getPhysicalPixelScaleFactor`) and copy it 1:1;
     - it owns a `Controls`, a `Steps` child over `theme::stepsBounds` (100, 88, 664, 72) and a `Console`;
     - `paint` draws the ground, then `controls.paint`, then `console.paint`. `paintOverChildren` draws
       `console.paintOver`.
   - **The editor's ticks:**
     - a 60 Hz timer: each tick is `advance(captureFrame(), elapsed)`;
     - `advance` hands the frame to `steps.advance` and `console.advance`;
     - each step's column then gets `controls.setLight(s, steps.light(s))`, and is dimmed with
       `controls.setColumnDim(s, notesMode || s >= seq_length)`. In Notes mode the light is 0;
     - `controls.setHostClock(frame.hostClock)` follows, then a repaint;
     - `freeze()` stops the timer, so a check drives the window only through `advance`. Nothing drawn depends on wall
       time.
   - **The editor's hooks:**
     - `controlFor(id)` and `hover(id)` pass to `controls`;
     - `stepCellBounds(s)`, `stepAt(point)` and `plotBounds()` come from `Theme.h`'s geometry, so they hold with the
       stubs;
     - `drawnTexts()` paints the whole window once into an offscreen image, every widget's cache dropped
       (`dropCache()`), with `texts::record()` pointing at the result.
   - **A click on a cell** (`steps.onClick`) toggles a rest, as one gesture on `step<n>_vel`
     (`beginChangeGesture`, `setValueNotifyingHost`, `endChangeGesture`). A velocity above 0 is remembered and set to
     0. At 0, the step gets back the velocity remembered for it, or 0.80 if it never had one.
   - **The stub headers** declare exactly the API below with inline empty bodies (the light 0, the bounds empty, -1
     from `stepAt`, `nullptr` from `controlFor`). So the window builds and runs with no controls and no displays until
     tasks 3 to 5 replace them.

3. **The controls** (`plugin/ui/Controls.h`, `plugin/ui/Controls.cpp`)
   - **One control per parameter** at its place (design section 3), a child of the window:
     - `vco1_wave`, `vco2_wave`, `sub_octave`, `vca_attack` and `seq_mode` are `Switch`es;
     - every other parameter is a `Knob`, a `juce::Slider`.
     Each is named by its id (`setName`) and titled by its name (`setTitle`).
   - **A knob:**
     - `RotaryHorizontalVerticalDrag`, no text box, from 1.25 pi to 2.75 pi with stop at end, 250 px of drag;
     - attached with `AudioProcessorValueTreeState::SliderAttachment`, so its range, skew and whole steps are the
       parameter's;
     - after attaching: its `textFromValueFunction` reads as the design's readout table, and a double-click returns it
       to the parameter's default;
     - its component is its dial plus 12 px, square and centred on the place;
     - it draws itself as section 3 and the mockup do: track, value arc (from 12 o'clock for a bipolar one), detents,
       cap and pointer;
     - hovered, its arc glows and turns `rose_bright`; lit (`setLight`), its arc and pointer turn `rose_bright` with the
       light; dimmed, it is drawn at 32% opacity.
   - **A switch:** a `juce::Component` over a `juce::ParameterAttachment`:
     - one segment per choice, the chosen one lit;
     - a click (`mouseUp`) chooses the segment under it as one complete gesture;
     - the wave switches draw path glyphs, `sub_octave` reads `0` and `-1`, and the others read their choices in
       capitals.
   - **`Controls::paint` draws the printed faceplate** of section 2:
     - section legends with their rules (RUMBLE's rule starts at x 114, after its lamp), row labels, strip heads and
       dotted dividers;
     - each control's legend: 13.5 px under a knob's dial, or at a switch's legend anchor;
     - the values that show: the top tier's always, the others while hovered or dragged. TEMPO reads `HOST` under a
       host's clock. A lit column's values turn `rose_bright` with the light.

     Every word goes through `texts::draw` with the theme's fonts.

4. **The step cells** (`plugin/ui/Steps.h`, `plugin/ui/Steps.cpp`)
   - The cells of section 4.1, drawn in the window's coordinates through the component's offset:
     - glass, number, trace, lit state, rests, steps past the length, the Notes-mode stamp, and the drift marks.
   - **`advance`:**
     - reads the parameters through `value` and keeps an `undertow::Pattern` up to date;
     - when `frame.step` changes, the new step's cell clears and lights (light 1). The other cells age by one and fade
       their light over 250 ms;
     - the playing cell's trace fills `66 * elapsed / stepLength` of its 66 columns. It takes the newest
       `round(seconds * sampleRate)` samples of the frame's scope, as min/max per column;
     - the drift marks compare `pattern.at(passStart + s, frame.root)` with `pattern.home(s)` for each sounding step
       on the current pass. `passStart` is `floor(count / length) * length`.
   - **A click** on a cell calls `onClick(step)`. `light`, `cellBounds` and `stepAt` answer for the window.

5. **The console** (`plugin/ui/Console.h`, `plugin/ui/Console.cpp`)
   - The displays of sections 4.2 to 4.8: the head (the mark, the clock on glass, LIVE), the spectrum (it owns an
     `undertow::Spectrum`), the sub's number, the drift arc, the meter and the clip lamp, the rumble's lamp and
     breathing rule, and (in `paintOver`) the hazard stripes and the clip flash.
   - **`advance`:**
     - runs `spectrum.analyse` on every other tick (at least 1/30 s apart), keeping the newest curve and the four
       before it;
     - holds the meter's peak, moves the number, counts silence for the hint, and watches the beat clock for the
       ghost kicks;
     - everything follows the frame and the parameters, through `value`.

6. **The window as one, its budget and its review images** (`artifacts/ui/window-idle.png`, `window-playing.png`,
   `window-clipping.png`)
   - The parts meet: task 2's ticks pass each column's light from the cells to its knobs, and dim the columns past the
     length and in Notes mode. If they do not (P6.35), the wiring is in `plugin/PluginEditor.cpp`.
   - Keep 60 Hz with headroom (design section 9):
     - static layers drawn once per display scale;
     - glows as wide low-alpha strokes, never blurs;
     - the analysis on every other tick.

     If the budget fails, find the cost in `plugin/PluginEditor.cpp` and the three widgets' `.cpp` files.
   - Run `tools/spec.sh 6 budget`. Commit the three 2x images it writes to `artifacts/ui/`: 2400 x 1520, each under
     3 MB. The 1x copies in `build/previews/` stay uncommitted.

## API

The headers the tasks write. Names, types and signatures are what the spec tests compile against; the private parts
are free.

```cpp
// core/include/undertow/spectrum.h
#pragma once

#include <memory>

namespace undertow {

constexpr int kSpectrumSize = 16384;  // samples a frame analyses: the scope's length, kScopeSamples
constexpr int kSpectrumPoints = 220;  // the display's points, log-spaced from 20 Hz to 2 kHz

class Spectrum {
public:
    Spectrum();
    ~Spectrum();
    Spectrum(const Spectrum&) = delete;
    Spectrum& operator=(const Spectrum&) = delete;

    void setSampleRate(double sampleRate);    // then reset()
    void reset();                             // every point at the floor (-96 dB), the share at 0
    // One frame: the newest kSpectrumSize output samples, oldest first, `seconds` after the frame before.
    void analyse(const float* samples, double seconds);
    static double pointHz(int index);         // 20 * 100^(index / 219)
    float frameLevel(int index) const;        // this frame's band power in dB re a full-scale sine, at least -96
    float level(int index) const;             // frameLevel, smoothed: 25 ms rising, 300 ms falling
    float share() const;                      // the power from 20 to 150 Hz over the power above 20 Hz, smoothed 500 ms

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace undertow
```

```cpp
// core/include/undertow/engine.h, changed
constexpr int kScopeSamples = 16384;  // 341 ms at 48 kHz
```

```cpp
// plugin/ui/Frame.h
#pragma once
#include <undertow/engine.h>
#include <array>

struct WindowFrame {
    float peak = 0.0f;
    float rms = 0.0f;
    bool clipping = false;
    int note = -1;              // -1 when the voice is idle
    float noteHz = 0.0f;        // the note's pitch without the thud
    int step = -1;              // the step playing, 0 .. 7, or -1
    long long count = 0;        // that step's count (Pattern::at)
    int root = -1;              // that step's root, a MIDI note
    float rumble = 0.0f;
    double ppq = 0.0;
    double bpm = 120.0;
    bool hostClock = false;
    double sampleRate = 48000.0;
    std::array<float, undertow::kScopeSamples> scope{};  // the newest output, oldest first
};
```

```cpp
// plugin/ui/Theme.h: the parts the spec tests use (the rest is the design's data, free in form)
namespace undertow::ui::theme {
// The tokens of the design's section 6: bg_void is bgVoid, and the cap's three stops and rim are capHi, capMid, capLo
// and capRim. A token may be a const or a constexpr juce::Colour; more tokens are free.
inline const juce::Colour bgVoid{0xff060608};
inline const juce::Colour bgMid{0xff080810};
inline const juce::Colour glass{0xff070609};
inline const juce::Colour rule{0xff1e1620};
inline const juce::Colour rule2{0xff30222e};
inline const juce::Colour rose{0xffaa7088};
inline const juce::Colour roseBright{0xffcc90a8};
inline const juce::Colour roseDim{0xff7a5060};
inline const juce::Colour roseEmber{0xff482838};
inline const juce::Colour roseDeep{0xff3a2030};
inline const juce::Colour legend{0xff8e6474};
inline const juce::Colour value{0xff988090};
inline const juce::Colour track{0xff2e1b28};
inline const juce::Colour pointer{0xffa8829a};
inline const juce::Colour capHi{0xff1d151c};
inline const juce::Colour capMid{0xff120d12};
inline const juce::Colour capLo{0xff0a070a};
inline const juce::Colour capRim{0xff2e202c};
inline const juce::Colour bone{0xffc8b890};
inline const juce::Colour boneDim{0xff8a7a5a};
inline const juce::Colour textDim{0xff584858};
inline const juce::Colour textGhost{0xff302830};
inline const juce::Colour textPhantom{0xff201820};
inline const juce::Colour dream{0xff585878};
juce::Font legendFont(float px, float tracking = 0.0f);       // Michroma
juce::Font numberFont(float px, float tracking = 0.2f);       // Geist Mono Regular
juce::Font numberLightFont(float px, float tracking = 0.0f);  // Geist Mono Light
inline constexpr int editorWidth = 1200;
inline constexpr int editorHeight = 760;
}  // namespace undertow::ui::theme
```

```cpp
// plugin/PluginEditor.h
class UndertowEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit UndertowEditor(UndertowProcessor&);
    ~UndertowEditor() override;
    WindowFrame captureFrame() const;                        // read the processor, as each tick does
    void freeze();                                           // stop the timer: only advance() changes the picture
    void advance(const WindowFrame& frame, double seconds);  // take this state, move every animation on by `seconds`
    juce::Component* controlFor(const juce::String& parameterId) const;
    void hover(const juce::String& parameterId);             // draw that control as hovered; "" for none
    std::vector<DrawnText> drawnTexts();                     // every word the window draws, with its box
    juce::Rectangle<int> stepCellBounds(int step) const;     // the glass cell of step 0 .. 7
    int stepAt(juce::Point<int> point) const;                // the step whose cell holds the point, or -1
    juce::Rectangle<int> plotBounds() const;                 // the spectrum's plot
    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    // private: the timer, the click on a cell, the ground
};
```

```cpp
// plugin/ui/Controls.h (task 3's: task 2 writes only the Controls stub below, and task 3 replaces it with all three
// classes and their bodies in Controls.cpp)
class Knob final : public juce::Slider {
public:
    Knob(bool bipolar, int detents);
    void setLight(float light);     // the playing step's column: 0 .. 1
    void setHovered(bool hovered);  // drawn as if the mouse were over it
    void setDimmed(bool dimmed);    // 32% opacity: TEMPO under a host's clock, a column past the length
    bool isHovered() const;
    void paint(juce::Graphics&) override;
};

class Switch final : public juce::Component {
public:
    Switch(juce::RangedAudioParameter& parameter, std::vector<juce::String> labels, bool numberLabels);
    int segments() const;
    int selected() const;                                 // the parameter's choice
    juce::Rectangle<int> segmentBounds(int index) const;  // in its own coordinates
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;       // chooses the segment under the mouse, one gesture
};

class Controls {
public:
    Controls();
    ~Controls();
    void attach(juce::AudioProcessorValueTreeState& state, juce::Component& window);  // one control per parameter
    void detach();
    juce::Component* controlFor(const juce::String& parameterId) const;
    void setLight(int step, float light);    // the step's two knobs, 0 .. 1
    void setColumnDim(int step, bool dim);   // the step's two knobs at 32%
    void setHostClock(bool host);            // TEMPO dims and reads HOST
    void setHover(const juce::String& parameterId);
    void paint(juce::Graphics&);             // the printed faceplate and the values that show
    void dropCache();
};
```

```cpp
// plugin/ui/Steps.h
class Steps final : public juce::Component {
public:
    std::function<float(int parameterIndex)> value;  // a parameter's value in real units
    std::function<void(int step)> onClick;           // a click on step `step`'s cell
    Steps();
    void advance(const WindowFrame& frame, double seconds);
    float light(int step) const;                      // 1 while it plays, to 0 over 250 ms after
    juce::Rectangle<int> cellBounds(int step) const;  // in the window's coordinates
    int stepAt(juce::Point<int> windowPoint) const;   // -1 off the cells
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void dropCache();
};
```

```cpp
// plugin/ui/Console.h
class Console {
public:
    std::function<float(int parameterIndex)> value;
    Console();
    void advance(const WindowFrame& frame, double seconds);
    void paint(juce::Graphics&);       // the head, the spectrum and the number, the arc, the meter, the rumble's lamp
    void paintOver(juce::Graphics&);   // the hazard bands and the clip flash
    void dropCache();
    juce::Rectangle<int> plotBounds() const;
};
```

Task 2 writes the three widget headers as these stubs, exactly, so the window builds and runs before tasks 3 to 5:

```cpp
// plugin/ui/Controls.h, the stub
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class Controls {
public:
    void attach(juce::AudioProcessorValueTreeState&, juce::Component&) {}
    void detach() {}
    juce::Component* controlFor(const juce::String&) const { return nullptr; }
    void setLight(int, float) {}
    void setColumnDim(int, bool) {}
    void setHostClock(bool) {}
    void setHover(const juce::String&) {}
    void paint(juce::Graphics&) {}
    void dropCache() {}
};
```

```cpp
// plugin/ui/Steps.h, the stub
#pragma once
#include "Frame.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class Steps final : public juce::Component {
public:
    std::function<float(int parameterIndex)> value;
    std::function<void(int step)> onClick;
    void advance(const WindowFrame&, double) {}
    float light(int) const { return 0.0f; }
    juce::Rectangle<int> cellBounds(int) const { return {}; }
    int stepAt(juce::Point<int>) const { return -1; }
    void dropCache() {}
};
```

```cpp
// plugin/ui/Console.h, the stub
#pragma once
#include "Frame.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class Console {
public:
    std::function<float(int parameterIndex)> value;
    void advance(const WindowFrame&, double) {}
    void paint(juce::Graphics&) {}
    void paintOver(juce::Graphics&) {}
    void dropCache() {}
    juce::Rectangle<int> plotBounds() const { return {}; }
};
```

## Acceptance checks

Each check is one command, run from the repository root. Checks that share a command are one verify step. The
spectrum's file builds with the `core` preset in seconds; the others build the `plugin` preset.

| Check | Command | Proves |
|---|---|---|
| P6.01 | `tools/spec.sh 6 spectrum` | The scope keeps the newest 16384 output samples in order through blocks of any size, without allocating |
| P6.02 | `tools/spec.sh 6 spectrum` | The 220 points are log-spaced from 20 Hz to 2 kHz; silence reads -96 dB |
| P6.03 | `tools/spec.sh 6 spectrum` | A sine reads its own level within 0.5 dB at the point nearest its frequency, never more anywhere, and below 150 Hz peaks within 3% of it, at 48 and 44.1 kHz |
| P6.04 | `tools/spec.sh 6 spectrum` | Pink noise reads flat within 3 dB from 100 Hz to 2 kHz; white noise rises 2 to 4 dB an octave |
| P6.05 | `tools/spec.sh 6 spectrum` | A point moves towards its frame's level with 25 ms rising and 300 ms falling; reset puts it on the floor |
| P6.06 | `tools/spec.sh 6 spectrum` | The share below 150 Hz: 99% or more for a 49 Hz sine, 1% or less at 200 Hz and 1 kHz, half for equal powers, smoothed over 500 ms |
| P6.07 | `tools/spec.sh 6 spectrum` | A frame's analysis allocates nothing and takes at most 3 ms of CPU |
| P6.08 | `tools/spec.sh 6 frame` | The window is a fixed 1200 x 760 UndertowEditor, and opens and closes cleanly; Undertow 1's window parts are gone |
| P6.09 | `tools/spec.sh 6 frame` | The bundled typefaces: Michroma, Geist Mono Regular and Light; sizes are em squares; tracking adds its pixels after each letter |
| P6.10 | `tools/spec.sh 6 frame` | captureFrame reads the meters (count and root included), the sample rate and the newest 16384 samples |
| P6.11 | `tools/spec.sh 6 frame` | The hooks: the step cells where the design puts them, hit-testing, the plot; a frozen window draws the same picture twice |
| P6.12 | `tools/spec.sh 6 frame` | The theme holds every token of the design's section 6 at its value; the ground: near-black margins, never white, at 1x and 2x |
| P6.13 | `tools/spec.sh 6 controls` | Every parameter has one visible control, a child of the window, named by its id and titled by its name |
| P6.14 | `tools/spec.sh 6 controls` | Each control is the design's kind at the design's place (knobs within 1 px, switches exact), inside the window, clear of the displays and of each other |
| P6.15 | `tools/spec.sh 6 controls` | Every control follows its parameter both ways; a click on a switch's segment chooses it |
| P6.16 | `tools/spec.sh 6 controls` | Every readout reads as the design writes it (43 cases) |
| P6.17 | `tools/spec.sh 6 controls` | The knobs' gestures: 270 degrees, 250 px of drag, double-click to the default, whole steps where stepped, no text box |
| P6.18 | `tools/spec.sh 6 controls` | The faceplate's words: section legends, row labels, strip heads and every control's legend drawn where the design puts them, their letters on its baselines within 1.5 px |
| P6.19 | `tools/spec.sh 6 controls` | The top tier's values always show, the others only when hovered, a hovered knob glows, and TEMPO reads HOST under a host's clock |
| P6.20 | `tools/spec.sh 6 steps` | The cells are a Steps child over the row; the playing step's cell is the brightest, lit rose_bright, and fades back over 250 ms to an unlit cell's look |
| P6.21 | `tools/spec.sh 6 steps` | The playing step's trace sweeps across its cell in one step; a rest stays empty and reads REST; every cell shows its number |
| P6.22 | `tools/spec.sh 6 steps` | Cells past the length dim to 30% |
| P6.23 | `tools/spec.sh 6 steps` | The drift's marks: each step moved on the current pass shows how (+12, -5, GHOST: Pattern::at against home), right-aligned in its cell, and only those |
| P6.24 | `tools/spec.sh 6 steps` | A click on a cell makes the step a rest, a second brings its velocity back (0.80 for a step that never had one), each one gesture |
| P6.25 | `tools/spec.sh 6 steps` | In Notes mode the cells dim under the stamp NOTES MODE: EACH MIDI NOTE IS ONE HIT |
| P6.26 | `tools/spec.sh 6 console` | The spectrum draws a sine at its own frequency and level on the plot's axes; silence draws no curve |
| P6.27 | `tools/spec.sh 6 console` | The spectrum's legends: the stamp, the band's label, the axis labels at their frequencies, the level lines |
| P6.28 | `tools/spec.sh 6 console` | The share below 150 Hz while sound plays; the idle hint in Key and Notes modes, none in Free |
| P6.29 | `tools/spec.sh 6 console` | The one number: the sub's pitch (49.00 for G2 at -1 oct, 98.00 at 0 oct) and its name in bone, moving between pitches, ghosted when idle, bone nowhere else |
| P6.30 | `tools/spec.sh 6 console` | The drift arc: 16 bars shaped by the drift and the sounding steps, the current pass alone rose_bright, PASS n/16 or 16 PASSES |
| P6.31 | `tools/spec.sh 6 console` | The head: the mark, the clock (tempo, HOST or INT, bar.beat.sixteenth) on glass, LIVE lit while the output sounds |
| P6.32 | `tools/spec.sh 6 console` | The meter: the RMS fill, the held peak's tick and readout, the scale, the clip lamp |
| P6.33 | `tools/spec.sh 6 console` | The hazard stripes show only while the output clips, with their legend |
| P6.34 | `tools/spec.sh 6 console` | The rumble: the ghost kick's lamp flashes for 120 ms on its grid; the rule brightens with the bed |
| P6.35 | `tools/spec.sh 6 budget` | The window as one: the playing column's knobs light with its cell and fade with it; past the length and in Notes mode a column's knobs dim |
| P6.36 | `tools/spec.sh 6 budget` | The whole window idle, playing and clipping, at 1x and 2x: at least 80% near-black, never white, at least 90% of the coloured pixels rose, bone only in the number's box and only once a note has played |
| P6.37 | `tools/spec.sh 6 budget` | A tick and a full repaint keep 60 Hz with headroom: CPU median 11 ms and 95th percentile 16 ms at 1x, median 27 ms at 2x (9 and 20 ms from phase 7.2) |
| P6.38 | `tools/spec.sh 6 budget` | The review images: idle, playing and clipping at 2x in artifacts/ui (under 3 MB each), 1x in build/previews |
| P6.39 | `tools/check.sh --plugin` | The whole local check still passes: phases 1 to 6, both presets, pluginval |

## Rules every task keeps

- The rules of phases 1 to 5 still hold: never edit `tests/spec/`, `third_party/`, `docs/` or the given tools; `core/`
  stays plain C++17; nothing the audio thread calls allocates, locks, does I/O or logs.
- **The UI thread's reach:**
  - it reads the engine only through `meters()` and `copyScope()`, and the parameters through the processor's
    `AudioProcessorValueTreeState`;
  - it changes the sound only through parameters;
  - every control is attached to its parameter, and no control holds a value of its own.
- **Words:**
  - every word is drawn through `texts::draw` (`plugin/ui/Texts.h`) with the theme's fonts;
  - text is ASCII plus the middle dot (U+00B7, `juce::String::charToString(0x00B7)`);
  - wave glyphs, lamps and brackets are paths, never characters.
- **Colour:**
  - `bone` and `bone_dim` are for the one number and its line alone;
  - no white;
  - every other colour is a token of the design's section 6.
- **Cost:** glows are wide low-alpha strokes, never blurs, and static layers are drawn once per display scale.

## Also required, not checked automatically

- The judged list in `docs/design/window.md` section 11. It reads as one instrument: calm when silent, alive when
  playing, the sub's number first, legible at 1x and on Retina, nothing decorative.
- In the Standalone app and in a host:
  - knobs drag, scroll and double-click;
  - hovering shows a value;
  - a click on a cell rests the step and brings it back;
  - the window opens and closes many times without leaks (JUCE's leak detector stays quiet).

## Notes for the planner

- This spec is `docs/specs/phase-6-window.md`. Put it first among the files every task reads, and
  `docs/design/window.md` second. The spec holds the rules, the API and which task owns which file. The design holds
  every number. The tests check both, to the pixel.
- Six tasks. Task 1 and task 2 can run in parallel; tasks 3, 4 and 5 run in parallel after task 2 (task 5 also needs
  task 1); task 6 comes last. No two parallel tasks share a file: `plugin/PluginEditor.h` and `.cpp` and
  `plugin/ui/Theme.h` belong to task 2, and each widget's header to task 2 (the stub) and then its own task. Each
  task's verify step is exactly its check command, unchanged:
  1. **The spectrum and the scope.** Integrative: about 170 lines.
     - Creates: `core/include/undertow/spectrum.h`, `core/src/spectrum.cpp`.
     - Edits: `core/include/undertow/engine.h`.
     - Its files are those 3 paths; it deletes nothing.
     - Checks: P6.01 to P6.07, one step (`tools/spec.sh 6 spectrum`).
  2. **The frame.** Architectural: about 450 lines, most of them the design's data.
     - Edits: `plugin/CMakeLists.txt`, `plugin/ui/Theme.h`, `plugin/PluginEditor.h`, `plugin/PluginEditor.cpp`.
     - Creates: `plugin/ui/Frame.h`, `plugin/ui/Controls.h`, `plugin/ui/Steps.h`, `plugin/ui/Console.h` (the stubs).
     - Deletes: `plugin/ui/Displays.h`, `plugin/ui/Displays.cpp`, `plugin/ui/Look.h`, `plugin/ui/Look.cpp`,
       `plugin/ui/Status.h`, `plugin/ui/Status.cpp`, `plugin/ui/UnitArray.h`, `plugin/ui/UnitArray.cpp`,
       `plugin/ui/UiFrame.h`.
     - 8 files written; its 9 deletions don't count towards the file budget. Its files are all 17 of those paths,
       the deleted ones included. `plugin/ui/Texts.h` stays as it is.
     - Checks: P6.08 to P6.12, one step (`tools/spec.sh 6 frame`).
     - Its first plugin build in a fresh worktree compiles JUCE (about a minute at 4 jobs).
  3. **The controls.** Needs task 2. Integrative: about 420 lines.
     - Edits: `plugin/ui/Controls.h`.
     - Creates: `plugin/ui/Controls.cpp`.
     - Its files are those 2 paths; it deletes nothing.
     - Checks: P6.13 to P6.19, one step (`tools/spec.sh 6 controls`).
  4. **The step cells.** Needs task 2. Integrative: about 250 lines.
     - Edits: `plugin/ui/Steps.h`.
     - Creates: `plugin/ui/Steps.cpp`.
     - Its files are those 2 paths; it deletes nothing.
     - Checks: P6.20 to P6.25, one step (`tools/spec.sh 6 steps`).
  5. **The console.** Needs tasks 1 and 2. Architectural: about 420 lines.
     - Edits: `plugin/ui/Console.h`.
     - Creates: `plugin/ui/Console.cpp`.
     - Its files are those 2 paths; it deletes nothing.
     - Checks: P6.26 to P6.34, one step (`tools/spec.sh 6 console`).
  6. **The window as one, its budget and its review images.** Needs tasks 3, 4 and 5. Integrative: the images, and
     any fix the whole window's checks ask for.
     - Creates: `artifacts/ui/window-idle.png`, `artifacts/ui/window-playing.png`, `artifacts/ui/window-clipping.png`.
     - Edits, only if its checks ask: `plugin/PluginEditor.cpp`, `plugin/ui/Controls.cpp`, `plugin/ui/Steps.cpp`,
       `plugin/ui/Console.cpp`.
     - Its files are those 7 paths; it deletes nothing.
     - Checks: P6.35 to P6.38, one step (`tools/spec.sh 6 budget`). It commits the three images its check writes.
- `tools/check.sh --plugin` (P6.39) is the plan's final check, not a task's.
- A test file is one executable. Each plugin test waits for its own task's file (`spec-needs`), so tasks 3 to 5 never
  compile one another's checks.
- Every task builds C++: its agent needs `bash` and `edit_file`, to run `cmake` and its own `tools/spec.sh` check
  before it hands off. Leave `allowed_tools` out, or include `bash`, `edit_file`, `read_file`, `write_file`, `grep`
  and `glob`.
