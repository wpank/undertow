// Phase 5 spec tests: the install script (docs/specs/phase-5-ship.md, P5.07). Run after
// `tools/install.sh --dest build/install-test`: a test install, never into the user's own folders.
// spec-needs: core/include/undertow/presets.h

#include "doctest.h"

#include "spec_paths.h"

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

namespace {

bool exists(const std::string& path) { return std::system(("test -e '" + path + "'").c_str()) == 0; }

bool executable(const std::string& path) { return std::system(("test -f '" + path + "' -a -x '" + path + "'").c_str()) == 0; }

std::string read(const std::string& path) {
    std::ifstream in(path);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

} // namespace

TEST_CASE("P5.07 install.sh --dest puts the three signed bundles where a DAW and the Finder look") {
    const std::string dest = spec::sourceDir() + "/build/install-test";  // in the checkout this test runs in
    const std::string bundles[] = {dest + "/VST3/Metakosmia Undertow.vst3", dest + "/Components/Metakosmia Undertow.component",
                                   dest + "/Applications/Metakosmia Undertow.app"};
    for (const auto& bundle : bundles) {
        INFO(bundle);
        REQUIRE_MESSAGE(exists(bundle), "install.sh --dest " << dest << " must put this bundle here");
        CHECK_MESSAGE(executable(bundle + "/Contents/MacOS/Metakosmia Undertow"),
                      "the installed bundle must hold its executable, Contents/MacOS/Metakosmia Undertow: copy the whole bundle "
                      "(cp -R), not its parts");
        CHECK_MESSAGE(read(bundle + "/Contents/Info.plist").find("audio.metakosmia.undertow") != std::string::npos,
                      "the installed bundle's Info.plist must carry the bundle id audio.metakosmia.undertow");
        CHECK_MESSAGE(std::system(("codesign --verify --deep --strict '" + bundle + "' 2>/dev/null").c_str()) == 0,
                      "the installed bundle must be signed (ad hoc: codesign --force --deep --sign -) and verify");
    }
    // What auval and a host read first: the AU's component entry (type, subtype, manufacturer).
    const std::string au = read(dest + "/Components/Metakosmia Undertow.component/Contents/Info.plist");
    for (const char* entry : {"<string>aumu</string>", "<string>MkUt</string>", "<string>Mtks</string>"})
        CHECK_MESSAGE(au.find(entry) != std::string::npos, "the installed AU's Info.plist must declare its AudioComponents entry "
                                                           "(type aumu, subtype MkUt, manufacturer Mtks); "
                                                               << std::string(entry) << " is missing");
}
