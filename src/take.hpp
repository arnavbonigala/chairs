#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/math.hpp"

namespace chairs {

using namespace skein;

struct TakeFrame {
    double t = 0;
    long steps = 0;
    char mode = 'r';
    Vec3 eye;
    Vec3 target;
    float fov = 50.0f;
    bool tags = true;
};

struct Take {
    uint32_t seed = 0;
    int gen = 0;
    std::vector<int> cast;
    std::vector<TakeFrame> frames;

    bool save(const std::string& path) const;
    bool load(const std::string& path);
    TakeFrame at(double t) const;
    double length() const { return frames.empty() ? 0.0 : frames.back().t; }
};

}  // namespace chairs
