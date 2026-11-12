# Phase 7.1: silent until played

**Goal**: A new Undertow 2 in the Standalone app is silent until it is played.
- With no host's clock, in Key and Notes modes, the engine's own clock runs only while a key is held. The room's ghost
  kick and duck run with it.
- The first key starts the clock at beat 0 on its own sample: step 1 plays at once, and the ghost kick lands with it.
- The last key's release stops the clock and returns it to beat 0. The note and the room's tail ring out.
- Free mode, and everything under a host, stay as they are.

Last, pluginval's strict log is made again from the final build.

**Depends on**: phase 7.

**Size**: S, 2 tasks: the own clock (about 20 lines), then the log.

---

## Scope

In scope:
- the held-key query (`core/include/undertow/sequencer.h`, `core/src/sequencer.cpp`);
- the own clock's rule (`core/src/engine.cpp`);
- `artifacts/reports/pluginval-strict.txt`, made again.

Given by this phase's seed, never edited by a task:
- `tests/spec/p7_1/standalone_test.cpp` and `tests/spec/p7_1/keys_test.cpp`;
- three checks that play the own clock with no key, now in Free mode: P1.06's own-clock part
  (`tests/spec/p1/engine_test.cpp`), P1.09's (`tests/spec/p1/rumble_test.cpp`) and P6.01's room
  (`tests/spec/p6/spectrum_test.cpp`).

Out of scope:
- anything under a host;
- the rumble, the voice and the sequencer's tick, which already take the clock sample by sample;
- the window: it reads the meters, so at rest it shows the hint, an empty meter and a dark LIVE lamp with no change.

## Context

- The PRD (`docs/PRD.md`) section 3.4 gives the clock, and section 1.5 the modes.
- **Why.** A new Standalone instance thumps with no key held.
  - Phase 1's rule runs the own clock whenever there is no host, and phases 4 and 5 kept it.
  - So the ghost kick fires on its grid with nothing played: the window's LIVE and rumble lamps light, and the
    "hold a key" hint never shows.
  - Under a host, a stopped transport is already silent.
- The spec tests: `tests/spec/p7_1/standalone_test.cpp` and `keys_test.cpp` (core preset). Change the code, never the
  tests.
- The engine already passes every note-on and note-off to the sequencer, Notes mode included, and the sequencer
  already counts the keys that are down. It only lacks a way to say so.

## What to build

1. **The held-key query** (`core/include/undertow/sequencer.h`, `core/src/sequencer.cpp`)
   - `bool anyKeyHeld() const`: true while at least one key is down, in every mode.
   - It counts the keys the sequencer already counts: `keyOn`, `keyOff`, `allKeysOff` and `reset()`.

2. **The own clock's rule** (`core/src/engine.cpp`)
   - **With a host position** (`Transport::valid`): no change.
   - **Without one:**
     - in Free mode the own clock always runs, as now;
     - in Key and Notes modes it runs only while `sequencer.anyKeyHeld()`. Read it for each sample, after that
       sample's MIDI.
   - **When it starts** (it was stopped and now runs: the first key, or a change to Free), it starts at beat 0 on that
     sample.
   - **When it stops** (the last key's release, or a change away from Free with no key held), it returns to beat 0 on
     that sample and rests there.
   - **A change of mode with a key held** keeps it running where it is. A preset that changes `seq_mode` is a change
     of mode.
   - **So a key from silence:**
     - in Key mode, step 1 (count 0) starts on the key's sample;
     - the room's ghost kick and duck fire on it too: beat 0 is a grid point, and the rumble fires on the first sample
       the clock runs.
   - **After the release** no step starts and no ghost kick or duck comes. Nothing is reset or cut:
     - the note rings out on its decay;
     - the room's tail rings out on `rumble_decay`;
     - the duck recovers on its 150 ms.
   - A key pressed again while the tails ring is a key from silence.
   - `meters().ppq` reads the own clock: 0 at rest.

3. **The strict validation's log, again** (`artifacts/reports/pluginval-strict.txt`)
   - Once task 1 is in, run `tools/pluginval.sh 10 artifacts/reports/pluginval-strict.txt` and commit the log it
     keeps, as phase 7's task 2 did. Never write or edit the log by hand.
   - pluginval's log is the same from run to run but for one line: the plugin's identifier (`Testing plugin:
     VST3-Metakosmia Undertow 2-<hash>-<hash>`), whose first hash comes from the bundle's path, so a run in another
     checkout changes that line. Commit what the script keeps either way. If nothing changes, the task changes nothing,
     and that is right: its checks pass on the unchanged tree.

## API

```cpp
// core/include/undertow/sequencer.h: one query is added to class Sequencer
bool anyKeyHeld() const;  // a key (in Notes mode, a note) is down
```

## Acceptance checks

Each check is one command, run from the repository root; the checks that share a command are one verify step.

| Check | Command | Proves |
|---|---|---|
| P7.1.01 | `tools/spec.sh 7_1 standalone` | With no host and no key, Key and Notes modes are exact silence for 4 s with the ghost kick and the room at full: no hit, the clock at beat 0 |
| P7.1.02 | `tools/spec.sh 7_1 standalone` | A key from silence starts the clock at beat 0 on its sample: step 1 at once on the key's note, and the ghost kick with it |
| P7.1.03 | `tools/spec.sh 7_1 standalone` | The last key's release stops the clock on its sample and returns it to beat 0: no step, ghost kick or duck after it, and the note and the room's tail ring out unchanged |
| P7.1.04 | `tools/spec.sh 7_1 standalone` | A key again while the tails ring is a key from silence: step 1, the ghost kick and the duck on its sample |
| P7.1.05 | `tools/spec.sh 7_1 standalone` | Notes mode: a held note runs the room's clock from beat 0 and its release stops it; one hit per note, no pattern |
| P7.1.06 | `tools/spec.sh 7_1 standalone` | Free mode with no host plays at once, as before, and a key never restarts its clock |
| P7.1.07 | `tools/spec.sh 7_1 standalone` | With no host a change of mode starts or stops the clock at beat 0; with a key held it keeps running |
| P7.1.08 | `tools/spec.sh 7_1 keys` | `Sequencer::anyKeyHeld()` is true while a key or a note is down, in every mode |
| P7.1.09 | `tools/spec.sh 1 engine` | The clock follows a host, and in Free mode runs on its own at seq_bpm (P1.03 to P1.07). A regression check: it passes on the base too |
| P7.1.10 | `tools/spec.sh 1 rumble` | The room pumps on a playing host's beats in Key mode with no key, stops with the host, and runs on the own clock in Free (P1.09). A regression check: it passes on the base too |
| P7.1.11 | `tools/spec.sh 4 engine` | The sequencer through the engine: Key, Free and Notes, the stopped host's audition (P4.12 to P4.18). A regression check: it passes on the base too |
| P7.1.12 | `tools/spec.sh 7 ship` | The strict validation's log, made again: strictness 10, every test passed on Metakosmia Undertow 2 2.0.0, no folder outside the checkout. A regression check: it passes on the base too |
| P7.1.13 | `tools/pluginval.sh 10` | pluginval passes at strictness 10 (without its GUI tests). A regression check: it passes on the base too |
| P7.1.14 | `tools/check.sh --plugin` | The whole local check still passes: phases 1 to 7.1, both presets, pluginval |

## Rules every task keeps

- The rules of phases 1 to 7 still hold: never edit `tests/spec/`, `third_party/`, `docs/` or the given tools; `core/`
  stays plain C++17; nothing the audio thread calls allocates, locks, does I/O or logs.
- The engine reads the held keys from the sequencer and never counts notes a second time.

## Also required, not checked automatically

- In the Standalone app, a new window:
  - is silent, with the hint "HOLD A KEY TO PLAY THE PATTERN" in Key mode;
  - plays from step 1 with the room when a key goes down;
  - lets the note and the room ring out when the key comes up.
  In Free mode it plays at once.

## Notes for the planner

- This spec is `docs/specs/phase-7.1-silent-until-played.md`. Put it first among the files every task reads: its "What
  to build" holds the clock's rule.
- Two tasks, in order. Each task's verify steps are exactly its checks' commands, unchanged:
  1. **The own clock.** Focused: about 20 lines.
     - Edits: `core/include/undertow/sequencer.h`, `core/src/sequencer.cpp`, `core/src/engine.cpp`.
     - Its files are those 3 paths; it creates and deletes nothing.
     - Checks: P7.1.01 to P7.1.07 (one step), P7.1.08, then the regression checks P7.1.09, P7.1.10 and P7.1.11
       (`expect = "pass_on_base"`).
  2. **The strict validation's log, again.** Needs task 1. Mechanical: it runs one script and commits the log it keeps.
     - Edits: `artifacts/reports/pluginval-strict.txt`.
     - Its files are that 1 path; it creates and deletes nothing.
     - Checks: the regression checks P7.1.12 and P7.1.13 (`expect = "pass_on_base"`).
     - Its log differs from phase 7's in one line at most, the plugin's identifier; if it comes out the same, the task
       changes nothing.
- `tools/check.sh --plugin` (P7.1.14) is the plan's final check, not a task's.
- Both tasks run commands: each agent needs `bash` and `edit_file`. Leave `allowed_tools` out, or include `bash`,
  `edit_file`, `read_file`, `write_file`, `grep` and `glob`.
