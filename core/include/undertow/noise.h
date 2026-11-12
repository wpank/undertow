#pragma once

namespace undertow {

class Noise {
public:
    void reset();
    float process();

private:
    static constexpr unsigned int kSeed = 0x12345678u;
    static constexpr double kScale = 0.86602540378443864676;  // sqrt(3) / 2

    unsigned int state_ = kSeed;
};

}  // namespace undertow
