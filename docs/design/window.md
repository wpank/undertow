# Undertow 2's window

The window is one fixed panel, 1200 x 760 logical pixels, laid out with the DFAM's panel logic and drawn in the
ROSEDUST phosphor language. It reads top to bottom as the sound is made: the **sequencer** makes the hits, the
**voice** (VCO, VCF, VCA) shapes them, the **rumble** and the **output** finish them. The right-hand column is the
console: how the pattern plays, what the sound is doing (a spectrum and the sub's pitch), and the output level.

From across a room you see whether Undertow is silent (dark and still), rolling (a lit step walking across the top,
the spectrum pulsing in its low band) or clipping (hazard stripes). Up close you read every value.

Phase 6 (`docs/specs/phase-6-window.md`) builds this design; its review images in `artifacts/ui/` show the window
idle, playing and clipping.

## 1. The rules

1. **Light follows significance.** The ground is near-black. The brightest things are the playing step, the
   spectrum's curve and the one number; legends sit low on the ramp.
2. **Colour is a taxonomy, never decoration.** Rose means sound and control. Bone is the one number. Indigo
   (`dream`) marks the window's own clock (no host). Nothing else has a colour of its own.
3. **One bone number:** the pitch of the sub (section 4.3). Nothing else is bone.
4. **Glass for what changes, void for what you touch.** Every live display sits on dark glass with a hairline
   bezel and faint scanlines: the step cells, the spectrum, the drift arc, the meter and the clock. Every control
   sits on the bare void. A reader learns the rule once and finds every moving number by it.
5. **Restraint.** No boxes around sections: a legend and a hairline rule mark each one. At least 80% of the panel
   stays near-black (luma under 0.06) in every state; a full mockup of this design measures 88% to 93%.
6. **Hardware logic.** Each voice section ends in its envelope: an amount above, the **decay** below, behind a
   dotted divider. The three decays sit in the same row, so short-roll and long-hold read as one family.
7. **Motion follows audio or time, never a random timer.** Controls answer at once and never animate on their own;
   only displays move. A frozen state draws the same picture every time.

## 2. Size, grid and zones

**Size: 1200 x 760, fixed, not resizable** (the size phase 1's placeholder already has). It holds the 49 controls at
40 px with room to spare and keeps the review images and the guide's figures at one scale.

**Grid:** a 4 px grid. Two columns: the **instrument** (x 24 to 760: the steps, the voice, the rumble) and the
**console** (x 784 to 1176: the pattern, the spectrum, the output), 24 px apart and 24 px from the edges. Three
tiers under the head. The step columns are 82 px wide from x 104; the rumble's knobs sit on the same columns, so the
panel has one vertical rhythm.

| Zone | Rectangle (x, y, w, h) | Holds |
|---|---|---|
| HEAD | 0, 0, 1200, 48 | the mark, the clock (glass), the LIVE lamp; `bg_mid` with a hairline under it |
| SEQUENCER, steps | 24, 70, 736, 250 | row labels (x 24 to 104), 8 step columns: a cell, a pitch knob, a velocity knob |
| SEQUENCER, pattern | 784, 70, 392, 250 | MODE, ROOT, RATE, LENGTH, TEMPO, DRIFT and the drift arc |
| VOICE | 24, 338, 736, 240 | VCO (x 24 to 368), VCF (400 to 628), VCA (656 to 760) |
| SPECTRUM | 784, 338, 392, 262 | the spectrum, the share below 150 Hz, the one number |
| RUMBLE | 24, 614, 736, 120 | 8 knobs in signal order, the ghost-kick lamp, the breathing rule |
| OUTPUT | 784, 614, 392, 120 | MASTER, the meter, the clip lamp |
| Hazard bands | 0, 0, 1200, 10 and 0, 750, 1200, 10 | only while the output clips |

```text
+-------------------------------------------------------------------------------------------------------------+
| UNDERTOW 2 | METAKOSMIA            [ 130.0 BPM  HOST  002.1.3 ]                                 (o) LIVE    |
+-------------------------------------------------------------------------------------------------------------+
| SEQUENCER ------------------------------------------------------------------------------------------------- |
| STEP     :1 REST: [2 ~~ ] [#3 ~~#] [4 ~~ ] :5 REST: [6 ~~ ] [7 +12] [8 ~~ ]    MODE [ KEY | FREE | NOTES ]  |
| PITCH       ( )     ( )     ( )     ( )     ( )     ( )     ( )     ( )       ( )   ( )    ( )    ( )       |
|              0       0       0       0       0       0       0       0       ROOT  RATE  LENGTH  TEMPO      |
| VELOCITY    ( )     ( )     ( )     ( )     ( )     ( )     ( )     ( )       ( )  [ ..::|||||||::.. ]      |
|             REST    0.22    1.00    0.55    REST    0.25    0.90    0.60     DRIFT  STEPS AWAY FROM HOME    |
| VCO ---------------------------- VCF ------------------- VCA ---------   [ SPECTRUM           49.00 Hz ]    |
|  VCO 1  VCO 2  NOISE  SUB   : PITCH            RES  : ENV                [ 98% BELOW 150 Hz G1 THE SUB ]    |
|  [/\_]  [/\_]         [0|-1]:         CUTOFF       :       [FAST|SLOW]   [     /\                      ]    |
|   ( )    ( )    ( )    ( )  :  ( )     (   )   ( )  :  ( )               [  __/  \_/\__                ]    |
|  LEVEL  LEVEL  LEVEL  LEVEL : AMOUNT          DRIVE : AMOUNT   ATTACK    [   |30-150 Hz|  \___         ]    |
|   ( )    ( )                :  ( )             ( )  :  ( )      ( )      [ 20 30 50 100 150 300 1k  2k ]    |
|  FM 1-2  TUNE               : DECAY                 : DECAY    DECAY                                        |
| RUMBLE (o) -------------------------------------------------------------   OUTPUT -----------------------   |
|             ( )     ( )     ( )     ( )     ( )     ( )     ( )     ( )      ( )  [#########:::::]  o       |
|             KICK    RATE    SEND    DECAY   TONE    DRIVE   DUCK    LEVEL   MASTER  -48 ...  -6  0  CLIP    |
+-------------------------------------------------------------------------------------------------------------+
  while clipping: //////////// CLIP  OUTPUT HELD AT -0.3 dBFS //////////// along the top and bottom 10 px
```

### Legends and rules

| Text | Where | Notes |
|---|---|---|
| `SEQUENCER` | x 24, baseline 82 | rule from the legend's end + 10 to x 1176 |
| `VCO`, `VCF`, `VCA` | x 24, 400, 656; baseline 350 | rules to x 368, 628, 760 |
| `RUMBLE`, `OUTPUT` | x 24, 784; baseline 626 | rules to x 760, 1176; RUMBLE's lamp sits at x 104 and its rule starts at x 114 |
| `STEP`, `PITCH`, `VELOCITY` | x 24; baselines 127, 203, 279 | the step rows' labels |
| `VCO 1`, `VCO 2`, `NOISE`, `SUB`, `PITCH` | centred on x 56, 120, 184, 248, 336; baseline 372 | the VCO's strip heads |
| `ENV` | centred on x 596, baseline 372 | the VCF's envelope strip |
| dotted dividers | x 292 and 556, y 362 to 570 | before each envelope strip |

A rule is 1 px, `rose` at 20% alpha, 3.5 px above its legend's baseline, starting 10 px after the legend.

## 3. Every parameter, placed

All 49 parameters of `docs/PRD.md` section 4, by id and table index, each with exactly one control. Centres are
logical pixels. A knob's dial is the size given; its component bounds are a square of the dial plus 12 px, centred
on it. The editor draws its legend centred under it, on a baseline 13.5 px below the dial, and its value 13 px below
that; a step knob has no legend, so its value sits 14 px below the dial. A switch is the rectangle given.

**Which values show at rest.** The top tier shows every value always: the steps and the pattern are read, not
felt. The voice, rumble and output knobs show their value only while hovered or dragged, where the legend's line
gains a second line. Switches show their state.

**Sequencer: the steps** (step `n` is column `n - 1`, centre x = 145 + 82 (n - 1))

| # | id | control | legend | centre (x, y) | default reads | value shows |
|---|---|---|---|---|---|---|
| 24 | `step1_pitch` | knob 40, bipolar, stepped | row label PITCH | 145, 200 | 0 | always |
| 25 | `step2_pitch` | knob 40, bipolar, stepped | row label PITCH | 227, 200 | 0 | always |
| 26 | `step3_pitch` | knob 40, bipolar, stepped | row label PITCH | 309, 200 | 0 | always |
| 27 | `step4_pitch` | knob 40, bipolar, stepped | row label PITCH | 391, 200 | 0 | always |
| 28 | `step5_pitch` | knob 40, bipolar, stepped | row label PITCH | 473, 200 | 0 | always |
| 29 | `step6_pitch` | knob 40, bipolar, stepped | row label PITCH | 555, 200 | 0 | always |
| 30 | `step7_pitch` | knob 40, bipolar, stepped | row label PITCH | 637, 200 | 0 | always |
| 31 | `step8_pitch` | knob 40, bipolar, stepped | row label PITCH | 719, 200 | 0 | always |
| 32 | `step1_vel` | knob 40 | row label VELOCITY | 145, 276 | REST | always |
| 33 | `step2_vel` | knob 40 | row label VELOCITY | 227, 276 | 0.22 | always |
| 34 | `step3_vel` | knob 40 | row label VELOCITY | 309, 276 | 1.00 | always |
| 35 | `step4_vel` | knob 40 | row label VELOCITY | 391, 276 | 0.55 | always |
| 36 | `step5_vel` | knob 40 | row label VELOCITY | 473, 276 | REST | always |
| 37 | `step6_vel` | knob 40 | row label VELOCITY | 555, 276 | 0.25 | always |
| 38 | `step7_vel` | knob 40 | row label VELOCITY | 637, 276 | 0.90 | always |
| 39 | `step8_vel` | knob 40 | row label VELOCITY | 719, 276 | 0.60 | always |

**Sequencer: the pattern**

| # | id | control | legend | centre (x, y), or switch x, y, w x h | default reads | value shows |
|---|---|---|---|---|---|---|
| 18 | `seq_mode` | switch, 3 segments | MODE (x 784, baseline 127.5) | 840, 112, 336 x 24 | KEY / FREE / NOTES | its state |
| 22 | `seq_root` | knob 40, stepped | ROOT | 833, 200 | G2 | always |
| 19 | `seq_rate` | knob 40, 8 detents | RATE | 931, 200 | 1/16 | always |
| 20 | `seq_length` | knob 40, 8 detents | LENGTH | 1029, 200 | 8 | always |
| 23 | `seq_bpm` | knob 40 | TEMPO | 1127, 200 | 130.0 (HOST under a host's clock) | always |
| 21 | `seq_drift` | knob 40 | DRIFT | 833, 276 | 0.70 | always |

**VCO** (four sources, each a strip: its switch under its head, its level, and for the oscillators one more knob;
then the pitch envelope)

| # | id | control | legend | centre (x, y), or switch x, y, w x h | default reads | value shows |
|---|---|---|---|---|---|---|
| 0 | `vco1_wave` | switch, 2 segments, wave glyphs | strip head VCO 1 | 30, 380, 52 x 18 | triangle / square | its state |
| 1 | `vco1_level` | knob 40 | LEVEL | 56, 430 | 0.45 | hover, drag |
| 5 | `vco_fm` | knob 40 | FM 1-2 | 56, 518 | 0.15 | hover, drag |
| 2 | `vco2_wave` | switch, 2 segments, wave glyphs | strip head VCO 2 | 94, 380, 52 x 18 | triangle / square | its state |
| 4 | `vco2_level` | knob 40 | LEVEL | 120, 430 | 0.28 | hover, drag |
| 3 | `vco2_tune` | knob 40, bipolar | TUNE | 120, 518 | -12 st | hover, drag |
| 6 | `noise_level` | knob 40 | LEVEL | 184, 430 | 0.05 | hover, drag |
| 8 | `sub_octave` | switch, 2 segments | strip head SUB | 222, 380, 52 x 18 | 0 / -1 | its state |
| 7 | `sub_level` | knob 40 | LEVEL | 248, 430 | 1.00 | hover, drag |
| 9 | `pitch_env` | knob 40 | AMOUNT | 336, 430 | 7 st | hover, drag |
| 10 | `pitch_decay` | knob 40 | DECAY | 336, 518 | 75 ms | hover, drag |

**VCF** (the cutoff large, between the rows; then resonance and drive; then the filter envelope)

| # | id | control | legend | centre (x, y) | default reads | value shows |
|---|---|---|---|---|---|---|
| 11 | `vcf_cutoff` | knob 64 | CUTOFF | 440, 474 | 140 Hz | hover, drag |
| 12 | `vcf_res` | knob 40 | RES | 516, 430 | 0.32 | hover, drag |
| 13 | `vcf_drive` | knob 40 | DRIVE | 516, 518 | 0.40 | hover, drag |
| 14 | `vcf_env` | knob 40 | AMOUNT | 596, 430 | 2 oct | hover, drag |
| 15 | `vcf_decay` | knob 40 | DECAY | 596, 518 | 275 ms | hover, drag |

**VCA**

| # | id | control | legend | centre (x, y), or switch x, y, w x h | default reads | value shows |
|---|---|---|---|---|---|---|
| 16 | `vca_attack` | switch, 2 segments | ATTACK (centred x 706, baseline 463.5) | 660, 418, 92 x 24 | FAST / SLOW | its state |
| 17 | `vca_decay` | knob 40 | DECAY | 706, 518 | 390 ms | hover, drag |

**Rumble** (in signal order: the ghost kick and its rate, the voice's send, the room's decay, tone and drive, the
duck, and the bed's level last, beside the output; on the step columns)

| # | id | control | legend | centre (x, y) | default reads | value shows |
|---|---|---|---|---|---|---|
| 42 | `rumble_kick` | knob 40 | KICK | 145, 678 | 0.60 | hover, drag |
| 43 | `rumble_rate` | knob 40, 4 detents | RATE | 227, 678 | 1/4 | hover, drag |
| 41 | `rumble_send` | knob 40 | SEND | 309, 678 | 0.20 | hover, drag |
| 44 | `rumble_decay` | knob 40 | DECAY | 391, 678 | 4.5 s | hover, drag |
| 45 | `rumble_tone` | knob 40 | TONE | 473, 678 | 100 Hz | hover, drag |
| 46 | `rumble_drive` | knob 40 | DRIVE | 555, 678 | 0.50 | hover, drag |
| 47 | `rumble_duck` | knob 40 | DUCK | 637, 678 | 0.88 | hover, drag |
| 40 | `rumble_level` | knob 40 | LEVEL | 719, 678 | 0.30 | hover, drag |

**Output**

| # | id | control | legend | centre (x, y) | default reads | value shows |
|---|---|---|---|---|---|---|
| 48 | `master_gain` | knob 44 | MASTER | 820, 678 | -12.0 dB | hover, drag |

### Readouts

Each value reads as follows (the formats the checks compare):

| Parameters | Format | Examples |
|---|---|---|
| `step*_pitch` | whole semitones, signed when not 0 | `0`, `+7`, `-5`, `+12` |
| `step*_vel` | `REST` at 0, else two decimals | `REST`, `0.22`, `1.00` |
| `seq_root` | the note name, MIDI 60 = C4 | `G2`, `C#3` |
| Choices (`seq_rate`, `rumble_rate`) | the table's label | `1/16`, `1/8T`, `1 bar` |
| `seq_length` | the number | `8` |
| `seq_bpm` | one decimal; `HOST` while a host's clock drives | `130.0`, `HOST` |
| 0 to 1 without a unit (levels, FM, noise, sub, res, drive, drift, the rumble's 0 to 1 knobs) | two decimals | `0.45`, `0.05` |
| `vco2_tune`, `pitch_env` | number and `st`; tune signed when not 0 | `-12 st`, `+7 st`, `7 st` |
| Times in ms | below 1000 ms in ms, else in s; 0 decimals from 100, 1 from 10, else 2, trailing zeros cut | `75 ms`, `390 ms`, `1.5 s`, `20 s` |
| `rumble_decay` | seconds, the same decimals rule | `4.5 s`, `0.2 s` |
| `vcf_cutoff`, `rumble_tone` | Hz below 1000, else kHz, the same rule | `140 Hz`, `1.2 kHz` |
| `vcf_env` | octaves | `2 oct`, `0.5 oct` |
| `master_gain` | one decimal, signed when not 0, `dB` | `-12.0 dB`, `0.0 dB`, `+3.5 dB` |

## 4. The displays

### 4.1 The step cells: the step row lit in time

Eight glass cells, one above each step column: `x = 108 + 82 (n - 1), y 96, 74 x 56`, radius 3. Each is a small
oscilloscope of its step's last hit, so the row reads as the pattern itself: the rests are empty, the strong hits
are tall, and how far a hit's trace reaches across its cell is its decay.

- **The step number**, `1` to `8`, top left (x + 7, baseline y + 13), Geist Mono 9.5 px.
- **The trace** fills `x + 4, y + 18, 66 x 34`. One min/max pair per pixel column, drawn as a filled band (the
  phosphor) with its upper and lower edges as 1 px strokes. A sample of +-0.4 (-8 dBFS) reaches the half height;
  larger ones are clamped. When step `n` starts (`meters().step` changes), its cell is cleared and its trace sweeps in
  from the left as the step plays, at 66 px per step length (the step length from `seq_rate` and `meters().bpm`), from
  the newest output samples (`copyScope`). A bright 1 px sweep head marks where it is.
- **Lit:** the playing step's cell has a `rose_bright` border (1.2 px), an ember fill fading downwards, a soft outer
  glow and a `rose_bright` trace, and its two knobs' arcs, pointers and readouts turn `rose_bright` too: the whole
  column lights. When the step moves on, the column fades back over 250 ms (ease-out), a phosphor trail across the
  row.
- **Older traces** stay, dimmer with age: `alpha = max(0.28, 0.85 - 0.09 * age)`, where `age` is the number of
  steps since that cell last lit.
- **A rest** (velocity 0): a dashed outline (`rule2`, 3 on, 3 off), the word `REST` centred in `text_ghost`, no trace.
- **Past the length** (`n > seq_length`): the whole column, cell and both knobs and readouts, at 30% opacity, still
  editable.
- **Drift marks:** a step the drift has moved on the current pass shows how, top right (right edge x + 68, baseline
  y + 13), in `rose_bright`: the change in semitones (`+12`, `+7`, `-5`, `-12`) in Geist Mono 9 px, or `GHOST` in
  Michroma 6 px for a ghost hit at half its velocity. Rests never drift. The marks compare `Pattern::at(count, root)`
  with `Pattern::home(step)` for each step's count on the current pass, with the playing step's root (section 10), so
  the reader sees which steps have wandered and watches them come home.
- **Not playing:** no cell is lit; the traces of the last pass stay at their faded alpha.

### 4.2 The spectrum: where the energy sits

The console's glass, `784, 338, 392 x 262`, radius 6, with scanlines and a `+` crosshair every 32 px in
`text_phantom`. It shows the output's spectrum from 20 Hz to 2 kHz on a log axis, with the body band, 30 to 150 Hz,
marked: the instrument's whole claim, visible at a glance.

- **The plot:** `798, 404, 364 x 166`. x is log frequency, 20 Hz at the left edge to 2 kHz at the right; y is level,
  -18 dB at the top to -96 dB at the bottom (0 dB = a full-scale sine). Dotted level lines at -36, -60 and -84 dB,
  labelled at the right edge (Geist Mono 8 px, `text_dim`). Ticks and labels under the axis at 20, 30, 50, 100, 150,
  300, 500, 1k and 2k (Geist Mono 8.5 px, `text_dim`; 30 and 150 in `rose`).
- **The band:** 30 to 150 Hz, a fill of `rose` rising from 0% alpha at the top to 9% at the bottom, dashed `rose` edges
  at 30% alpha, and the legend `30-150 Hz` (Michroma 6.5 px, `rose_dim`) centred in it on baseline 564, 6 px above the
  axis, drawn over the curves so a loud sub never hides it.
- **The analysis** (30 times a second, on the UI thread):
  - the newest 16384 output samples (`copyScope`), a 4-term Blackman-Harris window, a 16384-point FFT, magnitudes
    scaled so a full-scale sine's peak bin reads 0 dB;
  - 220 display points, log-spaced from 20 Hz to 2 kHz. Each is the power in a band around it: a twelfth of an octave
    either side, widened to at least 1.5 bins either side, with fractional bins at its edges, summed and divided by
    the window's equivalent noise bandwidth (2.0 bins). So a sine reads its own level (within 0.4 dB) at its own
    frequency (within 1 px from 40 Hz up), and pink noise reads flat. In dB, floored at -96;
  - each point moves towards its new value with a 25 ms time constant rising and 300 ms falling.
- **The curve:** the newest points as a 1.5 px `rose_bright` line over two glow strokes (3.5 px at 30% alpha, 7 px at
  16%), with a fill under it (`rose` 42% at the top, `rose_ember` 30%, `rose_deep` 5% at the axis). The four curves
  before it stay behind as phosphor, at 55%, 36%, 22% and 12% alpha, in `rose_dim` then `rose_ember`. Every curve is
  clipped to the plot, 1 px above its bottom edge: the floor (-96 dB) lies on the axis, out of sight, so silence draws
  no curve at all.
- **The share below 150 Hz,** top left (x 798, baseline 390): the power from 20 to 150 Hz over the power above 20 Hz,
  from the newest frame, smoothed with a 500 ms time constant, as a whole percent: `98` in Geist Mono Light 22 px
  `rose`, `%` in 12 px `rose_dim`, then `BELOW 150 Hz` (Michroma 6.5 px, `rose_dim`). Hidden while the output is
  silent.
- **The stamp,** top left (x 798, baseline 360): `SPECTRUM` (Michroma 7.5 px, `rose`) and `20 Hz - 2 kHz` (Geist Mono
  8.5 px, `text_dim`).
- **The hint,** while nothing sounds, on a small glass plate centred in the plot at 40% of its height: `HOLD A KEY TO
  PLAY THE PATTERN` in Key mode, `PLAY MIDI NOTES` in Notes mode, nothing in Free mode (Michroma 7.5 px, `rose_dim`).
  It fades out over 300 ms when sound starts.

### 4.3 The one number

The sub's pitch: the fundamental the instrument is built around, at the top right of the spectrum's glass.

- **The value:** `meters().noteHz` (the note's pitch, without the thud), halved while `sub_octave` is `-1 oct`. So
  holding G2 shows `49.00`, the sub at G1. Two decimals below 1000 Hz, one from 1000 Hz.
- **The digits:** Geist Mono Light 44 px, `bone`, right-aligned at x 1134 on baseline 386. `Hz` in Geist Mono 14 px,
  `bone_dim`, right-aligned at x 1160 on the same baseline. No glow: bone on glass is already the brightest thing in the
  window.
- **The label under them:** the sub's note name and what the number is, `G1 · THE SUB` (Michroma 7 px, `bone_dim`,
  right-aligned at x 1160, baseline 406).
- **Changes:** a new pitch moves the digits from the old one over 120 ms (ease-out); they never flash.
- **Idle:** when the voice falls idle (`meters().note == -1`), the number fades to `text_ghost` over 600 ms and stays
  until the next note. Before the first note nothing is drawn there.
- Nothing else in the window is `bone` or `bone_dim`.

### 4.4 The drift arc

Beside DRIFT, a glass strip `884, 259, 292 x 36`: the drift's arc of 16 passes as 16 bars, each as tall as the number
of steps away from home on that pass, `min(S, floor(3 * seq_drift * sin(pi * j / 16) + 0.5))` for pass `j`, where `S`
is the number of sounding steps among the first `seq_length` (the recipe of `docs/specs/phase-4-sequencer.md`),
against a full height of 3 steps (20 px). The bars show the shape of the wandering before it happens: none at the
start of the arc, the most in the middle, none at the end.

- Bars 14.4 px wide with 3 px gaps, from x 892, bottom at y 287; at least 1.5 px tall.
- While the pattern plays: the current pass `rose_bright` with a glow; the passes before it `rose` at 55%; the passes
  after it `rose_dim` at 35%. Not playing: all `rose_dim` at 55%.
- Under it: `STEPS AWAY FROM HOME` (Michroma 8 px, `legend`, x 884, baseline 308.5) and, right-aligned at x 1176,
  `PASS 5/16` while playing or `16 PASSES` at rest (Geist Mono 9.5 px).

### 4.5 The head

- **The mark:** `UNDERTOW` (Michroma 13 px, tracking 5.4 px, `rose`) at x 24, baseline 29.5; `2` (Michroma 13 px,
  `rose_dim`) at x 188; a 1 px `rule2` divider at x 216 from y 17 to 31; `METAKOSMIA` (Michroma 7 px, tracking 2.4 px,
  `rose_dim`) at x 230, baseline 28.5.
- **The clock,** on glass `488, 12, 224 x 26`: the tempo in use, `meters().bpm`, as `130.0` (Geist Mono 12 px, `value`)
  and `BPM` (Michroma 6.5 px, `rose_dim`); a badge (40 x 13 at x 584) reading `HOST` (`rose_dim`) while a host's
  transport drives the clock, or `INT` (`dream`) on the window's own clock; the position `002.1.3`, bar, beat and
  sixteenth from `meters().ppq` in 4/4 (Geist Mono 12 px, `value`, right-aligned at x 698).
- **LIVE,** right: a lamp (r 3.2 at x 1140, y 24) and `LIVE` (Michroma 7 px, tracking 1.6 px, at x 1151, baseline 27).
  Lit (`rose_bright` with a glow, the word in `rose`) while the output's RMS is above -60 dBFS; dark (`glass` with a
  `rule2` ring, the word in `text_dim`) otherwise.

### 4.6 The rumble's lamp and rule

- The lamp after `RUMBLE` (r 2.6 at x 104, y 622.5) lights `rose_bright` for 120 ms at each ghost kick: each time the
  beat clock crosses a multiple of the kick's grid (1, 2, 4 or 0.5 beats for `1/4`, `1/2`, `1 bar`, `1/8`) while the
  clock runs and `rumble_kick` is above 0. Dark, it is `glass` with a `rule2` ring.
- The rule after it breathes with the bed: with `g` the rumble's level (`meters().rumble`) mapped from -60 to -12 dBFS
  onto 0 to 1, the rule (x 114 to 760, on y 622) is `rose` at `0.20 + 0.45 g` alpha and `1 + g` px wide. The duck
  shows as the rule dimming on every kick.

### 4.7 The meter

- Glass `868, 672, 268 x 12`, radius 2: -48 to 0 dBFS, linear in dB. The output RMS (`meters().rms`) as a fill from
  `rose_deep` through `rose_dim` to `rose`; the peak (`meters().peak`, held 1.5 s, then falling 20 dB/s) as a 2 px
  `rose_bright` tick on whole pixels.
- Ticks under it at -48, -36, -24, -12, -6 and 0, labelled in Geist Mono 8 px, `text_dim`. Above its right end, the
  held peak: `PEAK -12.1 dBFS` (Geist Mono 8.5 px, `value`).
- The clip lamp at (1158, 678), r 3.2, with `CLIP` under it: lit `rose_bright` while `meters().clipping`.

### 4.8 The hazard stripes

While `meters().clipping`: bands across the top and bottom 10 px of diagonal stripes (45 degrees, a 12 px period,
6 px wide) in `rose_bright` at 92% on `bg_void`, scrolling left at 40 px/s. A `bg_void` plate in the middle of the top
band reads `CLIP  OUTPUT HELD AT -0.3 dBFS` (Michroma 6.5 px, `rose_bright`). On the first clipping frame every glass
bezel (the cells, the spectrum, the arc, the meter, the clock) flashes `rose_bright` and fades over 200 ms. The bands go
when the meter clears.

### 4.9 Modes and clocks

- **Key** (the default): as drawn. Idle, the spectrum shows the hint.
- **Free:** as drawn, no hint.
- **Notes:** the eight step columns at 40% opacity with no cell lit, and a glass plate centred over the cells reading
  `NOTES MODE: EACH MIDI NOTE IS ONE HIT` (Michroma 7.5 px, `rose_dim`). The drift arc dims to its rest state.
- **A host's clock:** TEMPO dims to 32% and reads `HOST`; the host sets the tempo. On the window's own clock (the
  Standalone app) TEMPO is live and the badge reads `INT`.

## 5. Type

Two typefaces, bundled with the plugin (no system fonts, so it looks the same on every Mac):

- **Michroma** (SIL Open Font License 1.1): an extended panel-legend face, for every word.
- **Geist Mono** (SIL Open Font License 1.1), Regular and Light: for every number.

The three files, `Michroma-Regular.ttf` (64 KB), `GeistMono-Regular.ttf` and `GeistMono-Light.ttf` (148 KB each), go in
`third_party/fonts/` with their licence texts and are compiled in as JUCE binary data.

| Use | Face | Size (px) | Tracking (px) | Colour |
|---|---|---|---|---|
| The mark `UNDERTOW` | Michroma | 13 | 5.4 | `rose` |
| `2`, `METAKOSMIA` | Michroma | 13, 7 | 0, 2.4 | `rose_dim` |
| Section legends (`SEQUENCER`, `VCO` ...) | Michroma | 9 | 2.3 | `rose` |
| Strip heads (`VCO 1` ... `ENV`) | Michroma | 7 | 1.2 | `rose_dim` |
| Knob legends, row labels, `STEPS AWAY FROM HOME` | Michroma | 8 | 1.1 | `legend` |
| Switch segments | Michroma (Geist Mono 9.5 for `0`, `-1`) | 7 | 1.1 | `rose_bright` when on, `text_dim` off |
| Values | Geist Mono Regular | 9.5 | 0.2 | `value` (always shown), `rose_bright` (hovered, or lit) |
| Step numbers | Geist Mono Regular | 9.5 | 0.2 | `text_dim`; `rose_bright` lit; `text_ghost` on a rest |
| The clock | Geist Mono Regular | 12 | 0.2 to 0.4 | `value` |
| The one number | Geist Mono Light | 44; `Hz` 14 | -0.5 | `bone`; `bone_dim` |
| The share | Geist Mono Light | 22; `%` 12 | 0 | `rose`; `rose_dim` |
| Axis and level labels | Geist Mono Regular | 8 to 8.5 | 0.2 | `text_dim` (30 and 150 in `rose`) |
| Small stamps (`REST`, `CLIP`, `BELOW 150 Hz`, `30-150 Hz`, `GHOST`) | Michroma | 6 to 6.5 | 1.0 to 1.2 | as in section 4 |

**Tracking** is added after every letter: JUCE's extra kerning is a multiple of the font's height, so a font with `t` px
of tracking is `font.withExtraKerningFactor(t / font.getHeight())`, and a centred word sits half its tracking right of
its anchor.

**Text is ASCII,** plus the middle dot (U+00B7) in `G1 · THE SUB`, written as
`juce::String::charToString(0x00B7)`: neither face has arrows, triangles, `⌈ ⌋` or `◉`, so the wave glyphs, lamps and
brackets are drawn as paths, never as characters.

## 6. Colour

The ROSEDUST tokens, plus seven refinements for the glass and the controls, every one in the rose hue band. Contrast
is against `bg_void`.

| Token | Hex | Contrast | Use |
|---|---|---|---|
| `bg_void` | `#060608` | | the ground, with a faint vignette (`#0A080C` at the centre to `#040405` at the corners) and a sparse noise |
| `bg_mid` | `#080810` | | the head |
| `glass` | `#070609` | | every live display's face, with scanlines (one dark row in three, 34% black) |
| `rule` | `#1E1620` | | glass bezels |
| `rule2` | `#30222E` | | switch outlines, dividers, a rest's dashed outline |
| `rose` | `#AA7088` | 5.2:1 | value arcs, section legends, the spectrum's fill |
| `rose_bright` | `#CC90A8` | 7.8:1 | the brightest thing: the lit step, the curve, a hovered knob, lamps, switches that are on, clipping |
| `rose_dim` | `#7A5060` | 3.0:1 | strip heads, small stamps, phosphor history |
| `rose_ember` | `#482838` | | the lit cell's fill, phosphor residue |
| `rose_deep` | `#3A2030` | | empty trace lines, the bottom of fills |
| `legend` | `#8E6474` | 4.1:1 | knob legends and row labels (`rose_dim` lifted to read at 8 px) |
| `value` | `#988090` | 5.6:1 | values that always show (`text_primary`) |
| `track` | `#2E1B28` | | a knob's track |
| `pointer` | `#A8829A` | | a knob's pointer at rest (under rose_bright's brightness, so only a lit or hovered knob is bright) |
| `cap` | `#1D151C`, `#120D12` at 55%, `#0A070A`; rim `#2E202C` | | a knob's cap, lit from the top left |
| `bone` | `#C8B890` | 10.3:1 | the one number only |
| `bone_dim` | `#8A7A5A` | | its unit and note line only |
| `text_dim` | `#584858` | 2.4:1 | quiet labels: axis labels, switch segments that are off |
| `text_ghost` | `#302830` | | rests, the idle number, steps past the length |
| `text_phantom` | `#201820` | | the crosshair grid, level lines |
| `dream` | `#585878` | | the `INT` badge only |

In code every token is a `juce::Colour` in `undertow::ui::theme`, named in camel case (`bg_void` is `bgVoid`); the
cap's stops and rim are `capHi`, `capMid`, `capLo` and `capRim`. Phase 6's checks read each one's value.

White never appears; the brightest pixel is `rose_bright` or `bone`. Colour never carries state alone: every state
also has a shape, a glyph or a word (a dashed outline for a rest, `REST`, `HOST`, `CLIP`, the stripes).

## 7. Motion

| What | Driven by | How |
|---|---|---|
| The lit column | `meters().step` | on at once at the step's start; fades back over 250 ms (ease-out) |
| A cell's trace | the newest output samples | sweeps across its cell over one step; older traces fade by age |
| The spectrum | the newest 16384 samples | 30 updates a second; 25 ms rising, 300 ms falling; four phosphor curves |
| The share | the same frames | 500 ms smoothing |
| The one number | `meters().noteHz`, `note`, `sub_octave` | 120 ms between pitches; 600 ms fade to `text_ghost` when idle |
| The drift arc | `meters().count` | the current pass steps along once per pass |
| The rumble rule | `meters().rumble` | follows the level every frame |
| The ghost-kick lamp | `meters().ppq`, `rumble_rate` | 120 ms flash per kick |
| LIVE | `meters().rms` | on above -60 dBFS |
| The meter | `meters().rms`, `peak` | every frame; the peak held 1.5 s, then -20 dB/s |
| Hover | the mouse | the arc gains its glow and the value appears at once; no fade |
| The hazard stripes | `meters().clipping` | scroll left at 40 px/s; the 200 ms flash on the first frame |
| The idle hint | silence | fades out over 300 ms when sound starts, back in after 1 s of silence |

## 8. Interaction

| Control | Gestures |
|---|---|
| **Knob** (continuous) | Drag up or right to raise it, down or left to lower it: 250 px covers the whole range, along the parameter's skew (cutoff, the decays and the rumble's decay and tone turn evenly in their musical scale). Double-click: back to the default. Mouse wheel: small steps. Hover: the arc glows and the value shows under the legend. Each drag is one host gesture (`beginChangeGesture` to `endChangeGesture`), so automation records it cleanly. |
| **Stepped knob** (`seq_root`, `seq_rate`, `seq_length`, `rumble_rate`, the 8 step pitches) | The same gestures, snapping to whole positions (250 px still covers the range, so a step's pitch moves a semitone every 10 px), and one position per wheel notch. Up to 8 positions show as detent dots around the dial, the current one lit. |
| **Bipolar knob** (`step*_pitch`, `vco2_tune`) | The arc grows from 12 o'clock, where a tick marks 0. Double-click returns to the default, as on every knob (0 for a step, -12 st for TUNE). |
| **Switch** (`seq_mode`, the two waves, `sub_octave`, `vca_attack`) | Click a segment to choose it; the chosen segment lights. One click is one complete gesture. |
| **Step cell** | Click to make the step a rest, and click again to bring back its velocity (the last one it had, or 0.80 if it never had one), as one gesture on `step<n>_vel`. The cell shows the result at once. |
| **Displays** (spectrum, drift arc, meter, clock, lamps) | None: they only show. |

Every control is attached to its parameter through the processor's `AudioProcessorValueTreeState`, so the host, the
window and automation always agree; a control never holds a value of its own. Each control's component is named by
its parameter id and titled by its parameter name, for accessibility and for the checks.

## 9. Drawing and the frame budget

- One 60 Hz timer while the window is open; nothing runs when it is closed. Each tick reads the engine through its
  lock-free outputs (`meters()`, `copyScope`), advances the animations by the elapsed time and repaints only what
  moves: the head's clock and lamp, the step cells and the lit column's knobs, the drift arc, the spectrum, the rumble
  rule and lamp, the meter, and the hazard bands.
- The static layer (the void with its vignette and noise, legends, rules, dividers, glass faces and bezels, knob
  tracks, caps and their shadows, the spectrum's grid, band and axis) is drawn once per display scale at that scale
  and then copied 1:1.
- Glows are layered strokes at low alpha, never blurs. The spectrum's FFT runs on every other tick.
- Budget, as phase 6's check measures it, counting the painting thread's own CPU time: a full repaint at most a median
  of 11 ms and a 95th percentile of 16 ms at 1x, and a median of 27 ms at 2x.

## 10. What the engine gives the window

| Need | From | State |
|---|---|---|
| Step, note, pitch, levels, clip, clock | `meters()`: `step`, `note`, `noteHz`, `rms`, `peak`, `clipping`, `rumble`, `ppq`, `hostClock`, `bpm` | phases 1 to 4 |
| 16384 newest samples for the spectrum (341 ms at 48 kHz) | `copyScope` | phase 6 raises `kScopeSamples` from 4096 to 16384: at 4096 a Blackman-Harris window smears the 49 Hz sub across 25 to 70 Hz |
| The count and the root of the step playing, for the drift marks and the arc | `meters()`: `count`, `root` (from `Sequencer::count()` and `root()`) | phase 4 |
| The pattern's drift, to compare with home | `undertow::Pattern` (phase 4), fed the parameters' current values | in place: the window keeps its own `Pattern` and calls `setParams`, `at(count, root)` and `home(step)` |
| Parameter values | the processor's `AudioProcessorValueTreeState` | in place |

The spectrum's analysis (window, FFT, display points, smoothing and the share) is plain arithmetic: phase 6 can build
it as a small core class with its own fast test, so the window only draws its points.

## 11. What is checked, and what is judged

Phase 6's checks drive a frozen window with fixed frames, measure offscreen snapshots of it at 1x and 2x, and read
its component tree and every word it draws:

- **Layout:** 1200 x 760, fixed. One control per parameter, each named by its id, of the kind in section 3, its
  centre within 2 px of the table, its dial at least the size given. Every legend, row label, strip head and stamp of
  section 2 drawn where it says, and no drawn word overlapping another or leaving its zone.
- **Readouts:** every format of section 3's readout table, at the defaults and at chosen values.
- **Colour:** at least 80% of the pixels near-black (luma under 0.06) in every state; no white or near-white; at
  least 90% of the coloured pixels (saturation and brightness at least 0.15) in the rose hue band, 300 to 15
  degrees; bone-like pixels only while a note plays, all inside the number's box (x 940 to 1170, y 340 to 412), and
  none before the first note. A full mockup of this design measures 88% to 93% near-black, 95% to 98% of its
  coloured pixels rose, and 985 bone-like pixels while playing, all in the box.
- **The step cells:** eight cells left to right at the table's places; the playing step's cell is the brightest; a
  rest is empty; steps past the length dim; a sine in the scope draws a trace across at least 90% of the lit cell
  after one step; a drifted step shows its mark. A click toggles a rest and restores the velocity.
- **The spectrum:** a 49 Hz sine at -12 dBFS peaks within 1.5 px of 49 Hz and 0.5 dB of -12 dB on the plot, and a
  98 Hz one within 1.5 px of 98 Hz; pink noise, averaged over a second, reads flat within 3 dB from 100 Hz to 2 kHz;
  the share reads 99% or more for the sine; the band, labels and hint are where section 4.2 puts them.
- **The one number, the clock, LIVE, the meter, the stripes:** each reads what the frame says, and the stripes show
  only while clipping.
- **The budget** of section 9, and the review images: `artifacts/ui/window-idle.png`, `window-playing.png` and
  `window-clipping.png` at 1x, with 2x copies (2400 x 1520) for the guide.

Judged by a person, from the review images and the Standalone app:

- Does it read as one instrument, rose light on near-black glass, calm when silent and alive when playing?
- From across the room: does the lit step walking the top row and the spectrum pulsing in its band say "rolling"?
- Is the sub's number the first thing the eye finds when a note plays, and are legends readable at 1x and on Retina?
- Does each decay read as the end of its section, and does the drift arc make the drift obvious?
- Is anything decorative, cramped or clipped? (Nothing should be.)
