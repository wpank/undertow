#pragma once

// Where a spec test finds its checkout and build tree at run time.
//
// tools/spec.sh and tools/check.sh export UNDERTOW_ROOT, the checkout they run in. The build tree is
// UNDERTOW_BUILD_IN_SOURCE under it, and a file the build made is at its path relative to the build tree. So a test
// compiled in one checkout and run in another reads and writes the checkout it runs in. That happens when a build
// tree is copied, compiled objects included, from one checkout to another, as roko's warm attempt trees are; the
// absolute paths compiled into the test still name the first checkout. Without UNDERTOW_ROOT (ctest run by hand),
// those compiled paths serve.

#include <cstdlib>
#include <string>

namespace spec {

inline const char* runRoot() {
    const char* root = std::getenv("UNDERTOW_ROOT");
    return root != nullptr && root[0] != '\0' ? root : nullptr;
}

// The checkout: where artifacts/ and the scripts are.
inline std::string sourceDir() {
    return runRoot() != nullptr ? std::string(runRoot()) : std::string(UNDERTOW_SOURCE_DIR);
}

// This test's build tree (build/core or build/plugin).
inline std::string binaryDir() {
    if (runRoot() == nullptr)
        return UNDERTOW_BINARY_DIR;
    return std::string(runRoot()) + "/" + UNDERTOW_BUILD_IN_SOURCE;
}

// A file the build made, given by its path in the build tree and the absolute path compiled in.
inline std::string built(const std::string& inBuild, const std::string& compiled) {
    return runRoot() != nullptr && !inBuild.empty() ? binaryDir() + "/" + inBuild : compiled;
}

} // namespace spec
