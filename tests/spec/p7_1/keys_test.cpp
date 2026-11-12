// SPDX-License-Identifier: AGPL-3.0-only
// Phase 7.1 spec tests: the sequencer tells whether a key is down (docs/specs/phase-7.1-silent-until-played.md,
// check P7.1.08).
// spec-needs: core/include/undertow/sequencer.h

#include "doctest.h"

#include "spec_engine.h"
#include "undertow/sequencer.h"

TEST_CASE("P7.1.08 Sequencer::anyKeyHeld() is true while a key or a note is down, in every mode") {
    for (const float mode : {0.0f, 1.0f, 2.0f}) {
        INFO("seq_mode " << mode);
        undertow::Sequencer s;
        s.prepare(48000.0);
        auto p = spec::defaults();
        spec::set(p, undertow::param::SeqMode, mode);
        s.setParams(p.data());
        s.reset();
        CHECK_MESSAGE(!s.anyKeyHeld(), "after reset() no key is held");
        s.keyOn(45);
        CHECK_MESSAGE(s.anyKeyHeld(), "a key went down");
        s.keyOn(47);
        s.keyOff(45);
        CHECK_MESSAGE(s.anyKeyHeld(), "one key came up, one is still down");
        s.keyOff(47);
        CHECK_MESSAGE(!s.anyKeyHeld(), "the last key came up");
        s.keyOff(47);
        CHECK_MESSAGE(!s.anyKeyHeld(), "a second key-up of the same key changes nothing");
        s.keyOn(40);
        s.keyOn(40);
        s.keyOff(40);
        CHECK_MESSAGE(!s.anyKeyHeld(), "a key pressed twice is one key: one key-up lifts it");
        s.keyOn(40);
        s.allKeysOff();
        CHECK_MESSAGE(!s.anyKeyHeld(), "allKeysOff() lifts every key");
        s.keyOn(52);
        s.reset();
        CHECK_MESSAGE(!s.anyKeyHeld(), "reset() lifts every key");
    }
}
