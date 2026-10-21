#include "take.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace chairs {

bool Take::save(const std::string& path) const {
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;
    std::fprintf(f, "take 1\nseed %u\ngen %d\ncast", seed, gen);
    for (int id : cast) std::fprintf(f, " %d", id);
    std::fprintf(f, "\n");
    for (const TakeFrame& k : frames)
        std::fprintf(f, "f %.5f %ld %c %.4f %.4f %.4f %.4f %.4f %.4f %.3f %d\n", k.t, k.steps, k.mode, k.eye.x, k.eye.y,
                     k.eye.z, k.target.x, k.target.y, k.target.z, k.fov, k.tags ? 1 : 0);
    std::fclose(f);
    return true;
}

bool Take::load(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return false;
    char line[512];
    while (std::fgets(line, sizeof(line), f)) {
        if (!std::strncmp(line, "seed ", 5)) {
            seed = static_cast<uint32_t>(std::strtoul(line + 5, nullptr, 10));
        } else if (!std::strncmp(line, "gen ", 4)) {
            gen = std::atoi(line + 4);
        } else if (!std::strncmp(line, "cast", 4)) {
            char* p = line + 4;
            char* end = nullptr;
            for (long id = std::strtol(p, &end, 10); end != p; id = std::strtol(p, &end, 10)) {
                cast.push_back(static_cast<int>(id));
                p = end;
            }
        } else if (line[0] == 'f') {
            TakeFrame k;
            int tags = 1;
            if (std::sscanf(line, "f %lf %ld %c %f %f %f %f %f %f %f %d", &k.t, &k.steps, &k.mode, &k.eye.x, &k.eye.y,
                            &k.eye.z, &k.target.x, &k.target.y, &k.target.z, &k.fov, &tags) == 11) {
                k.tags = tags != 0;
                frames.push_back(k);
            }
        }
    }
    std::fclose(f);
    return !frames.empty() && !cast.empty();
}

TakeFrame Take::at(double t) const {
    if (frames.empty()) return {};
    auto it = std::upper_bound(frames.begin(), frames.end(), t, [](double v, const TakeFrame& k) { return v < k.t; });
    if (it == frames.begin()) return frames.front();
    if (it == frames.end()) return frames.back();
    const TakeFrame& a = *(it - 1);
    const TakeFrame& b = *it;
    const float s = static_cast<float>((t - a.t) / std::max(b.t - a.t, 1e-6));
    TakeFrame k = a;
    k.t = t;
    k.steps = a.steps + static_cast<long>(static_cast<double>(b.steps - a.steps) * s);
    k.eye = lerp(a.eye, b.eye, s);
    k.target = lerp(a.target, b.target, s);
    k.fov = a.fov + (b.fov - a.fov) * s;
    return k;
}

}  // namespace chairs
