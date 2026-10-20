#pragma once
#include <functional>

#include "assets/mesh.hpp"
#include "core/random.hpp"

namespace chairs::kit {

using namespace skein;

struct Terrain {
    float size = 100.0f;
    int resolution = 128;
    std::function<float(float x, float z)> height;
    std::function<Vec3(const Vec3& position, const Vec3& normal)> color;
};

MeshData terrain(const Terrain& t);
void displace(MeshData& mesh, float amount, float frequency, uint32_t seed);
void gradient(MeshData& mesh, const Vec3& bottom, const Vec3& top);

MeshData tree(Random& random, const Vec3& leaves, const Vec3& bark);
MeshData pine(Random& random, const Vec3& needles, const Vec3& bark);
MeshData bush(Random& random, const Vec3& leaves);
MeshData rock(Random& random, const Vec3& color);
MeshData flower(Random& random, const Vec3& petals);
MeshData grassTuft(Random& random, const Vec3& base, const Vec3& tip, int blades = 7);
MeshData cloud(Random& random);

MeshData beanBody(float radius = 0.42f, float height = 1.5f);
MeshData eye(float radius = 0.16f);

}  // namespace chairs::kit
