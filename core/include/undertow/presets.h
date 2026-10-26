#pragma once

#include "undertow/params.h"
#include "undertow/sequencer.h"

#include <string_view>

namespace undertow {

int numFactoryPresets();
const char* factoryPresetName(int index);
const char* factoryPresetSlug(int index);
bool factoryPreset(int index, float* values, Pattern* pattern);
int findFactoryPreset(std::string_view nameOrSlug);

}  // namespace undertow
