#include "undertow/pattern.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace undertow {
namespace {

constexpr std::uint32_t kStepHashSalt = 0x9e3779b9u;

std::uint32_t hash32(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

long long floorDiv(long long a, long long b) {
    return a >= 0 ? a / b : -((-a + b - 1) / b);
}

int changedOnPass(int sounding, float drift, int j) {
    const double x = 3.0 * static_cast<double>(drift) * std::sin(3.14159265358979323846 * static_cast<double>(j) / 16.0);
    const int changed = static_cast<int>(std::floor(x + 0.5));
    return sounding < changed ? sounding : changed;
}

bool canMove(int root, int pitch, int subOctave) {
    if (pitch < -12 || pitch > 12)
        return false;
    const int subNote = root + pitch - (subOctave ? 12 : 0);
    return subNote >= 28;
}

}  // namespace

void Pattern::setParams(const float* values) {
    for (int i = 0; i < kSteps; ++i) {
        home_[static_cast<std::size_t>(i)].pitch = static_cast<int>(std::lround(values[param::Step1Pitch + i]));
        home_[static_cast<std::size_t>(i)].velocity = values[param::Step1Vel + i];
    }

    const int length = static_cast<int>(std::lround(values[param::SeqLength]));
    length_ = length < 1 ? 1 : length > kSteps ? kSteps : length;
    drift_ = values[param::SeqDrift];
    subOctave_ = values[param::SubOctave] > 0.5f ? 1 : 0;
}

int Pattern::length() const {
    return length_;
}

Step Pattern::home(int index) const {
    return home_[static_cast<std::size_t>(index)];
}

Step Pattern::at(long long count, int root) const {
    const int length = length_;
    const long long pass = floorDiv(count, length);
    const int step = static_cast<int>(count - pass * length);
    Step out = home_[static_cast<std::size_t>(step)];
    if (out.velocity <= 0.0f || drift_ <= 0.0f)
        return out;

    const long long arc = floorDiv(pass, 16);
    const int j = static_cast<int>(pass - arc * 16);
    const std::uint32_t a = static_cast<std::uint32_t>(arc);

    struct RankedStep {
        std::uint32_t order;
        int step;
    };

    RankedStep ranked[kSteps];
    int sounding = 0;
    for (int t = 0; t < length; ++t) {
        if (home_[static_cast<std::size_t>(t)].velocity > 0.0f) {
            ranked[sounding].order = hash32(a * 8u + static_cast<std::uint32_t>(t));
            ranked[sounding].step = t;
            ++sounding;
        }
    }

    const int changeCount = changedOnPass(sounding, drift_, j);
    if (changeCount <= 0)
        return out;

    for (int i = 1; i < sounding; ++i) {
        RankedStep current = ranked[i];
        int jdx = i - 1;
        while (jdx >= 0 && (ranked[jdx].order > current.order ||
                            (ranked[jdx].order == current.order && ranked[jdx].step > current.step))) {
            ranked[jdx + 1] = ranked[jdx];
            --jdx;
        }
        ranked[jdx + 1] = current;
    }

    bool chosen = false;
    for (int i = 0; i < changeCount; ++i) {
        if (ranked[i].step == step) {
            chosen = true;
            break;
        }
    }
    if (!chosen)
        return out;

    const auto kind = hash32((a * 8u + static_cast<std::uint32_t>(step)) ^ kStepHashSalt) % 4u;
    if (kind == 3u) {
        out.velocity *= 0.5f;
        return out;
    }

    const int way = kind == 0u ? 12 : kind == 1u ? 7 : -5;
    const int otherWay = kind == 0u ? -12 : kind == 1u ? -5 : 7;
    if (canMove(root, out.pitch + way, subOctave_))
        out.pitch += way;
    else if (canMove(root, out.pitch + otherWay, subOctave_))
        out.pitch += otherWay;
    return out;
}

}  // namespace undertow
