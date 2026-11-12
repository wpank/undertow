// SPDX-License-Identifier: AGPL-3.0-only
// Phase 7.1 spec tests: with no host, Key and Notes modes are silent until a key is held
// (docs/specs/phase-7.1-silent-until-played.md, checks P7.1.01 to P7.1.07).
// spec-needs: core/include/undertow/engine.h

#include "doctest.h"

#include "spec_analysis.h"
#include "spec_engine.h"
#include "undertow/engine.h"
#include "undertow/params.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace {

constexpr double kRate = 48000.0;
constexpr double kBpm = 130.0;  // seq_bpm's default
constexpr std::size_t kBlock = 64;
constexpr std::size_t kNone = static_cast<std::size_t>(-1);
using namespace undertow::param;
enum Mode { Key = 0, Free = 1, Notes = 2 };

// A render: the output, every hit at its sample from the render's start, and the meters after each block.
struct Run {
    spec::Signal out;
    std::vector<undertow::Hit> hits;
    std::vector<undertow::Meters> meters;
    std::vector<std::size_t> blockEnd;  // the sample after each block's last
};

Run play(undertow::Engine& engine, std::size_t samples, std::vector<undertow::MidiEvent> events = {},
         undertow::Transport transport = {}) {
    Run r;
    r.out.resize(samples);
    std::vector<float> right(samples);
    std::vector<undertow::MidiEvent> blockEvents;
    blockEvents.reserve(events.size());
    undertow::Hit hits[undertow::kMaxHits];
    std::size_t next = 0;
    for (std::size_t start = 0; start < samples; start += kBlock) {
        const auto n = std::min(kBlock, samples - start);
        blockEvents.clear();
        while (next < events.size() && static_cast<std::size_t>(events[next].sampleOffset) < start + n) {
            auto e = events[next++];
            e.sampleOffset -= static_cast<int>(start);
            blockEvents.push_back(e);
        }
        engine.process(r.out.data() + start, right.data() + start, static_cast<int>(n), blockEvents.data(),
                       static_cast<int>(blockEvents.size()), transport);
        const int count = engine.hits(hits, undertow::kMaxHits);
        for (int k = 0; k < count; ++k) {
            auto h = hits[k];
            h.sampleOffset += static_cast<int>(start);
            r.hits.push_back(h);
        }
        r.meters.push_back(engine.meters());
        r.blockEnd.push_back(start + n);
        if (transport.valid && transport.playing)
            transport.ppq += static_cast<double>(n) / kRate * transport.bpm / 60.0;
    }
    return r;
}

void prepare(undertow::Engine& engine, const spec::Params& p) {
    engine.prepare(kRate, 512);
    spec::apply(engine, p);
    engine.reset();
}

// The voice and the room loud: step 1 sounds, so its hit shows when it starts; the ghost kick, the room and the
// voice's send to it at full. Silence then means nothing at all plays.
spec::Params loud(Mode mode) {
    auto p = spec::defaults();
    spec::set(p, SeqMode, static_cast<float>(mode));
    spec::set(p, Step1Vel, 0.8f);
    spec::set(p, RumbleLevel, 1);
    spec::set(p, RumbleKick, 1);
    spec::set(p, RumbleSend, 1);
    return p;
}

// The room alone: the ghost kick and the room at full, the voice's sources off.
spec::Params room(Mode mode) {
    auto p = loud(mode);
    for (const int source : {Vco1Level, Vco2Level, NoiseLevel, SubLevel})
        spec::set(p, source, 0);
    return p;
}

// The voice alone: the room off.
spec::Params voice(Mode mode) {
    auto p = loud(mode);
    spec::set(p, RumbleLevel, 0);
    spec::set(p, RumbleKick, 0);
    spec::set(p, RumbleSend, 0);
    return p;
}

std::size_t firstSound(const spec::Signal& x, std::size_t from = 0) {
    for (std::size_t n = from; n < x.size(); ++n)
        if (x[n] != 0.0f)
            return n;
    return kNone;
}

// The first sample from `from` on where two renders differ.
std::size_t firstDifference(const spec::Signal& a, const spec::Signal& b, std::size_t from = 0) {
    for (std::size_t n = from; n < std::min(a.size(), b.size()); ++n)
        if (a[n] != b[n])
            return n;
    return kNone;
}

// How long the room takes from a ghost kick to its first sound: Free mode starts the clock, and so the kick, at
// sample 0 after reset().
std::size_t roomLatency() {
    undertow::Engine e;
    prepare(e, room(Free));
    const auto heard = firstSound(play(e, spec::ms(400)).out);
    REQUIRE_MESSAGE(heard != kNone, "in Free mode with no host the room never sounded: Free runs the own clock from "
                                    "beat 0 after reset(), and its ghost kick fires there");
    return heard;
}

// The own clock's beat after `samples` samples of running: one beat is 60 / seq_bpm seconds.
double beatsIn(std::size_t samples) { return static_cast<double>(samples) * kBpm / (60.0 * kRate); }

// Every block that ends after `from`: its meters' beat must be the clock's beat at that block's end, counted from
// `from` (or 0 while it rests). Returns how many blocks were off, and the first of them in `what`.
int clockOff(const Run& r, std::size_t from, std::size_t to, bool running, std::string& what) {
    int off = 0;
    for (std::size_t b = 0; b < r.blockEnd.size(); ++b) {
        if (r.blockEnd[b] <= from || r.blockEnd[b] > to)
            continue;
        const double want = running ? beatsIn(r.blockEnd[b] - from) : 0.0;
        if (std::fabs(r.meters[b].ppq - want) > 1e-9) {
            if (off++ == 0)
                what = "after sample " + std::to_string(r.blockEnd[b]) + " meters().ppq is " +
                       std::to_string(r.meters[b].ppq) + "; want " + std::to_string(want);
        }
    }
    return off;
}

std::vector<undertow::Hit> hitsFrom(const Run& r, std::size_t from) {
    std::vector<undertow::Hit> out;
    for (const auto& h : r.hits)
        if (static_cast<std::size_t>(h.sampleOffset) >= from)
            out.push_back(h);
    return out;
}

} // namespace

TEST_CASE("P7.1.01 with no host and no key, Key and Notes modes are exact silence, the ghost kick and room at full") {
    for (const Mode mode : {Key, Notes}) {
        INFO(std::string(mode == Key ? "Key mode" : "Notes mode"));
        undertow::Engine e;
        prepare(e, loud(mode));
        const auto r = play(e, static_cast<std::size_t>(4 * kRate));
        const auto heard = firstSound(r.out);
        CHECK_MESSAGE(heard == kNone, "with no host and no key the output sounds from sample "
                                          << heard
                                          << ": the own clock must rest, so no step plays and no ghost kick fires "
                                             "(it runs only while a key is held, or in Free mode)");
        CHECK_MESSAGE(r.hits.empty(), r.hits.size() << " hits with no key held");
        const auto& m = r.meters.back();
        CHECK_MESSAGE(m.ppq == 0.0, "at rest the own clock reads beat 0; meters().ppq is " << m.ppq);
        CHECK_MESSAGE(!m.hostClock, "with no host, meters().hostClock is false");
        CHECK_MESSAGE(m.step == -1, "at rest no step plays; meters().step is " << m.step);
        CHECK_MESSAGE(m.rumble == 0.0f, "at rest the room is silent; meters().rumble is " << m.rumble);
    }
}

TEST_CASE("P7.1.02 a key from silence starts the own clock at beat 0 on its sample: step 1 at once, the ghost kick "
          "with it") {
    const std::size_t at = 24007;  // mid-block, mid-beat of a clock that would have run since reset
    {
        undertow::Engine e;
        prepare(e, loud(Key));
        const auto r = play(e, at + static_cast<std::size_t>(kRate), {spec::noteOn(static_cast<int>(at), 43)});
        CHECK_MESSAGE(firstSound(r.out) >= at, "the output sounds at sample " << firstSound(r.out)
                                                                              << ", before the key at " << at);
        REQUIRE_MESSAGE(!r.hits.empty(), "the key at sample " << at << " played nothing");
        const auto& first = r.hits.front();
        CHECK_MESSAGE(first.sampleOffset == static_cast<int>(at),
                      "the first hit comes at sample " << first.sampleOffset << "; a key from silence plays step 1 on "
                                                       << "its own sample, " << at);
        CHECK_MESSAGE(first.step == 0, "the key's first hit is step " << first.step + 1 << "; it is step 1 (count 0)");
        CHECK_MESSAGE(first.note == 43, "step 1 plays the key's note, 43; it played " << first.note);
        if (r.hits.size() > 1) {
            const double due = static_cast<double>(at) + 0.25 * 60.0 / kBpm * kRate;  // one 1/16 later
            CHECK_MESSAGE(std::fabs(r.hits[1].sampleOffset - due) <= 1.0,
                          "step 2 comes at sample " << r.hits[1].sampleOffset << "; a 1/16 at 130 BPM from the key is "
                                                    << due);
        }
        std::string what;
        CHECK_MESSAGE(clockOff(r, 0, at, false, what) == 0, "before the key the clock rests at beat 0: " << what);
        CHECK_MESSAGE(clockOff(r, at, r.out.size(), true, what) == 0,
                      "from the key the own clock counts from beat 0 at seq_bpm: " << what);
    }
    // The ghost kick lands with the key: the room's first sound comes as long after the key as it comes after a
    // ghost kick at sample 0.
    undertow::Engine e;
    prepare(e, room(Key));
    const auto r = play(e, at + spec::ms(400), {spec::noteOn(static_cast<int>(at), 43)});
    const auto heard = firstSound(r.out);
    REQUIRE_MESSAGE(heard != kNone, "after a key from silence the room stays silent: the ghost kick fires on the "
                                    "key's sample, beat 0 of the clock it starts");
    const auto latency = roomLatency();
    const double after = static_cast<double>(heard) - static_cast<double>(at);
    CHECK_MESSAGE(std::fabs(after - static_cast<double>(latency)) <= static_cast<double>(spec::ms(2)),
                  "the room sounds " << after << " samples after the key; a ghost kick reaches the output " << latency
                                     << " samples after it fires: fire it on the key's sample");
}

TEST_CASE("P7.1.03 releasing the last key stops the clock on its sample and returns it to beat 0; the note and the "
          "room ring out") {
    const std::size_t press = 4801, release = press + 30000;  // mid-step: step 6 began at press + 27692
    const std::size_t total = release + static_cast<std::size_t>(2.5 * kRate);
    // The voice: released, and held throughout. Until the next step was due they are the same: nothing is cut.
    {
        undertow::Engine held, released;
        prepare(held, voice(Key));
        prepare(released, voice(Key));
        const auto h = play(held, total, {spec::noteOn(static_cast<int>(press), 43)});
        const auto r = play(released, total,
                            {spec::noteOn(static_cast<int>(press), 43), spec::noteOff(static_cast<int>(release), 43)});
        const double nextStep = static_cast<double>(press) + 6 * 0.25 * 60.0 / kBpm * kRate;
        const auto differ = firstDifference(h.out, r.out);
        CHECK_MESSAGE(static_cast<double>(differ) >= nextStep - 1,
                      "the voice changed at sample " << differ << ", after the release at " << release
                                                     << ": the playing note rings out on its decay, unchanged, and "
                                                        "only the next step (due at "
                                                     << nextStep << ") no longer comes");
        CHECK_MESSAGE(hitsFrom(r, release).empty(), hitsFrom(r, release).size()
                                                        << " hits after the release: once the last key is up no step "
                                                           "starts");
        CHECK_MESSAGE(spec::rms(r.out, release, release + spec::ms(20)) > 0.0,
                      "the note was cut at the release; it rings out on its decay");
        std::string what;
        CHECK_MESSAGE(clockOff(r, press, release, true, what) == 0, "while the key is held: " << what);
        CHECK_MESSAGE(clockOff(r, release, total, false, what) == 0,
                      "from the release the clock rests at beat 0: " << what);
    }
    // The room: its tail rings out untouched, and no ghost kick or duck comes after the release.
    {
        undertow::Engine held, released;
        prepare(held, room(Key));
        prepare(released, room(Key));
        const auto h = play(held, total, {spec::noteOn(static_cast<int>(press), 43)});
        const auto r = play(released, total,
                            {spec::noteOn(static_cast<int>(press), 43), spec::noteOff(static_cast<int>(release), 43)});
        // Beat 2 is the first grid point after the release. Held, its duck dips the ringing tail on that sample;
        // released, nothing happens there. So the two rooms are the same up to it, and part on it.
        const double nextKick = static_cast<double>(press) + 2.0 * 60.0 / kBpm * kRate;
        const auto differ = firstDifference(h.out, r.out);
        CHECK_MESSAGE(static_cast<double>(differ) >= nextKick - 1,
                      "the room changed at sample " << differ << ", before the next grid point (" << nextKick
                                                    << "): the release cuts nothing and fires nothing; the tail "
                                                       "rings out on rumble_decay and the duck recovers");
        CHECK_MESSAGE(static_cast<double>(differ) <= nextKick + 2,
                      "the released room is the held room's until sample "
                          << differ << ", past the next grid point (" << nextKick
                          << "): after the release no ghost kick or duck may come, so it must part from the held room "
                             "on that grid point");
        const auto tailFrom = static_cast<std::size_t>(nextKick) + spec::ms(100);
        CHECK_MESSAGE(spec::rms(r.out, tailFrom, tailFrom + spec::ms(500)) > 0.0, "the room's tail was cut");
    }
}

TEST_CASE("P7.1.04 a key again, even while the tails ring, is a key from silence: step 1, the ghost kick and the duck "
          "on its sample") {
    const std::size_t press = 4800, release = 30000, again = 40011;
    const std::size_t total = again + static_cast<std::size_t>(kRate);
    undertow::Engine e;
    prepare(e, loud(Key));
    const auto r = play(e, total,
                        {spec::noteOn(static_cast<int>(press), 43), spec::noteOff(static_cast<int>(release), 43),
                         spec::noteOn(static_cast<int>(again), 50)});
    const auto after = hitsFrom(r, release);
    REQUIRE_MESSAGE(!after.empty(), "the second key played nothing");
    CHECK_MESSAGE(after.front().sampleOffset == static_cast<int>(again),
                  "the second key's first hit comes at " << after.front().sampleOffset << "; it is due on its sample, "
                                                         << again);
    CHECK_MESSAGE(after.front().step == 0, "the second key starts at step " << after.front().step + 1
                                                                            << "; a key from silence starts step 1");
    CHECK_MESSAGE(after.front().note == 50, "step 1 plays the new key's note, 50; it played " << after.front().note);
    std::string what;
    CHECK_MESSAGE(clockOff(r, again, total, true, what) == 0,
                  "from the second key the clock counts from beat 0 again: " << what);
    // Its grid point: the room with the second key parts from the room without it on the key's sample, where the
    // duck, which fires with the ghost kick, dips the tail that still rings.
    undertow::Engine with, without;
    prepare(with, room(Key));
    prepare(without, room(Key));
    const auto a = play(with, total,
                        {spec::noteOn(static_cast<int>(press), 43), spec::noteOff(static_cast<int>(release), 43),
                         spec::noteOn(static_cast<int>(again), 50)});
    const auto b = play(without, total,
                        {spec::noteOn(static_cast<int>(press), 43), spec::noteOff(static_cast<int>(release), 43)});
    const auto differ = firstDifference(a.out, b.out);
    REQUIRE_MESSAGE(differ != kNone, "the second key left the room as it was: its ghost kick and duck never came");
    CHECK_MESSAGE(differ == again, "the room parts from the room without the second key at sample "
                                       << differ << "; the key's duck and ghost kick fire on its own sample, "
                                       << again);
}

TEST_CASE("P7.1.05 Notes mode: a held note runs the room's clock from beat 0, its release stops it, and no pattern "
          "plays") {
    const std::size_t press = 9601, release = press + 30000;
    const std::size_t total = release + static_cast<std::size_t>(kRate);
    undertow::Engine e;
    prepare(e, loud(Notes));
    const auto r = play(e, total,
                        {spec::noteOn(static_cast<int>(press), 40), spec::noteOff(static_cast<int>(release), 40)});
    REQUIRE_MESSAGE(r.hits.size() == 1, "Notes mode played " << r.hits.size() << " hits for one note; each note is "
                                                             << "one hit, and no pattern plays");
    CHECK(r.hits.front().sampleOffset == static_cast<int>(press));
    CHECK(r.hits.front().step == -1);
    CHECK(r.hits.front().note == 40);
    CHECK_MESSAGE(firstSound(r.out) >= press, "the output sounds before the note, at " << firstSound(r.out));
    std::string what;
    CHECK_MESSAGE(clockOff(r, 0, press, false, what) == 0, "before the note the clock rests: " << what);
    CHECK_MESSAGE(clockOff(r, press, release, true, what) == 0, "while the note is held the clock runs: " << what);
    CHECK_MESSAGE(clockOff(r, release, total, false, what) == 0, "from the note's release it rests: " << what);
    undertow::Engine roomOnly;
    prepare(roomOnly, room(Notes));
    const auto q = play(roomOnly, press + spec::ms(400), {spec::noteOn(static_cast<int>(press), 40)});
    const auto heard = firstSound(q.out);
    REQUIRE_MESSAGE(heard != kNone, "a held note in Notes mode left the room silent: it starts the room's clock");
    const double after = static_cast<double>(heard) - static_cast<double>(press);
    CHECK_MESSAGE(std::fabs(after - static_cast<double>(roomLatency())) <= static_cast<double>(spec::ms(2)),
                  "the room sounds " << after << " samples after the note; its ghost kick fires on the note's sample");
}

TEST_CASE("P7.1.06 Free mode with no host plays at once, as before, and a key never restarts its clock") {
    const std::size_t press = 30011;
    const std::size_t total = static_cast<std::size_t>(2 * kRate);
    undertow::Engine e;
    prepare(e, loud(Free));
    const auto r = play(e, total, {spec::noteOn(static_cast<int>(press), 50), spec::noteOff(40000, 50)});
    REQUIRE_MESSAGE(!r.hits.empty(), "Free mode with no host played nothing");
    CHECK_MESSAGE(r.hits.front().sampleOffset == 0, "Free mode's first hit comes at " << r.hits.front().sampleOffset
                                                                                       << "; it plays at once, on the "
                                                                                          "first sample");
    CHECK(r.hits.front().step == 0);
    CHECK(r.hits.front().note == 43);  // seq_root
    std::string what;
    CHECK_MESSAGE(clockOff(r, 0, total, true, what) == 0,
                  "in Free mode the own clock runs from 0 after reset() whatever the keys do: " << what);
    CHECK_MESSAGE(firstSound(r.out) != kNone, "Free mode with no host must sound");
}

TEST_CASE("P7.1.07 with no host a change of mode starts or stops the clock; with a key held it keeps running") {
    const std::size_t part = 24000;
    // Key to Free with no key: the clock starts at beat 0, Free's step 1 at once.
    {
        undertow::Engine e;
        prepare(e, loud(Key));
        const auto quiet = play(e, part);
        CHECK(firstSound(quiet.out) == kNone);
        e.setParam(SeqMode, Free);
        const auto r = play(e, part);
        REQUIRE_MESSAGE(!r.hits.empty(), "after Key to Free, Free plays nothing");
        CHECK_MESSAGE((r.hits.front().sampleOffset == 0 && r.hits.front().step == 0),
                      "after Key to Free the pattern starts at once at step 1; its first hit is step "
                          << r.hits.front().step + 1 << " at sample " << r.hits.front().sampleOffset);
        std::string what;
        CHECK_MESSAGE(clockOff(r, 0, part, true, what) == 0, "Free starts the clock at beat 0: " << what);
    }
    // Free to Key with no key: the clock stops and rests at beat 0, the tails ring out.
    {
        undertow::Engine e;
        prepare(e, loud(Free));
        (void)play(e, part);
        e.setParam(SeqMode, Key);
        const auto r = play(e, part);
        CHECK_MESSAGE(r.hits.empty(), "after Free to Key with no key " << r.hits.size() << " hits played");
        std::string what;
        CHECK_MESSAGE(clockOff(r, 0, part, false, what) == 0, "after Free to Key the clock rests: " << what);
        CHECK_MESSAGE(spec::rms(r.out, 0, spec::ms(100)) > 0.0, "Free to Key cut the sound; the tails ring out");
    }
    // A key held: Key to Free keeps the clock running where it is.
    {
        undertow::Engine e;
        prepare(e, loud(Key));
        (void)play(e, part, {spec::noteOn(100, 43)});
        e.setParam(SeqMode, Free);
        const auto r = play(e, part);
        const double want = beatsIn(2 * part - 100);
        CHECK_MESSAGE(std::fabs(r.meters.back().ppq - want) <= 1e-9,
                      "with a key held a change of mode keeps the clock running: it reads beat "
                          << r.meters.back().ppq << "; want " << want);
    }
}
