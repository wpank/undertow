# Undertow's editor: a phosphor instrument

The editor is a single fixed panel, 1200 x 760 logical pixels, drawn in the ROSEDUST language of
`bardo/prd/18-interfaces` (`rendering/00-design-system.md`, `04-nerv-aesthetic.md`, `01-demoscene.md`,
`screens/02-widget-catalog.md`). Its job is to make the sound visible: from across a room you see whether Undertow is
silent, rolling, pushing or clipping; up close you read every value. Phase 4 (`docs/specs/phase-4-ui.md`) builds it.

We take bardo's grammar (light follows significance, colour is a taxonomy, one bone number, hard borders around soft
interiors, dense margins around a sparse centre, time as the main dimension) and leave its mythology behind. Where
bardo is a terminal grid, this is pixels: the same rules, drawn with JUCE's `Graphics`.

## 1. The rules, applied

1. **Light follows significance.** The ground is dark. Brightness is earned: the sound itself (scope, tunnel, pump
   field) and the one number are the brightest things; labels sit near the floor of the ramp.
2. **Colour is a taxonomy, never decoration.** Rose means sound is happening. Bone is the one number. Indigo
   (`dream`) is reserved for the internal clock (no host). Warning amber is never used for normal state.
3. **One bone number.** The pitch the voice is playing (`55.00 Hz` with `A1` under it). Nothing else is bone.
4. **Bold boundaries, soft interiors.** 1 px hairline borders; inside, gradients of density and opacity.
5. **Restraint.** At least half of the panel is void or near-void in every state.
6. **The medium acknowledges itself.** Scanlines and a sparse noise floor are part of the surface (cached, static).
7. **Dual reading.** Across the room: dark and still (silent), a rose pulse (rolling), a bright tunnel (pushing),
   hazard stripes (clipping). Up close: every knob value, the clock, the step data.

## 2. Size, grid and zones

Fixed 1200 x 760, not resizable. An 8 px grid. Zones follow bardo's spatial grammar (HEAD, LIMBS, CHEST, GUT,
GROUND): dense at the edges, sparse in the centre.

| Zone | Rectangle (x, y, w, h) | Holds | Density |
|---|---|---|---|
| HEAD | 0, 0, 1200, 48 | the mark, the clock readout, the output meter, the LIVE lamp | high |
| LEFT LIMB | 16, 56, 280, 464 | OSC/01, FILTER/02, ENV/03 knob panels | high |
| CHEST | 312, 56, 576, 464 | the tunnel, the scope, the AT field, the bone number | sparse centre |
| RIGHT LIMB | 904, 56, 280, 464 | RUMBLE/04 (pump field and knobs), MOD/05 | high |
| GUT | 16, 528, 1168, 136 | SEQ/06: the 16-cell unit array | medium |
| GROUND | 0, 672, 1200, 88 | VOICE/07, OUT/08 and SEQ controls | high, quiet |
| Hazard bands | 0, 0, 1200, 16 and 0, 744, 1200, 16 | the NERV stripes, only while clipping | none at rest |

## 3. Tokens

Colours are the ROSEDUST tokens. Hex and RGB; alpha where given.

| Token | Hex | RGB | Use here |
|---|---|---|---|
| `bg_void` | `#060608` | 6, 6, 8 | the ground; never pure black |
| `bg_raised` | `#0C0A0E` | 12, 10, 14 | panel interiors |
| `bg_mid` | `#080810` | 8, 8, 16 | HEAD and GROUND strips |
| `border` | `#181420` | 24, 20, 32 | panel hairlines |
| `border_active` | `#AA7088` at 27% | 170, 112, 136 | the focused panel, the current step |
| `rose` | `#AA7088` | 170, 112, 136 | knob arcs, the scope, active data |
| `rose_bright` | `#CC90A8` | 204, 144, 168 | the brightest element: peaks, hover, clipping, the lit step |
| `rose_dim` | `#7A5060` | 122, 80, 96 | labels, corner stamps, quiet data |
| `rose_ember` | `#482838` | 72, 40, 56 | phosphor residue, scope persistence, lit cell fill |
| `rose_deep` | `#3A2030` | 58, 32, 48 | noise floor, knob tracks, the tunnel's dark end |
| `bone` | `#C8B890` | 200, 184, 144 | the one number only |
| `bone_dim` | `#8A7A5A` | 138, 122, 90 | the note name under it; the last pitch while idle |
| `text_primary` | `#988090` | 152, 128, 144 | knob values on hover, the clock |
| `text_dim` | `#584858` | 88, 72, 88 | knob labels |
| `text_ghost` | `#302830` | 48, 40, 48 | steps past `seq_length`, disabled things |
| `text_phantom` | `#201820` | 32, 24, 32 | the crosshair grid behind the scope |
| `dream` | `#585878` | 88, 88, 120 | the `INT` clock badge (internal clock) |
| `scanline_dark` | `#050507` | 5, 5, 7 | every other row, at 12% |

White (`#FFFFFF`) never appears; the brightest pixel is `rose_bright` or `bone`. Colour never carries state alone:
every state also has a glyph, a shape or a word.

**Type.** JUCE's own fonts, no bundled files: the default sans (San Francisco on macOS) for words, and
`Font::getDefaultMonospacedFontName()` (Menlo) for every number. Sizes: the bone number 56 px mono, its note name
18 px; panel stamps 10 px mono caps with 1.5 px tracking; knob labels 10 px sans caps; values 11 px mono; the
clock 14 px mono; the mark 13 px sans caps with 4 px tracking. Institutional stamps use corner brackets: `⌈ CLIP ⌋`.

## 4. Widgets

Each widget is a transducer: it names what drives it, how it moves and how it decays.

### 4.1 Knob

- 40 px dial in a 64 x 64 cell: a 270 degree track in `rose_deep`, the value arc in `rose` (from the minimum, or from
  the centre for bipolar ranges such as `filter_env`), a 2 px tick at the value. Label under it in `text_dim`.
- Hover or drag: the arc goes `rose_bright` and the value appears in `text_primary` mono under the label; double
  click resets to the default; shift-drag is fine.
- Int and Choice parameters are stepped knobs whose readout is the choice name (`1/16`, `Saw`). `seq_on` is a lamp
  button (`◉ SEQ` lit in rose, `○ SEQ` dark).
- Driven by the parameter, attached to the APVTS. Knobs never animate on their own.

### 4.2 The bone number

- The voice's pitch, `meters().noteHz`, as `55.00` in bone 56 px mono with `Hz` in `bone_dim`, and the note name (`A1`)
  in `bone_dim` 18 px under it, centred in the lower third of the CHEST.
- A new note lerps the digits from the old pitch over 120 ms (ease-out). It never flashes.
- Idle (`note == -1`): the last pitch fades to `bone_dim` over 600 ms; before any note it shows nothing.

### 4.3 The oscilloscope

- The output (mono) waveform, 480 x 200 at the CHEST's centre, from `Engine::copyScope`: triggered on a rising zero
  crossing so the trace stands still, two cycles of the playing note wide (20 ms when idle).
- A 1.5 px `rose` path with a 4 px `rose_bright` glow at 12% alpha; the five previous traces stay as phosphor,
  fading from `rose_ember` to `rose_deep`. A `+` crosshair grid in `text_phantom` every 32 px behind it (NERV B2.3).

### 4.4 The AT field

- An octagon wireframe around the scope (NERV B5), 1 px `rose_dim`. It shows the filter's resonance: calm and
  symmetric at 0; above 0.5 its vertices jitter (up to 3 px, scaled by resonance and energy); above 0.7 segments break
  into dashes and the lines brighten to `rose`. It is the voice's boundary under stress.

### 4.5 The tunnel

- The CHEST's background (demoscene 2.2): an XOR-texture tunnel from precomputed angle and distance tables, rendered
  at a quarter of the resolution (144 x 116) into an image and drawn scaled with smoothing, under the scope at most 35%
  opacity, coloured along `bg_void` to `rose_deep` to `rose_ember` to `rose`, darker with depth.
- Driven by the output energy (a smoothed RMS, -48 to -6 dBFS mapped to 0 .. 1, 30 ms up, 300 ms down): rotation
  `0.05 + 0.6 * energy` radians/s, forward speed `0.1 + 1.2 * energy`, brightness `0.25 + 0.75 * energy`. Silent, it
  barely moves in the dark; at full energy it rushes.

### 4.6 The rumble pump field

- RUMBLE/04's display, 264 x 120: the rumble's level folded by the beat (a "phosphor raster"). Each column is one
  beat (the last 32 beats, newest on the right); each of 24 rows is a slice of that beat, top to bottom. A cell's
  brightness is the rumble level there, along the rose ramp. The duck shows as dark bands at the top of every column
  and the swell as brightness below: the pumping becomes a texture.
- Driven by `meters().rumble` and `meters().ppq` each frame; a stopped transport freezes it.

### 4.7 The unit array (sequencer)

- SEQ/06: 16 cells in a row (NERV B1, the widget catalogue's UnitArray), each 64 x 104 with 8 px gaps, centred in the
  GUT. In each cell, from the top: a vertical gauge for the note (fill height `(semitones + 24) / 48`, `rose`); the
  step number `01`..`16` in `rose_dim` mono; flags: `▲` accent (`rose_bright`), `⟋` slide (joining to the next cell),
  probability as four dots (lit = quarters of 1).
- A rest has a dashed border and a dimmed cell; steps past `seq_length` are `text_ghost`.
- The playing step (`meters().step`) lights at once (border `rose_bright`, fill `rose_ember`) and fades back over
  250 ms when the sequencer moves on: a phosphor trail across the array.
- Editing: click toggles the gate, shift-click the accent, alt-click the slide, drag up or down changes the note in
  semitones; each edit goes to `UndertowProcessor::setPattern` at once.

### 4.8 The NERV hazard stripe

- While `meters().clipping` is true: 16 px bands across the top and bottom edges of diagonal stripes (45 degrees,
  12 px period) in `rose_bright` on `bg_void`, scrolling left at 40 px/s, with a `bg_void` plate in the middle of the
  top band reading `⌈ CLIP ⌋  OUTPUT -0.3 dBFS` in `rose_bright` mono. On the first clipping frame every panel border
  flashes `rose_bright` for 200 ms (NERV B4.2). The bands go when the meter clears.

### 4.9 HEAD and GROUND

- HEAD: `METAKOSMIA` in `rose_dim` and `UNDERTOW` in `rose` (wide tracking) at the left; the clock in the middle as
  `128.0 BPM · HOST · 012.3.4` (bar.beat.sixteenth) in `text_primary` mono, with `HOST` or `INT` (the internal clock,
  in `dream`) as a badge; at the right a thin output meter (peak and RMS, `rose` to `rose_bright`) and a `◉ LIVE` lamp
  lit while the output is not silent.
- GROUND: VOICE/07 (glide, glide mode, accent, bend range), OUT/08 (saturation, width, mono below, master) and the
  sequencer's controls (seq on, sync, bpm, rate, length, gate, swing, root, scale, mutate, evolve, seed) as knobs.

### 4.10 The static layer

- Drawn once into a cached image when the editor opens: the void, panel interiors and hairlines, corner stamps
  (`OSC/01` .. `OUT/08`, NERV B14), knob tracks' backgrounds, the crosshair grid, scanlines (every other row towards
  `scanline_dark` at 12%) and a seeded noise floor (0.4% of pixels in `rose_deep`).

## 5. Motion and the frame budget

- One 60 Hz timer while the editor is open; nothing runs when it is closed. Each tick reads the engine through its
  lock-free outputs (`meters()`, `copyScope`), builds a `UiFrame`, advances the animations by the elapsed time and
  repaints only the dynamic regions (CHEST, pump field, unit array, HEAD meter and clock, hazard bands).
- Budget: 60 Hz with headroom. A full repaint of the whole editor (what a frame costs at worst, as when it opens)
  takes about 5 ms at 1x on this machine; the checks allow a median of 11 ms and a 95th percentile of 16 ms (one 60 Hz
  frame) at 1x, and a median of 27 ms at 2x (Retina), counting the painting thread's own CPU time so other work on the
  machine does not count. A tick repaints only the dynamic regions, so it costs less. The expensive parts: the tunnel
  (quarter resolution, precomputed tables, one pass) and the static layer, which is drawn once per display scale at
  that scale (`getPhysicalPixelScaleFactor`) and then blitted 1:1; a 1x image drawn scaled to 2x costs more than the
  rest of the frame. Per-pixel trigonometry or a blur over a full-resolution layer every frame does not fit.
- Controls answer at once; only displays animate. Everything that animates follows audio or time, never a random
  timer, so a frozen state draws the same picture every time.

## 6. Mock

```text
 ┌──────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
 │ METAKOSMIA ⌈ U N D E R T O W ⌋          128.0 BPM · HOST · 012.3.4                       ▕▇▇▇▅▃▁ ◉ LIVE     │ HEAD
 ├────────────────────────────┬─────────────────────────────────────────────────────┬────────────────────────────┤
 │ OSC/01                     │ +       +       +       +       +       +       +     │ RUMBLE/04                  │
 │  ◜◝    ◜◝    ◜◝    ◜◝      │      ╱───────────────────────────────────╲          │ ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ │
 │  WAVE  PW   LEVEL  OCT     │    ╱  ░░▒▒▓▓ tunnel (energy) ▓▓▒▒░░        ╲        │ ░▒▓██▓▒░░▒▓██▓▒░░▒▓██▓▒░░▒ │
 │  ◜◝    ◜◝    ◜◝            │   │      ╭╮    ╭╮    ╭╮    ╭╮              │       │ ░▒▓██▓▒░░▒▓██▓▒░░▒▓██▓▒░░▒ │
 │  FINE  SUB  SUB OCT        │   │   ╭──╯╰────╯╰────╯╰────╯╰─── scope     │       │   pump field: beats ──►    │
 │ FILTER/02                  │   │                                          │       │  ◜◝   ◜◝   ◜◝   ◜◝         │
 │  ◜◝    ◜◝    ◜◝    ◜◝      │    ╲            55.00 Hz                   ╱        │  LVL  SEND KICK RATE       │
 │  CUT   RES  DRIVE  ENV     │      ╲            A1          AT field  ╱          │  ◜◝   ◜◝   ◜◝   ◜◝         │
 │  ◜◝                        │        ╲───────────────────────────────╱            │  DCY  TONE DRV  DUCK       │
 │  TRACK                     │                                                     │ MOD/05                     │
 │ ENV/03  A  D  S  R · A D S R│   +       +       +       +       +       +       + │  RATE SHAPE CUT PHASE · …  │
 ├────────────────────────────┴─────────────────────────────────────────────────────┴────────────────────────────┤
 │ SEQ/06  ┌──┐┌──┐┌──┐┌──┐┌──┐┌┄┄┐┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐                                      │
 │         │▁ ││▁ ││█▲││▁ ││▁ │┊  ┊│█⟋││▇▲││▁ ││▁ ││█▲││▁ ││▃ ││▁ ││▅⟋││█▲│   ◀ the lit cell is the playing step  │ GUT
 │         │01││02││03││04││05│┊06┊│07││08││09││10││11││12││13││14││15││16│                                      │
 │         └──┘└──┘└──┘└──┘└──┘└┄┄┘└──┘└──┘└──┘└──┘└──┘└──┘└──┘└──┘└──┘└──┘                                      │
 ├──────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
 │ VOICE/07 GLIDE MODE ACC BEND │ OUT/08 SAT WIDTH MONO MASTER │ ◉SEQ SYNC BPM RATE LEN GATE SWG ROOT SCL MUT EVO │ GROUND
 └──────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
   while clipping: ╱╲╱╲╱╲╱╲╱╲╱╲ ⌈ CLIP ⌋ OUTPUT -0.3 dBFS ╱╲╱╲╱╲╱╲╱╲╱╲  across the top and bottom 16 px
```

`docs/design/undertow-mock.svg` is the same layout in pixels, with the palette.

## 7. What is checked, and what is judged

Checked automatically by phase 4's spec tests, on offscreen snapshots of the real editor driven by fixed `UiFrame`s
(`tests/spec/p4/ui_test.cpp`):

- The size (1200 x 760, fixed) and one attached control per parameter (57).
- The ground: at least half of the pixels are void (luminance under 0.06); no white or near-white pixel.
- The taxonomy: of the coloured pixels, at least 75% are in the rose hue band; bone-coloured pixels exist only while a
  note plays, cover at most 3% of the panel, and sit in one region of the CHEST (the one number).
- The hazard stripes: the top band is mostly `rose_bright` while clipping and empty otherwise.
- The tunnel: the CHEST is at least 1.4 times brighter at full energy than at silence, and moves between frames.
- The scope: a sine in the scope buffer draws a trace across at least 90% of the scope's width.
- The unit array: 16 cells left to right in the GUT; hit-testing finds each; the playing step is the brightest cell;
  edits reach the processor's pattern.
- The frame budget (section 5).
- Preview images for review: `build/previews/undertow-idle.png`, `-playing.png` and `-clipping.png`.

Judged by a person (the review), from the previews and the Standalone app:

- Does it read as one instrument, rose light on violet-black, calm when silent and alive when playing?
- Is the bone number the first thing the eye finds while a note plays, and does the hazard stripe feel like an alarm?
- Are the margins dense and the centre sparse, labels legible at 100% and on Retina, and nothing cramped or clipped?
- Does the tunnel stay behind the scope (never louder than the data), and does the pump field show the duck?
- Does any colour appear only as decoration? (It should not.)
