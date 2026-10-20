#pragma once
#include <string>
#include <vector>

#include "core/math.hpp"

namespace chairs {

using namespace skein;

struct CameraKey {
    Vec3 eye;
    Vec3 target;
    float fov = 45.0f;
};

struct Shot {
    std::string name;
    float seconds = 6.0f;
    std::vector<CameraKey> keys;

    CameraKey at(float t) const;
};

const Shot* findShot(const std::string& name);
const std::vector<Shot>& shots();

}  // namespace chairs
