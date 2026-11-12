// SPDX-License-Identifier: AGPL-3.0-only
// Phase 7 spec tests: the install, into a test folder and never into the person's own folders
// (docs/specs/phase-7-ship.md, check P7.07).
// spec-preset: plugin
// spec-needs: tools/install.sh

#include "doctest.h"

#include "spec_paths.h"

#include <juce_core/juce_core.h>

#include <cstdio>
#include <memory>
#include <string>

#include <sys/wait.h>

namespace {

struct Run {
    int status = -1;
    std::string output;
};

// Run a command from the repository root, capturing what it prints.
Run shell(const std::string& command) {
    Run r;
    FILE* pipe = popen(("cd '" + spec::sourceDir() + "' && " + command + " 2>&1").c_str(), "r");
    REQUIRE_MESSAGE(pipe != nullptr, "cannot run " << command);
    char line[512];
    while (fgets(line, sizeof line, pipe) != nullptr)
        r.output += line;
    const int raw = pclose(pipe);
    r.status = WIFEXITED(raw) ? WEXITSTATUS(raw) : -1;
    return r;
}

juce::XmlElement* plistValue(juce::XmlElement& dict, const juce::String& key) {
    for (auto* child = dict.getFirstChildElement(); child != nullptr; child = child->getNextElement())
        if (child->hasTagName("key") && child->getAllSubText() == key)
            return child->getNextElement();
    return nullptr;
}

std::unique_ptr<juce::XmlElement> readPlist(const juce::File& bundle) {
    auto xml = juce::XmlDocument::parse(bundle.getChildFile("Contents/Info.plist"));
    if (xml == nullptr)
        return nullptr;
    auto* dict = xml->getChildByName("dict");
    return dict != nullptr ? std::make_unique<juce::XmlElement>(*dict) : nullptr;
}

juce::String text(juce::XmlElement& dict, const char* key) {
    auto* value = plistValue(dict, key);
    return value != nullptr ? value->getAllSubText() : juce::String("<missing>");
}

// The test destination the checks install into: tools/install.sh --dest build/install-test.
juce::File destination() { return juce::File{juce::String(spec::sourceDir() + "/build/install-test")}; }

// Undertow 2's three bundles, in the destination's three folders.
const char* const kBundles[] = {"VST3/Metakosmia Undertow 2.vst3", "Components/Metakosmia Undertow 2.component",
                                "Applications/Metakosmia Undertow 2.app"};

// Undertow 1's, which a person may have installed in the same folders.
const char* const kUndertow1[] = {"VST3/Metakosmia Undertow.vst3", "Components/Metakosmia Undertow.component",
                                  "Applications/Metakosmia Undertow.app"};

const char* const kUndertow1Plist = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<plist version=\"1.0\"><dict>"
                                    "<key>CFBundleIdentifier</key><string>audio.metakosmia.undertow</string>"
                                    "</dict></plist>\n";

} // namespace

TEST_CASE("P7.07 install.sh --dest installs the three bundles whole, signed, as Undertow 2 version 2.0.0") {
    const auto dest = destination();
    dest.deleteRecursively(); // an earlier run's copies must not stand in for this one's
    const auto install = shell("tools/install.sh --dest build/install-test");
    REQUIRE_MESSAGE(install.status == 0, "tools/install.sh --dest build/install-test failed:\n" << install.output);
    for (const char* path : kBundles) {
        const auto bundle = dest.getChildFile(path);
        INFO("installed bundle build/install-test/" << std::string(path));
        REQUIRE_MESSAGE(bundle.isDirectory(), "tools/install.sh --dest <dir> installs this bundle here");
        CHECK_MESSAGE(bundle.getChildFile("Contents/MacOS/Metakosmia Undertow 2").existsAsFile(),
                      "its executable, Contents/MacOS/Metakosmia Undertow 2, is missing: install the whole bundle "
                      "(cp -R), never its parts");
        auto plist = readPlist(bundle);
        REQUIRE_MESSAGE(plist != nullptr, "its Contents/Info.plist is missing or not an XML plist");
        CHECK_MESSAGE(text(*plist, "CFBundleIdentifier") == "audio.metakosmia.undertow2",
                      "its CFBundleIdentifier is " << text(*plist, "CFBundleIdentifier"));
        CHECK_MESSAGE(text(*plist, "CFBundleShortVersionString") == "2.0.0",
                      "its CFBundleShortVersionString is " << text(*plist, "CFBundleShortVersionString"));
        CHECK_MESSAGE(text(*plist, "CFBundleVersion") == "2.0.0",
                      "its CFBundleVersion is " << text(*plist, "CFBundleVersion"));
        const auto verify = shell("codesign --verify --deep --strict '" + bundle.getFullPathName().toStdString() + "'");
        CHECK_MESSAGE(verify.status == 0, "its signature does not verify: sign each installed copy ad hoc "
                                          "(codesign --force --deep --sign -) once it is copied\n"
                                              << verify.output);
    }
    // What auval and a host read to find the instrument: the AU's component entry.
    auto au = readPlist(dest.getChildFile(kBundles[1]));
    REQUIRE(au != nullptr);
    auto* components = plistValue(*au, "AudioComponents");
    REQUIRE_MESSAGE(components != nullptr, "the installed AU's Info.plist has no AudioComponents");
    auto* component = components->getChildByName("dict");
    REQUIRE(component != nullptr);
    CHECK_MESSAGE(text(*component, "type") == "aumu", "the AU's type is " << text(*component, "type")
                                                                          << ": an instrument is aumu");
    CHECK_MESSAGE(text(*component, "subtype") == "MkU2", "the AU's subtype is " << text(*component, "subtype"));
    CHECK_MESSAGE(text(*component, "manufacturer") == "Mtks",
                  "the AU's manufacturer is " << text(*component, "manufacturer"));
    CHECK_MESSAGE(text(*component, "name") == "Metakosmia: Metakosmia Undertow 2",
                  "the AU's name is " << text(*component, "name"));
    CHECK_MESSAGE(text(*component, "version") == "131072",
                  "the AU's version is " << text(*component, "version") << "; 2.0.0 is 131072 (0x00020000)");
}

TEST_CASE("P7.07 it installs beside Undertow 1, and --uninstall removes Undertow 2's three bundles and nothing else") {
    const auto dest = destination();
    dest.deleteRecursively();
    for (const char* path : kUndertow1) {
        const auto plist = dest.getChildFile(path).getChildFile("Contents/Info.plist");
        REQUIRE(plist.getParentDirectory().createDirectory());
        REQUIRE(plist.replaceWithText(kUndertow1Plist, false, false, "\n"));
    }
    auto untouched = [&](const char* when) {
        for (const char* path : kUndertow1)
            CHECK_MESSAGE(dest.getChildFile(path).getChildFile("Contents/Info.plist").loadFileAsString() ==
                              kUndertow1Plist,
                          "Undertow 1's " << std::string(path) << " changed " << std::string(when)
                                          << ": Undertow 2 installs beside it and never touches its bundles");
    };
    const auto install = shell("tools/install.sh --dest build/install-test");
    REQUIRE_MESSAGE(install.status == 0, "tools/install.sh --dest build/install-test failed:\n" << install.output);
    for (const char* path : kBundles)
        REQUIRE_MESSAGE(dest.getChildFile(path).isDirectory(),
                        "tools/install.sh --dest did not install " << std::string(path));
    untouched("by the install");
    const auto uninstall = shell("tools/install.sh --uninstall --dest build/install-test");
    REQUIRE_MESSAGE(uninstall.status == 0,
                    "tools/install.sh --uninstall --dest build/install-test failed:\n" << uninstall.output);
    for (const char* path : kBundles)
        CHECK_MESSAGE(!dest.getChildFile(path).exists(), std::string(path) << " is still installed after --uninstall");
    untouched("by --uninstall");
}
