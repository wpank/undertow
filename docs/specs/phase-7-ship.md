# Phase 7: ship

**Goal**: Undertow 2 is ready to install and to show:
- the Fast attack gets the softer start that listening chose: the level rises in 1.2 ms and the filter in 0.6 ms;
- pluginval validates the plugin at its strictest, and its log is kept for review;
- the install is proven in a test folder.

Installing for the person, and auval on the installed AU, are their steps, at the end.

**Depends on**: phase 6.

**Size**: S, 2 tasks: the softer attack, then the strict validation's log; about 5 lines of code and one log.

---

## Scope

In scope:
- the voice's Fast attack (`core/include/undertow/voice.h`, `core/src/voice.cpp`);
- the strict validation's log, `artifacts/reports/pluginval-strict.txt` (new).

Given by this phase's seed, never edited by a task:
- in `tests/spec/p3/voice_test.cpp`: P3.09's new attack times, and P3.13's bound below 30 Hz at the PRD's 2%;
- `tests/spec/p7/ship_test.cpp` and `tests/spec/p7/install_test.cpp`;
- `tools/pluginval.sh`, which now keeps its log with paths relative to the checkout;
- failure messages that print each percentage as one number (P3.13, P4.16, P5.01, P5.17 and P5.21).

Out of scope:
- any other change to the sound;
- `tools/install.sh`: phase 1 made it Undertow 2's (P1.16), and this phase checks it again;
- Developer ID signing, notarization and an installer package (ad hoc signing only);
- the real install and auval (the person's steps, below).

## Context

- The PRD (`docs/PRD.md`) section 1.4 gives the attack, and section 3.2 the plugin's identity.
- **Why the softer attack.** With the level and the filter each rising in 1 ms, every hit starts with a small click.
  A listening test chose a softer start: the filter rises in 0.6 ms, so it is already open when the level arrives,
  and the level rises in 1.2 ms. Over the default line, the band above 1 kHz falls from 48.7 to 54.2 dB under the
  whole, and the same band in each hit's first 3 ms from 50.2 to 55.5 dB under. The notes, the decays and the low end
  stay as they are.
- **Below 30 Hz.** Each hit's start leaves a little power below 30 Hz, and the softer attack leaves a little more: in
  the default line, the voice alone, from 0.75% to 1.1% of the power. P3.13 held the voice alone to 1%; it now takes
  the PRD's 2% (section 1.1), as every preset's check at the output does (P5.17). The output stage's 20 Hz high-pass
  takes most of it: at the output, Init keeps 0.42%.
- The spec tests: `tests/spec/p3/voice_test.cpp` (P3.09's new times and P3.13's bound),
  `tests/spec/p7/ship_test.cpp` (core preset) and `tests/spec/p7/install_test.cpp` (plugin preset). Change the code,
  never the tests.
- The given tools:
  - `tools/pluginval.sh 10 <report>` builds the VST3 and validates it with pluginval 1.0.4 at strictness 10, seed
    42, without the GUI tests, which open windows. On success it keeps the log at `<report>`, with its paths relative
    to the checkout. Once the plugin is built the run takes seconds.
  - `tools/install.sh --dest <dir>` builds the plugin and installs its three bundles, signed ad hoc, into
    `<dir>/VST3`, `<dir>/Components` and `<dir>/Applications`; `--uninstall --dest <dir>` removes them.
  - `tools/auval.sh` runs auval on the installed AU, so only after a real install: the person's step.

## What to build

1. **The softer attack** (`core/include/undertow/voice.h`, `core/src/voice.cpp`)
   - Fast: the level envelope's attack is 1.2 ms and the filter envelope's 0.6 ms. Both are 10-to-90% times, as
     `Decay::setAttack` takes them (phase 3).
   - Two constants replace the one Fast time, for example `kFastLevelAttackMs = 1.2` and `kFastFilterAttackMs = 0.6`.
   - Slow stays: the level rises in 100 ms and the filter in 500 ms.
   - Nothing else changes: the envelope's shape, the decays, the restart rule and the smoothing stay as they are.
   - The checks that hear the attack still pass:
     - the voice (P3.06 to P3.14), in the same command as P3.09;
     - the envelope itself (P3.01 to P3.05), which this task leaves as it is;
     - the default patch at the output (P3.15 to P3.19);
     - every preset's sound (P5.15 to P5.22).

2. **The strict validation's log** (`artifacts/reports/pluginval-strict.txt`)
   - Once task 1 is in, run `tools/pluginval.sh 10 artifacts/reports/pluginval-strict.txt` and commit the log it
     keeps. Never write or edit the log by hand: the check reads what pluginval wrote.
   - pluginval already passes at strictness 10 on phases 1 to 6: the plugin restores every parameter after fuzzing.
     If a test fails, fix the plugin, never the log, and run it again.
   - The install stays as it is: `tools/install.sh --dest build/install-test` puts the three signed bundles in a test
     folder beside Undertow 1's, and `--uninstall` removes only them (P7.07).

## Acceptance checks

Each check is one command, run from the repository root; the checks that share a command are one verify step.

| Check | Command | Proves |
|---|---|---|
| P7.01 | `tools/spec.sh 3 voice` | The softer attack: Fast rises in 1.2 ms for the level and 0.6 ms for the filter, Slow in 100 ms and 500 ms (10 to 90%), and every other check of the voice still passes (P3.06 to P3.14) |
| P7.02 | `tools/spec.sh 3 decay` | The envelope is unchanged: half a cosine, 10 to 90% in the attack time it is given (P3.01 to P3.05). A regression check: it passes on the base too |
| P7.03 | `tools/spec.sh 3 engine` | The default patch at the output still peaks between -12 and -6 dBFS with the highs at every hit under -60 dBFS (P3.15 to P3.19). A regression check: it passes on the base too |
| P7.04 | `tools/spec.sh 5 presets` | Every preset still meets the sound goals: the range, the roll and the hold, the level, under the kick (P5.15 to P5.22). A regression check: it passes on the base too |
| P7.05 | `tools/spec.sh 7 ship` | The strict validation's log is kept: pluginval at strictness 10, seed 42, passed every test on Metakosmia Undertow 2 2.0.0, and the log names no folder outside the checkout |
| P7.06 | `tools/pluginval.sh 10` | pluginval passes at strictness 10 (without its GUI tests). A regression check: it passes on the base too |
| P7.07 | `tools/spec.sh 7 install` | In a test folder, never the person's own: the three bundles whole, signed, named, version 2.0.0, the AU's component entry; Undertow 1's beside them untouched; `--uninstall` removes only Undertow 2's. A regression check: it passes on the base too |
| P7.08 | `tools/check.sh --plugin` | The whole local check still passes: phases 1 to 7, both presets, pluginval |

## Rules every task keeps

- The rules of phases 1 to 6 still hold: never edit `tests/spec/`, `third_party/`, `docs/` or the given tools; `core/`
  stays plain C++17; nothing the audio thread calls allocates, locks, does I/O or logs.
- No task installs into the person's own folders (`~/Library/Audio/Plug-Ins`, `~/Applications`) or runs `auval`: the
  checks install into `build/install-test`.
- The one file a task commits outside the code is `artifacts/reports/pluginval-strict.txt`, as `tools/pluginval.sh`
  wrote it. Everything else a check writes stays in `build/`.

## Also required, not checked automatically

- Listening, under a plain kick at 130 BPM: each hit of the default line starts soft, with no click, and the line
  rolls as before.
- pluginval at strictness 10 with its GUI tests: `tools/pluginval.sh 10 --gui`, run once at the end with a heads-up to
  the person, since it opens editor windows on their screen.
- The person's install, at the end and only once they agree: `tools/install.sh`, then `tools/auval.sh`.
  - The AU passes auval; its log, `artifacts/reports/auval.txt`, is committed afterwards.
  - After a plug-in rescan a host finds Undertow 2 beside Undertow 1, and Undertow 1's sessions still load Undertow 1.

## Notes for the planner

- This spec is `docs/specs/phase-7-ship.md`. Put it first among the files every task reads: its "What to build" holds
  the two attack times and the log's rules.
- Two tasks, in order. Each task's verify steps are exactly its checks' commands, unchanged:
  1. **The softer attack.** Focused: about 5 lines.
     - Edits: `core/include/undertow/voice.h`, `core/src/voice.cpp`.
     - Its files are those 2 paths; it creates and deletes nothing.
     - Checks: P7.01, then the regression checks P7.02, P7.03 and P7.04 (`expect = "pass_on_base"`).
  2. **The strict validation's log.** Needs task 1. Mechanical: it runs one script and commits the log it keeps.
     - Creates: `artifacts/reports/pluginval-strict.txt`.
     - Its files are that 1 path; it edits and deletes nothing.
     - Checks: P7.05, then the regression checks P7.06 and P7.07 (`expect = "pass_on_base"`).
     - Its first plugin build in a fresh worktree compiles JUCE (about a minute at 4 jobs): give the script's command
       a few minutes.
- `tools/check.sh --plugin` (P7.08) is the plan's final check, not a task's.
- Both tasks run commands: each agent needs `bash` and `edit_file`, to run its script or `cmake` and its own check
  before it hands off. Leave `allowed_tools` out, or include `bash`, `edit_file`, `read_file`, `write_file`, `grep`
  and `glob`.
