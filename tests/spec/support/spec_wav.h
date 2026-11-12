#pragma once

// A small WAV reader for the spec tests of the renders: RIFF/WAVE with a PCM (16 or 24-bit) or float (32-bit)
// data chunk.

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace spec {

struct Wav {
    bool ok = false;
    std::string error;
    int format = 0;        // 1 = PCM, 3 = IEEE float
    int channels = 0;
    int sampleRate = 0;
    int bitsPerSample = 0;
    std::size_t frames = 0;
    std::vector<float> left, right;   // right is left for mono files
    std::vector<char> bytes;          // the whole file
};

inline Wav readWav(const std::string& path) {
    Wav w;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        w.error = "cannot open " + path;
        return w;
    }
    w.bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    const auto& b = w.bytes;
    auto u16 = [&](std::size_t at) { return static_cast<std::uint32_t>(static_cast<unsigned char>(b[at]) | static_cast<unsigned char>(b[at + 1]) << 8); };
    auto u32 = [&](std::size_t at) { return u16(at) | u16(at + 2) << 16; };
    if (b.size() < 12 || std::memcmp(b.data(), "RIFF", 4) != 0 || std::memcmp(b.data() + 8, "WAVE", 4) != 0) {
        w.error = "not a RIFF/WAVE file";
        return w;
    }
    std::size_t at = 12, dataAt = 0, dataSize = 0;
    while (at + 8 <= b.size()) {
        const std::uint32_t size = u32(at + 4);
        if (std::memcmp(b.data() + at, "fmt ", 4) == 0 && at + 24 <= b.size()) {
            w.format = static_cast<int>(u16(at + 8));
            w.channels = static_cast<int>(u16(at + 10));
            w.sampleRate = static_cast<int>(u32(at + 12));
            w.bitsPerSample = static_cast<int>(u16(at + 22));
            if (w.format == 0xFFFE && size >= 40 && at + 8 + 26 <= b.size())
                w.format = static_cast<int>(u16(at + 8 + 24)); // WAVE_FORMAT_EXTENSIBLE: the sub-format's tag
        } else if (std::memcmp(b.data() + at, "data", 4) == 0) {
            dataAt = at + 8;
            dataSize = std::min<std::size_t>(size, b.size() - dataAt);
        }
        at += 8 + size + (size & 1);
    }
    if (w.channels < 1 || w.sampleRate <= 0 || dataAt == 0) {
        w.error = "no fmt or data chunk";
        return w;
    }
    const int bytesPerSample = w.bitsPerSample / 8;
    if (!((w.format == 1 && (bytesPerSample == 2 || bytesPerSample == 3)) || (w.format == 3 && bytesPerSample == 4))) {
        w.error = "unsupported sample format " + std::to_string(w.format) + "/" + std::to_string(w.bitsPerSample);
        return w;
    }
    w.frames = dataSize / static_cast<std::size_t>(bytesPerSample * w.channels);
    w.left.resize(w.frames);
    w.right.resize(w.frames);
    for (std::size_t f = 0; f < w.frames; ++f) {
        for (int c = 0; c < std::min(w.channels, 2); ++c) {
            const std::size_t p = dataAt + (f * static_cast<std::size_t>(w.channels) + static_cast<std::size_t>(c)) * static_cast<std::size_t>(bytesPerSample);
            float v = 0.0f;
            if (w.format == 3) {
                std::uint32_t raw = u32(p);
                std::memcpy(&v, &raw, 4);
            } else if (bytesPerSample == 2) {
                v = static_cast<float>(static_cast<std::int16_t>(u16(p))) / 32768.0f;
            } else {
                std::int32_t raw = static_cast<std::int32_t>(u16(p) | static_cast<std::uint32_t>(static_cast<unsigned char>(b[p + 2])) << 16);
                if (raw & 0x800000)
                    raw -= 0x1000000;
                v = static_cast<float>(raw) / 8388608.0f;
            }
            (c == 0 ? w.left : w.right)[f] = v;
        }
        if (w.channels == 1)
            w.right[f] = w.left[f];
    }
    w.ok = true;
    return w;
}

} // namespace spec
