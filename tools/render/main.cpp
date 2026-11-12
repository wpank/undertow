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
using undertow::MidiEvent;
using undertow::Transport;

struct Options {
    bool list = false;
    bool noteSpecified = false;
    std::string preset;
    std::string out;
    std::string hits;
    int bars = 8;
    double bpm = 130.0;
    int note = 0;
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
        if (arg == "--list") {
            options.list = true;
        } else if (arg == "--preset") {
            const char* value = needValue(arg);
            if (!value) return false;
            options.preset = value;
        } else if (arg == "--out") {
            const char* value = needValue(arg);
            if (!value) return false;
            options.out = value;
        } else if (arg == "--hits") {
            const char* value = needValue(arg);
            if (!value) return false;
            options.hits = value;
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
            if (!parseInt(value, options.note) || options.note < 0 || options.note > 127) {
                error = "invalid --note value";
                return false;
            }
            options.noteSpecified = true;
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
    if (options.list)
        return true;
    if (options.preset.empty()) { error = "missing --preset"; return false; }
    if (options.out.empty()) { error = "missing --out"; return false; }
    return true;
}

int presetIndexFromOption(std::string_view preset) {
    int index = -1;
    if (parseInt(preset, index)) return index;
    return undertow::findFactoryPreset(preset);
}

bool loadPreset(const Options& options, Engine& engine, std::string& error) {
    const int index = presetIndexFromOption(options.preset);
    if (index < 0 || index >= undertow::numFactoryPresets()) {
        error = "unknown preset: " + options.preset;
        return false;
    }

    std::vector<float> values(static_cast<std::size_t>(undertow::kNumParams));
    if (!undertow::factoryPreset(index, values.data())) {
        error = "unknown preset: " + options.preset;
        return false;
    }
    for (int i = 0; i < undertow::kNumParams; ++i)
        engine.setParam(i, values[static_cast<std::size_t>(i)]);
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

bool writeHitsJson(const std::string& path, int rate, double bpm, int bars, int root,
                   const std::vector<undertow::Hit>& hits, std::string& error) {
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        error = "failed to open output file: " + path;
        return false;
    }

    std::ostringstream json;
    json << std::defaultfloat << std::setprecision(6);
    json << "{\n";
    json << "  \"rate\": " << rate << ",\n";
    json << "  \"bpm\": " << bpm << ",\n";
    json << "  \"bars\": " << bars << ",\n";
    json << "  \"root\": " << root << ",\n";
    json << "  \"hits\": [\n";
    for (std::size_t i = 0; i < hits.size(); ++i) {
        const auto& hit = hits[i];
        json << "    {\"sample\": " << hit.sampleOffset << ", \"step\": "
             << (hit.step < 0 ? 0 : hit.step + 1) << ", \"note\": " << hit.note << ", \"pitch\": "
             << (hit.note - root) << ", \"velocity\": " << hit.velocity << "}";
        if (i + 1 < hits.size())
            json << ',';
        json << '\n';
    }
    json << "  ]\n";
    json << "}\n";

    out << json.str();
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

    if (options.list) {
        for (int i = 0; i < undertow::numFactoryPresets(); ++i)
            std::cout << undertow::factoryPresetSlug(i) << '\n';
        return 0;
    }

    Engine engine;
    engine.prepare(options.sampleRate, 512);
    if (!loadPreset(options, engine, error)) {
        std::cerr << "error: " << error << '\n';
        return 1;
    }
    const int root = options.noteSpecified ? options.note : static_cast<int>(engine.getParam(undertow::param::SeqRoot));
    engine.reset();

    constexpr int kPrepareBlock = 512;
    const int framesPerBar = static_cast<int>(std::llround(options.sampleRate * 4.0 * 60.0 / options.bpm));
    const int totalFrames = framesPerBar * options.bars;
    std::vector<float> left(static_cast<std::size_t>(totalFrames));
    std::vector<float> right(static_cast<std::size_t>(totalFrames));

    Transport transport;
    transport.valid = true;
    transport.playing = true;
    transport.bpm = options.bpm;
    transport.ppq = 0.0;

    const MidiEvent noteOn{0, 0x90, static_cast<std::uint8_t>(root), 100};
    std::vector<undertow::Hit> hits;
    hits.reserve(static_cast<std::size_t>(totalFrames / 16 + 1));
    int remaining = totalFrames;
    int offset = 0;
    bool sentNote = false;
    while (remaining > 0) {
        const int block = std::min(remaining, kPrepareBlock);
        const MidiEvent* events = sentNote ? nullptr : &noteOn;
        const int numEvents = sentNote ? 0 : 1;
        engine.process(left.data() + offset, right.data() + offset, block, events, numEvents, transport);
        undertow::Hit blockHits[undertow::kMaxHits];
        const int hitCount = engine.hits(blockHits, undertow::kMaxHits);
        for (int i = 0; i < hitCount; ++i)
            blockHits[i].sampleOffset += offset;
        hits.insert(hits.end(), blockHits, blockHits + hitCount);
        sentNote = true;
        if (transport.valid && transport.playing)
            transport.ppq += static_cast<double>(block) / options.sampleRate * options.bpm / 60.0;
        offset += block;
        remaining -= block;
    }

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
    if (!options.hits.empty() && !writeHitsJson(options.hits, static_cast<int>(options.sampleRate), options.bpm,
                                               options.bars, root, hits, error)) {
        std::cerr << "error: " << error << '\n';
        return 1;
    }

    std::cout << std::fixed << std::setprecision(1)
              << "wrote " << options.out << ": " << totalFrames
              << " frames, peak " << peakDb << " dBFS, rms " << rmsDb << " dBFS\n";
    return 0;
}
