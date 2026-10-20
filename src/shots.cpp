#include "shots.hpp"

#include <algorithm>

namespace chairs {

namespace {

Vec3 catmull(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, float t) {
    const float t2 = t * t, t3 = t2 * t;
    return (b * 2.0f + (c - a) * t + (a * 2.0f - b * 5.0f + c * 4.0f - d) * t2 + (b * 3.0f - a - c * 3.0f + d) * t3) * 0.5f;
}

float ease(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f) * 0.7f + t * 0.3f;
}

constexpr float FRONT = 6.75f;
float column(int i) { return (static_cast<float>(i) - 4.5f) * 1.3f; }

std::vector<Shot> build() {
    std::vector<Shot> all;
    all.push_back({"flyover", 8.0f,
                   {{{-24, 17, -20}, {-3, 0, -3}, 42},
                    {{-10, 13, -6}, {0, 0, 0}, 42},
                    {{4, 10, 9}, {0, 0.4f, 0}, 42},
                    {{16, 6.5f, 19}, {0.5f, 0.8f, 2}, 42}}});
    all.push_back({"reveal", 7.0f,
                   {{{0, 1.1f, FRONT + 3.2f}, {0, 1.0f, FRONT}, 40},
                    {{0, 3.0f, FRONT + 6.0f}, {0, 0.8f, FRONT - 1.0f}, 42},
                    {{0, 9.0f, FRONT + 13.0f}, {0, 0.3f, 0.5f}, 44},
                    {{0, 15.0f, FRONT + 19.0f}, {0, 0, 0}, 45}}});
    all.push_back({"dolly", 7.0f,
                   {{{column(0) - 2.0f, 1.25f, FRONT + 2.6f}, {column(0) - 0.5f, 1.0f, FRONT}, 38},
                    {{column(3), 1.2f, FRONT + 2.5f}, {column(3) + 1.0f, 1.0f, FRONT}, 38},
                    {{column(6), 1.2f, FRONT + 2.5f}, {column(6) + 1.0f, 1.0f, FRONT}, 38},
                    {{column(9) + 2.0f, 1.25f, FRONT + 2.6f}, {column(9) + 0.5f, 1.0f, FRONT}, 38}}});
    all.push_back({"orbit", 9.0f,
                   {{{-17, 6, 13}, {0, 0.5f, 0}, 44},
                    {{-4, 7.5f, 20}, {0, 0.5f, 0}, 44},
                    {{11, 9, 16}, {0, 0.5f, 0}, 44},
                    {{19, 11, 2}, {0, 0.3f, 0}, 44}}});
    auto closeup = [&](const char* name, int col) {
        const float x = column(col);
        all.push_back({name, 5.0f,
                       {{{x + 2.2f, 1.35f, FRONT + 4.6f}, {x, 0.95f, FRONT}, 36},
                        {{x + 1.6f, 1.25f, FRONT + 3.8f}, {x, 0.95f, FRONT}, 35},
                        {{x + 1.0f, 1.18f, FRONT + 3.2f}, {x, 0.95f, FRONT}, 34},
                        {{x + 0.6f, 1.12f, FRONT + 2.8f}, {x, 0.95f, FRONT}, 34}}});
    };
    closeup("closeup-kevin", 4);
    closeup("closeup-lilmoist", 3);
    closeup("closeup-wobbles", 9);
    all.push_back({"rows", 7.0f,
                   {{{-9, 3.2f, -9}, {0, 0.6f, 0}, 38},
                    {{-7.5f, 2.6f, -2}, {0, 0.7f, 2}, 38},
                    {{-8, 2.4f, 4}, {0, 0.8f, 3}, 38},
                    {{-9.5f, 2.8f, 10}, {-2, 0.8f, 5}, 38}}});
    return all;
}

}  // namespace

const std::vector<Shot>& shots() {
    static const std::vector<Shot> all = build();
    return all;
}

const Shot* findShot(const std::string& name) {
    for (const Shot& s : shots())
        if (s.name == name) return &s;
    return nullptr;
}

CameraKey Shot::at(float t) const {
    if (keys.size() == 1) return keys[0];
    const float u = ease(t / seconds) * static_cast<float>(keys.size() - 1);
    const int i = std::min(static_cast<int>(u), static_cast<int>(keys.size()) - 2);
    const float f = u - static_cast<float>(i);
    auto key = [&](int k) { return keys[static_cast<size_t>(std::clamp(k, 0, static_cast<int>(keys.size()) - 1))]; };
    const CameraKey a = key(i - 1), b = key(i), c = key(i + 1), d = key(i + 2);
    return CameraKey{catmull(a.eye, b.eye, c.eye, d.eye, f), catmull(a.target, b.target, c.target, d.target, f),
                     b.fov + (c.fov - b.fov) * f};
}

}  // namespace chairs
