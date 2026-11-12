// SPDX-License-Identifier: AGPL-3.0-only
// Phase 7 spec tests: strict validation's kept log (docs/specs/phase-7-ship.md, check P7.05).
// spec-needs: artifacts/reports/pluginval-strict.txt

#include "doctest.h"

#include "spec_paths.h"

#include <cctype>
#include <fstream>
#include <string>
#include <vector>

namespace {

const std::string kLog = "artifacts/reports/pluginval-strict.txt";

std::vector<std::string> linesOf(const std::string& path) {
    std::ifstream in(path);
    std::vector<std::string> lines;
    for (std::string line; std::getline(in, line);)
        lines.push_back(line);
    return lines;
}

bool has(const std::vector<std::string>& lines, const std::string& text) {
    for (const auto& line : lines)
        if (line.find(text) != std::string::npos)
            return true;
    return false;
}

} // namespace

TEST_CASE("P7.05 the kept log: pluginval at strictness 10 passed every test on Metakosmia Undertow 2 2.0.0") {
    const auto lines = linesOf(spec::sourceDir() + "/" + kLog);
    REQUIRE_MESSAGE(!lines.empty(), "there is no " << kLog << ": run `tools/pluginval.sh 10 " << kLog
                                                   << "` once the plugin is final, and commit the log it keeps");
    CHECK_MESSAGE(has(lines, "Strictness level: 10"), "the log is not a strictness 10 run: `tools/pluginval.sh 10 "
                                                          << kLog << "` makes it");
    CHECK_MESSAGE(has(lines, "Random seed: 0x2a"), "the log is not of tools/pluginval.sh's run, seed 42 (0x2a)");
    CHECK_MESSAGE(has(lines, "Metakosmia: Metakosmia Undertow 2 v2.0.0"),
                  "the log does not validate Metakosmia Undertow 2 version 2.0.0");
    // The groups strictness 10 adds weight to: state restored after fuzzing, automation, threads, the VST3 validator.
    for (const char* group : {"Plugin state restoration", "Automation", "Parameter thread safety", "vst3 validator",
                              "Fuzz parameters"})
        CHECK_MESSAGE(has(lines, std::string("Completed tests in pluginval / ") + group),
                      "the log never completes pluginval's \"" << std::string(group)
                                                                << "\" tests: keep the whole run's log");
    std::string failures;
    for (std::size_t i = 0; i < lines.size(); ++i)
        if (lines[i].find("FAILED") != std::string::npos || lines[i].find("!!! Test") != std::string::npos)
            failures += "\n  line " + std::to_string(i + 1) + ": " + lines[i];
    CHECK_MESSAGE(failures.empty(), "the log records failures:" << failures);
    std::string last;
    for (const auto& line : lines)
        if (line.find_first_not_of(" \t\r") != std::string::npos)
            last = line;
    CHECK_MESSAGE(last == "SUCCESS", "the log ends with \"" << last << "\", not SUCCESS: pluginval's whole run passes");
}

TEST_CASE("P7.05 the kept log names its paths from the checkout, never a folder outside it") {
    const auto lines = linesOf(spec::sourceDir() + "/" + kLog);
    REQUIRE_MESSAGE(!lines.empty(), "there is no " << kLog);
    // An absolute path: a slash that starts a word and is followed by a name ("/Users/..."). pluginval's own
    // separators ("pluginval / Fuzz parameters") have spaces round them.
    std::string absolute;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];
        for (std::size_t at = line.find('/'); at != std::string::npos; at = line.find('/', at + 1)) {
            const bool startsWord = at == 0 || line[at - 1] == ' ' || line[at - 1] == ':' || line[at - 1] == '"' ||
                                    line[at - 1] == '\'' || line[at - 1] == '(' || line[at - 1] == '=';
            const bool named = at + 1 < line.size() && std::isalnum(static_cast<unsigned char>(line[at + 1])) != 0;
            if (startsWord && named) {
                absolute += "\n  line " + std::to_string(i + 1) + ": " + line;
                break;
            }
        }
    }
    CHECK_MESSAGE(absolute.empty(), "the log names absolute paths:" << absolute
                                                                    << "\ntools/pluginval.sh keeps it with paths "
                                                                       "relative to the checkout: make it with that "
                                                                       "script, never by hand");
}
