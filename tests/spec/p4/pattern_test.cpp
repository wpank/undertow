// SPDX-License-Identifier: AGPL-3.0-only
// Phase 4 spec tests: the pattern and its drift (docs/specs/phase-4-sequencer.md, checks P4.01 to P4.05).
// spec-needs: core/include/undertow/pattern.h

#include "doctest.h"

#include "spec_alloc.h"
#include "spec_engine.h"
#include "undertow/pattern.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <utility>
#include <vector>

namespace {

using namespace undertow::param;

constexpr int kG2 = 43; // the default root

struct Home {
    int pitch[8];
    float velocity[8];
};

// The default steps, and patterns that reach the ends of the pitch range, so the drift has to fold.
const Home kDefault{{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0.22f, 1, 0.55f, 0, 0.25f, 0.9f, 0.6f}};
const Home kWide{{12, -12, 6, -8, 5, -7, 0, 9}, {1, 0.8f, 0.6f, 0.4f, 0.3f, 0.7f, 0.5f, 0.9f}};
const Home kRests{{3, -3, 0, 7, 0, -5, 12, -12}, {0, 0, 0, 0, 0, 0, 0, 0}};
const Home kMixed{{-2, 0, 10, 0, -10, 2, 0, 7}, {0.5f, 0, 1, 0.75f, 0, 0.125f, 0.35f, 0}};

// `subOctave` is sub_octave's choice: 1 (-1 oct, the default) or 0.
spec::Params params(const Home& h, int length, float drift, int subOctave = 1) {
    auto p = spec::defaults();
    for (int i = 0; i < undertow::kSteps; ++i) {
        spec::set(p, Step1Pitch + i, static_cast<float>(h.pitch[i]));
        spec::set(p, Step1Vel + i, h.velocity[i]);
    }
    spec::set(p, SeqLength, static_cast<float>(length));
    spec::set(p, SeqDrift, drift);
    spec::set(p, SubOctave, static_cast<float>(subOctave));
    return p;
}

undertow::Pattern make(const Home& h, int length, float drift, int subOctave = 1) {
    undertow::Pattern pattern;
    const auto p = params(h, length, drift, subOctave);
    pattern.setParams(p.data());
    return pattern;
}

// The spec's recipe, written out on its own: docs/specs/phase-4-sequencer.md, "The pattern and its drift".
std::uint32_t hash32(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

long long floorDiv(long long a, long long b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

int changedOnPass(int sounding, float drift, int j) {
    const double x = 3.0 * static_cast<double>(drift) * std::sin(3.14159265358979323846 * j / 16.0);
    return std::min(sounding, static_cast<int>(std::floor(x + 0.5)));
}

// The sub's frequency for VCO 1 on MIDI note `note`.
double subHz(int note, int subOctave) {
    return 440.0 * std::pow(2.0, (note - 69) / 12.0) / (subOctave == 1 ? 2.0 : 1.0);
}

undertow::Step recipe(const Home& h, int length, float drift, long long count, int root, int subOctave) {
    const long long pass = floorDiv(count, length);
    const int s = static_cast<int>(count - pass * length);
    undertow::Step out{h.pitch[s], h.velocity[s]};
    if (h.velocity[s] == 0.0f)
        return out; // rests stay rests
    const long long arc = floorDiv(pass, 16);
    const int j = static_cast<int>(pass - 16 * arc);
    const std::uint32_t a = static_cast<std::uint32_t>(arc);
    std::vector<std::pair<std::uint32_t, int>> order; // the arc's sounding steps
    for (int t = 0; t < length; ++t)
        if (h.velocity[t] > 0.0f)
            order.emplace_back(hash32(a * 8u + static_cast<std::uint32_t>(t)), t);
    std::sort(order.begin(), order.end());
    const int n = changedOnPass(static_cast<int>(order.size()), drift, j);
    bool chosen = false;
    for (int r = 0; r < n; ++r)
        chosen = chosen || order[static_cast<std::size_t>(r)].second == s;
    if (!chosen)
        return out;
    const int p = h.pitch[s];
    const auto kind = hash32((a * 8u + static_cast<std::uint32_t>(s)) ^ 0x9e3779b9u) % 4u;
    if (kind == 3) {
        out.velocity = h.velocity[s] * 0.5f;
        return out;
    }
    const int way = kind == 0 ? 12 : kind == 1 ? 7 : -5;
    const int otherWay = kind == 0 ? -12 : kind == 1 ? -5 : 7;
    auto ok = [&](int pitch) { return pitch >= -12 && pitch <= 12 && subHz(root + pitch, subOctave) >= 41.0; };
    if (ok(p + way))
        out.pitch = p + way;
    else if (ok(p + otherWay))
        out.pitch = p + otherWay;
    return out;
}

bool same(const undertow::Step& a, const undertow::Step& b) { return a.pitch == b.pitch && a.velocity == b.velocity; }

int position(long long count, int length) { return static_cast<int>(count - floorDiv(count, length) * length); }

} // namespace

TEST_CASE("P4.01 at drift 0 every count plays its home step, count mod length, on both sides of 0") {
    for (const Home* h : {&kDefault, &kWide, &kMixed})
        for (int length = 1; length <= 8; ++length) {
            const auto pattern = make(*h, length, 0.0f);
            INFO("length " << length);
            REQUIRE_MESSAGE(pattern.length() == length, "length() must be seq_length, " << length << "; got "
                                                                                        << pattern.length());
            for (int i = 0; i < undertow::kSteps; ++i) {
                const auto home = pattern.home(i);
                CHECK_MESSAGE((home.pitch == h->pitch[i] && home.velocity == h->velocity[i]),
                              "home(" << i << ") must be step " << i + 1 << "'s pitch and velocity as set, "
                                      << h->pitch[i] << " and " << h->velocity[i] << "; got " << home.pitch << " and "
                                      << home.velocity);
            }
            int wrong = 0;
            long long first = 0;
            for (long long count = -2000; count <= 20000; ++count) {
                const int s = position(count, length);
                if (!same(pattern.at(count, kG2), undertow::Step{h->pitch[s], h->velocity[s]}) && wrong++ == 0)
                    first = count;
            }
            CHECK_MESSAGE(wrong == 0, wrong << " counts did not play their home step at drift 0, the first count "
                                            << first << ": count c plays step c mod length, also for c below 0 "
                                                        "(count -1 is the last step)");
        }
}

TEST_CASE("P4.02 the drift follows the spec's recipe exactly, for every pattern, length, drift, root and count") {
    std::uint32_t mismatches = 0;
    // Roots high and low, and the sub at -1 oct or with VCO 1, so the guard at E1 (41.2 Hz) turns moves round.
    const std::pair<int, int> roots[] = {{kG2, 1}, {36, 1}, {31, 1}, {28, 0}};
    for (const auto& [root, subOctave] : roots)
        for (const Home* h : {&kDefault, &kWide, &kRests, &kMixed})
            for (int length = 1; length <= 8; ++length)
                for (const float drift : {0.0f, 0.2f, 0.45f, 0.7f, 0.85f, 1.0f}) {
                    const auto pattern = make(*h, length, drift, subOctave);
                    for (long long count = -3LL * 16 * length; count < 29LL * 16 * length; ++count) {
                        const auto want = recipe(*h, length, drift, count, root, subOctave);
                        const auto got = pattern.at(count, root);
                        if (!same(want, got) && mismatches++ < 6)
                            FAIL_CHECK("root " << root << " (sub_octave " << subOctave << "), length " << length
                                               << ", drift " << drift << ", count " << count << " (step "
                                               << position(count, length) + 1 << "): the recipe plays pitch "
                                               << want.pitch << " at velocity " << want.velocity << "; at() gave "
                                               << got.pitch << " at " << got.velocity);
                    }
                }
    CHECK_MESSAGE(mismatches == 0, mismatches << " counts differ from the recipe in docs/specs/phase-4-sequencer.md");
}

TEST_CASE("P4.03 the drift leaves home and comes back within each arc of 16 passes, one step at a time") {
    // The default steps at the default drift: two of the six sounding steps through the middle of every arc, and the
    // two rests never.
    const int want[16] = {0, 0, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 0};
    const auto pattern = make(kDefault, 8, 0.7f);
    for (long long arc = -2; arc < 62; ++arc) {
        std::set<int> before;
        undertow::Step change[8];
        bool seen[8] = {};
        for (int j = 0; j < 16; ++j) {
            std::set<int> now;
            for (int s = 0; s < 8; ++s) {
                const auto got = pattern.at((arc * 16 + j) * 8 + s, kG2);
                if (same(got, pattern.home(s)))
                    continue;
                now.insert(s);
                if (!seen[s]) {
                    seen[s] = true;
                    change[s] = got;
                } else
                    CHECK_MESSAGE(same(got, change[s]), "arc " << arc << ": step " << s + 1
                                                               << " changed one way and then another: a step keeps "
                                                                  "its change through the arc");
            }
            INFO("arc " << arc << ", pass " << j);
            CHECK_MESSAGE((now.count(0) == 0 && now.count(4) == 0), "steps 1 and 5 are rests: rests stay rests");
            CHECK_MESSAGE(static_cast<int>(now.size()) == want[j],
                          now.size() << " steps differ from home; at drift 0.7, pass " << j << " of an arc changes "
                                     << want[j] << " of the sounding steps (round(3 x drift x sin(pi x pass / 16)))");
            const bool nested = std::includes(now.begin(), now.end(), before.begin(), before.end()) ||
                                std::includes(before.begin(), before.end(), now.begin(), now.end());
            CHECK_MESSAGE(nested, "the changed steps must grow and shrink one at a time: a step joins as the count "
                                  "rises and leaves as it falls, so a pass's changed steps hold the previous pass's, "
                                  "or the other way round");
            before = now;
        }
    }
    // Any drift: home at the start of every arc, and never more than round(3 x drift) steps away.
    for (int hundredth = 0; hundredth <= 100; hundredth += 5) {
        const float drift = static_cast<float>(hundredth) / 100.0f;
        const int most = static_cast<int>(std::floor(3.0 * drift + 0.5));
        const auto p = make(kMixed, 8, drift);
        int over = 0, away = 0;
        for (long long count = -256; count < 64 * 128; ++count) {
            const int s = position(count, 8);
            const long long pass = floorDiv(count, 8);
            int differ = 0;
            if (s == 0)
                for (int t = 0; t < 8; ++t)
                    differ += same(p.at(count + t, kG2), p.home(t)) ? 0 : 1;
            over += differ > most ? 1 : 0;
            away += (pass - floorDiv(pass, 16) * 16 == 0 && !same(p.at(count, kG2), p.home(s))) ? 1 : 0;
        }
        INFO("drift " << drift);
        CHECK_MESSAGE(over == 0, over << " passes had more than " << most << " steps away from home");
        CHECK_MESSAGE(away == 0, away << " steps were away from home on the first pass of an arc; every arc starts "
                                         "at home");
    }
}

TEST_CASE("P4.04 a changed step goes up an octave, up a fifth or down a fourth, or plays as a ghost; rests stay "
          "rests; no move takes the sub under E1, 41.2 Hz") {
    for (const Home* h : {&kWide, &kMixed}) {
        const int length = 8;
        const auto pattern = make(*h, length, 1.0f);
        int octaves = 0, fifthsUp = 0, fourthsDown = 0, ghosts = 0, other = 0, outside = 0, low = 0, rests = 0;
        bool stepChanged[8] = {};
        std::set<std::vector<int>> arcs;
        for (long long arc = 0; arc < 256; ++arc) {
            std::vector<int> signature;
            for (int s = 0; s < length; ++s) {
                const auto home = pattern.home(s);
                const auto got = pattern.at((arc * 16 + 8) * length + s, kG2); // the middle of the arc
                if (got.pitch < -12 || got.pitch > 12)
                    ++outside;
                if (same(got, home))
                    continue;
                stepChanged[s] = true;
                if (home.velocity == 0.0f) {
                    ++rests;
                    continue;
                }
                const int moved = got.pitch - home.pitch;
                int kind = -1;
                if (got.velocity == home.velocity && (moved == 12 || moved == -12))
                    kind = 0;
                else if (got.velocity == home.velocity && moved == 7)
                    kind = 1;
                else if (got.velocity == home.velocity && moved == -5)
                    kind = 2;
                else if (moved == 0 && got.velocity == home.velocity * 0.5f)
                    kind = 3;
                if (kind < 0) {
                    if (other++ < 4)
                        FAIL_CHECK("arc " << arc << ", step " << s + 1 << ": home " << home.pitch << " at "
                                          << home.velocity << " became " << got.pitch << " at " << got.velocity
                                          << ", which is none of the drift's changes");
                    continue;
                }
                if (moved != 0 && subHz(kG2 + got.pitch, 1) < 41.0)
                    ++low;
                octaves += kind == 0 ? 1 : 0;
                fifthsUp += kind == 1 ? 1 : 0;
                fourthsDown += kind == 2 ? 1 : 0;
                ghosts += kind == 3 ? 1 : 0;
                signature.push_back(s * 8 + kind);
            }
            arcs.insert(signature);
        }
        INFO((h == &kWide ? "steps without rests" : "steps with rests"));
        CHECK_MESSAGE(other == 0, other << " changes were not an octave, a fifth up, a fourth down or a ghost");
        CHECK_MESSAGE(outside == 0, outside << " steps left the pitch range -12 to +12: a move that would leave it "
                                               "goes the other way to the same note");
        CHECK_MESSAGE(rests == 0, rests << " rests changed: only sounding steps drift");
        CHECK_MESSAGE(low == 0, low << " moves took the sub under E1 (41.2 Hz) on root G2: such a move goes the other "
                                       "way");
        CHECK_MESSAGE((octaves > 0 && fifthsUp > 0 && fourthsDown > 0 && ghosts > 0),
                      "over 256 arcs every change must occur: octaves " << octaves << ", fifths up " << fifthsUp
                                                                        << ", fourths down " << fourthsDown
                                                                        << ", ghosts " << ghosts);
        for (int s = 0; s < length; ++s)
            if (pattern.home(s).velocity > 0.0f)
                CHECK_MESSAGE(stepChanged[s], "over 256 arcs step " << s + 1
                                                                    << " never changed: each arc picks its own steps");
        CHECK_MESSAGE(arcs.size() >= 100, "256 arcs made only " << arcs.size()
                                                                << " different sets of changes; each arc picks its "
                                                                   "own steps and changes");
    }
    // A pattern of rests never drifts.
    const auto rests = make(kRests, 8, 1.0f);
    int moved = 0;
    for (long long count = -64; count < 64 * 128; ++count)
        moved += same(rests.at(count, kG2), rests.home(position(count, 8))) ? 0 : 1;
    CHECK_MESSAGE(moved == 0, "at drift 1 " << moved << " counts of a pattern of rests drifted: rests stay rests");

    // The guard: on root D2 (38) the sub sits at D1 (36.7 Hz) at home, so a fourth down (to A0, 27.5 Hz) must go up
    // a fifth instead. With the sub at VCO 1's pitch (sub_octave 0) the fourth down, to 55 Hz, is allowed.
    for (const int subOctave : {1, 0}) {
        const auto p = make(kDefault, 8, 1.0f, subOctave);
        int down = 0, up = 0;
        for (long long count = 0; count < 256 * 128; ++count) {
            const auto got = p.at(count, 38);
            const int moved = got.pitch - p.home(position(count, 8)).pitch;
            down += moved == -5 ? 1 : 0;
            up += moved == 7 ? 1 : 0;
        }
        INFO("sub_octave " << subOctave << " on root D2");
        if (subOctave == 1)
            CHECK_MESSAGE((down == 0 && up > 0), "with the sub an octave down, a fourth down from D2 would put it at "
                                                 "27.5 Hz: it must go up a fifth instead; fourths down "
                                                     << down << ", fifths up " << up);
        else
            CHECK_MESSAGE(down > 0, "with the sub at VCO 1's pitch a fourth down from D2 keeps it at 55 Hz: the "
                                    "fourth down must still occur");
    }
}

TEST_CASE("P4.05 the pattern is a pure function: the same parameters, root and count always play the same, without "
          "allocating") {
    const auto a = make(kMixed, 7, 0.85f), b = make(kMixed, 7, 0.85f);
    std::uint32_t state = 9;
    auto next = [&]() {
        state = state * 1664525u + 1013904223u;
        return state;
    };
    std::vector<long long> counts(5000);
    for (auto& c : counts)
        c = static_cast<long long>(next() % 400000u) - 200000;
    std::vector<undertow::Step> forward(counts.size()), backward(counts.size());
    undertow::Pattern c;
    const auto p = params(kMixed, 7, 0.85f);
    spec::AllocationCounter counter;
    c.setParams(p.data());
    for (std::size_t i = 0; i < counts.size(); ++i)
        forward[i] = a.at(counts[i], kG2);
    for (std::size_t i = counts.size(); i-- > 0;)
        backward[i] = b.at(counts[i], kG2);
    bool cSame = true;
    for (std::size_t i = 0; i < counts.size(); ++i)
        cSame = cSame && same(c.at(counts[i], kG2), forward[i]);
    const std::size_t allocations = counter.count();
    int differ = 0;
    for (std::size_t i = 0; i < counts.size(); ++i)
        differ += same(forward[i], backward[i]) ? 0 : 1;
    CHECK_MESSAGE(differ == 0, differ << " counts played differently when asked in another order: at() depends only "
                                         "on the parameters, the root and the count");
    CHECK_MESSAGE(cSame, "a new Pattern given the same parameters must play the same steps");
    CHECK_MESSAGE(allocations == 0, allocations << " heap allocations in setParams() and at(): they run on the audio "
                                                   "thread");
    CHECK_MESSAGE(same(a.at(123456789LL, kG2), b.at(123456789LL, kG2)), "the same count must play the same in two "
                                                                        "patterns");
}
