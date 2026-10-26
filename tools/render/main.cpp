#include "undertow/engine.h"
#include "undertow/presets.h"
#include "undertow/params.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using undertow::Engine;
using undertow::Meters;
using undertow::Pattern;
using undertow::Transport;

struct Options {
    std::string preset;
    std::string out;
    int bars = 4;
    double bpm = 128.0;
    int note = 45;
    double sampleRate = 48000.0;
};

bool parseInt(std::string_view text, int& value) {
    try {
        std::size_t pos = 0;
        int v = std::stoi(std::string(text), &pos, 10);
        if (pos != text.size()) return false;
        value = v;
        return true;
    } catch (...) {
        return false;
    }
}

bool parseDouble(std::string_view text, double& value) {
    try {
        std::size_t pos = 0;
        double v = std::stod(std::string(text), &pos);
        if (pos != text.size() || !std::isfinite(v)) return false;
        value = v;
        return true;
    } catch (...) {
        return false;
    }
}

bool parseArgs(int argc, char** argv, Options& options, std::string& error) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        auto needValue = [&](std::string_view name) -> const char* {
            if (i + 1 >= argc) {
                error = std::string(name) + " requires a value";
                return nullptr;
            }
            return argv[++i];
        };
        if (arg == "--preset") {
            const char* value = needValue(arg);
            if (!value) return false;
            options.preset = value;
        } else if (arg == "--out") {
            const char* value = needValue(arg);
            if (!value) return false;
            options.out = value;
        } else if (arg == "--bars") {
            const char* value = needValue(arg);
            if (!value) return false;
            if (!parseInt(value, options.bars) || options.bars <= 0) {
                error = "invalid --bars value";
                return false;
            }
        } else if (arg == "--bpm") {
            const char* value = needValue(arg);
            if (!value) return false;
            if (!parseDouble(value, options.bpm) || options.bpm <= 0.0) {
                error = "invalid --bpm value";
                return false;
            }
        } else if (arg == "--note") {
            const char* value = needValue(arg);
            if (!value) return false;
            if (!parseInt(value, options.note)) {
                error = "invalid --note value";
                return false;
            }
        } else if (arg == "--sample-rate") {
            const char* value = needValue(arg);
            if (!value) return false;
            if (!parseDouble(value, options.sampleRate) || options.sampleRate <= 0.0) {
                error = "invalid --sample-rate value";
                return false;
            }
        } else {
            error = "unknown argument: " + std::string(arg);
            return false;
        }
    }
    if (options.preset.empty()) { error = "missing --preset"; return false; }
    if (options.out.empty()) { error = "missing --out"; return false; }
    return true;
}

int presetIndexFromOption(std::string_view preset) {
    if (preset == "init") return -2;
    int index = -1;
    if (parseInt(preset, index)) return index;
    return undertow::findFactoryPreset(preset);
}

bool loadPreset(const Options& options, Engine& engine, Pattern& pattern, std::string& error) {
    const int index = presetIndexFromOption(options.preset);
    if (index == -2) {
        for (int i = 0; i < undertow::kNumParams; ++i) engine.setParam(i, undertow::paramSpec(i).def);
        pattern = undertow::defaultPattern();
        engine.setPattern(pattern);
        return true;
    }
    if (index < 0) {
        error = "unknown preset: " + options.preset;
        return false;
    }
    std::vector<float> values(undertow::kNumParams);
    if (!undertow::factoryPreset(index, values.data(), &pattern)) {
        error = "unknown preset: " + options.preset;
        return false;
    }
    for (int i = 0; i < undertow::kNumParams; ++i) engine.setParam(i, values[static_cast<std::size_t>(i)]);
    engine.setPattern(pattern);
    return true;
}

void writeLE16(std::ofstream& out, std::uint16_t value) {
    char bytes[2] = {static_cast<char>(value & 0xFF), static_cast<char>((value >> 8) & 0xFF)};
    out.write(bytes, 2);
}

void writeLE32(std::ofstream& out, std::uint32_t value) {
    char bytes[4] = {
        static_cast<char>(value & 0xFF),
        static_cast<char>((value >> 8) & 0xFF),
        static_cast<char>((value >> 16) & 0xFF),
        static_cast<char>((value >> 24) & 0xFF)
    };
    out.write(bytes, 4);
}

bool writeWav(const std::string& path, double sampleRate, const std::vector<float>& left, const std::vector<float>& right, std::string& error) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        error = "failed to open output file: " + path;
        return false;
    }
    const std::uint32_t numFrames = static_cast<std::uint32_t>(left.size());
    const std::uint32_t dataBytes = numFrames * 2u * 3u;
    out.write("RIFF", 4); writeLE32(out, 36u + dataBytes); out.write("WAVE", 4);
    out.write("fmt ", 4); writeLE32(out, 16u); writeLE16(out, 1u); writeLE16(out, 2u);
    writeLE32(out, static_cast<std::uint32_t>(sampleRate)); writeLE32(out, static_cast<std::uint32_t>(sampleRate) * 2u * 3u);
    writeLE16(out, 6u); writeLE16(out, 24u);
    out.write("data", 4); writeLE32(out, dataBytes);
    auto to24 = [](float x) {
        x = std::clamp(x, -1.0f, 0.9999999f);
        int v = static_cast<int>(std::lrintf(x * 8388607.0f));
        v = std::clamp(v, -8388608, 8388607);
        return v;
    };
    for (std::uint32_t i = 0; i < numFrames; ++i) {
        const int samples[2] = {to24(left[i]), to24(right[i])};
        for (int s : samples) {
            char bytes[3] = {static_cast<char>(s & 0xFF), static_cast<char>((s >> 8) & 0xFF), static_cast<char>((s >> 16) & 0xFF)};
            out.write(bytes, 3);
        }
    }
    return static_cast<bool>(out);
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    std::string error;
    if (!parseArgs(argc, argv, options, error)) {
        std::cerr << "error: " << error << '\n';
        return 1;
    }

    Engine engine;
    engine.prepare(options.sampleRate, 512);
    Pattern pattern;
    if (!loadPreset(options, engine, pattern, error)) {
        std::cerr << "error: " << error << '\n';
        return 1;
    }

    const int framesPerBar = static_cast<int>(std::llround(options.sampleRate * 4.0 * 60.0 / options.bpm));
    const int totalFrames = framesPerBar * options.bars;
    std::vector<float> left(static_cast<std::size_t>(totalFrames));
    std::vector<float> right(static_cast<std::size_t>(totalFrames));

    Transport transport;
    transport.valid = true;
    transport.playing = true;
    transport.bpm = options.bpm;
    transport.ppq = 0.0;
    engine.process(left.data(), right.data(), totalFrames, nullptr, 0, transport);

    Meters meters = engine.meters();
    double peakDb = -std::numeric_limits<double>::infinity();
    double rmsDb = -std::numeric_limits<double>::infinity();
    double peak = 0.0;
    double sumSq = 0.0;
    for (int i = 0; i < totalFrames; ++i) {
        peak = std::max(peak, static_cast<double>(std::max(std::fabs(left[i]), std::fabs(right[i]))));
        sumSq += 0.5 * (static_cast<double>(left[i]) * left[i] + static_cast<double>(right[i]) * right[i]);
    }
    if (peak > 0.0) peakDb = 20.0 * std::log10(peak);
    const double rms = std::sqrt(sumSq / std::max(1, totalFrames));
    if (rms > 0.0) rmsDb = 20.0 * std::log10(rms);

    if (!writeWav(options.out, options.sampleRate, left, right, error)) {
        std::cerr << "error: " << error << '\n';
        return 1;
    }

    std::cout << std::fixed << std::setprecision(1)
              << "wrote " << options.out << ": " << totalFrames
              << " frames, peak " << peakDb << " dBFS, rms " << rmsDb << " dBFS\n";
    (void)meters;
    return 0;
}
