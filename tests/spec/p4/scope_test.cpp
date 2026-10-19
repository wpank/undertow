// Phase 4 spec tests: the engine's scope tap for the editor (docs/specs/phase-4-ui.md, P4.01).
// spec-needs: plugin/ui/UiFrame.h

#include "doctest.h"

#include "spec_alloc.h"
#include "spec_engine.h"
#include "undertow/engine.h"

#include <cmath>
#include <initializer_list>
#include <vector>

TEST_CASE("P4.01 copyScope hands the editor the latest output, oldest first, and meters() names the clock in use") {
    undertow::Engine engine;
    engine.prepare(48000.0, 512);
    spec::apply(engine, spec::subPatch());
    engine.reset();
    // Render 3000 samples in odd blocks (none longer than the prepared 512) and keep the mono output ((left + right) / 2).
    std::vector<float> mono;
    std::vector<float> left(512), right(512);
    const std::vector<undertow::MidiEvent> on = {spec::noteOn(0, 45)};
    auto play = [&](std::initializer_list<int> sizes) {
        for (int size : sizes) {
            const bool first = mono.empty();
            engine.process(left.data(), right.data(), size, first ? on.data() : nullptr, first ? 1 : 0, {});
            for (int n = 0; n < size; ++n)
                mono.push_back(0.5f * (left[static_cast<std::size_t>(n)] + right[static_cast<std::size_t>(n)]));
        }
    };
    play({300, 511, 7, 512, 1, 333, 480, 512, 344});
    REQUIRE(mono.size() == 3000u);
    std::vector<float> scope(2048, -9.0f);
    spec::AllocationCounter counter;
    const int copied = engine.copyScope(scope.data(), 2048);
    const std::size_t allocations = counter.count();
    REQUIRE_MESSAGE(copied == 2048, "copyScope(…, 2048) after 3000 samples must copy 2048; it copied " << copied);
    int wrong = 0;
    for (int i = 0; i < 2048; ++i)
        wrong += std::fabs(scope[static_cast<std::size_t>(i)] - mono[static_cast<std::size_t>(3000 - 2048 + i)]) > 1e-6f ? 1 : 0;
    CHECK_MESSAGE(wrong == 0, wrong << " of 2048 scope samples differ from the last 2048 mono output samples ((left + right) / 2), "
                                       "oldest first");
    CHECK_MESSAGE(allocations == 0, "copyScope allocated " << allocations << " times");

    // After 5000 samples, asking for more than kScopeSamples gives the latest kScopeSamples.
    play({500, 500, 500, 500});
    std::vector<float> big(10000, -9.0f);
    const int clamped = engine.copyScope(big.data(), 10000);
    REQUIRE_MESSAGE(clamped == undertow::kScopeSamples, "after 5000 samples copyScope(…, 10000) must copy kScopeSamples ("
                                                            << undertow::kScopeSamples << "), the most it keeps; it copied " << clamped);
    CHECK_MESSAGE(big[static_cast<std::size_t>(clamped)] == -9.0f, "copyScope wrote past the count it returned");
    int late = 0;
    for (int i = 0; i < clamped; ++i)
        late += std::fabs(big[static_cast<std::size_t>(i)] - mono[mono.size() - static_cast<std::size_t>(clamped) + static_cast<std::size_t>(i)]) > 1e-6f
                    ? 1
                    : 0;
    CHECK_MESSAGE(late == 0, late << " of the " << clamped << " samples differ from the latest output, oldest first (the ring must wrap)");

    undertow::Engine fresh;
    fresh.prepare(48000.0, 512);
    std::vector<float> few(100, -9.0f);
    const int early = fresh.copyScope(few.data(), 100);
    CHECK_MESSAGE((early == 0 || few[0] == 0.0f), "before any audio, copyScope returns 0 samples or silence");

    // The editor's clock readout: meters() reports whether the host's clock drives the engine, and its tempo.
    undertow::Engine clock;
    clock.prepare(48000.0, 512);
    clock.setParam(undertow::findParam("seq_bpm"), 100.0f);
    std::vector<float> l(256), r(256);
    clock.process(l.data(), r.data(), 256, nullptr, 0, undertow::Transport{true, true, 133.0, 4.0});
    CHECK_MESSAGE(clock.meters().hostClock, "with a valid host transport and seq_sync Host, meters().hostClock is true");
    CHECK_MESSAGE(std::fabs(clock.meters().bpm - 133.0) <= 1e-9, "meters().bpm is the host's tempo (133); got " << clock.meters().bpm);
    clock.process(l.data(), r.data(), 256, nullptr, 0, undertow::Transport{});
    CHECK_MESSAGE(!clock.meters().hostClock, "without a host transport meters().hostClock is false");
    CHECK_MESSAGE(std::fabs(clock.meters().bpm - 100.0) <= 1e-9, "meters().bpm is seq_bpm (100) on the internal clock; got "
                                                                     << clock.meters().bpm);
}
