#include "undertow/presets.h"

#include <array>
#include <cstddef>

namespace undertow {
namespace {

constexpr const char* kName = "Init";
constexpr const char* kSlug = "init";

std::array<float, kNumParams> makeInitValues() {
    std::array<float, kNumParams> values{};
    for (int i = 0; i < kNumParams; ++i)
        values[static_cast<std::size_t>(i)] = paramSpec(i).def;
    return values;
}

const std::array<float, kNumParams> kInitValues = makeInitValues();

}  // namespace

int numFactoryPresets() {
    return 1;
}

const char* factoryPresetName(int index) {
    return index == 0 ? kName : "";
}

const char* factoryPresetSlug(int index) {
    return index == 0 ? kSlug : "";
}

bool factoryPreset(int index, float* values) {
    if (index != 0 || values == nullptr)
        return false;
    for (int i = 0; i < kNumParams; ++i)
        values[i] = kInitValues[static_cast<std::size_t>(i)];
    return true;
}

int findFactoryPreset(std::string_view nameOrSlug) {
    return nameOrSlug == kName || nameOrSlug == kSlug ? 0 : -1;
}

}  // namespace undertow
