#include "kit.hpp"

#include "core/noise.hpp"

namespace chairs::kit {

using namespace skein;

namespace {

Mat4 place(const Vec3& t, const Vec3& s, float yaw = 0.0f) {
    return composeTRS(t, Quat::axisAngle(Vec3{0, 1, 0}, yaw), s);
}

Vec3 mixColor(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * std::clamp(t, 0.0f, 1.0f); }

Vec3 jitter(Random& random, const Vec3& c, float amount) {
    return Vec3{c.x * (1.0f + amount * random.signedUnit()), c.y * (1.0f + amount * random.signedUnit()),
                c.z * (1.0f + amount * random.signedUnit())};
}

MeshData blob(Random& random, float radius, int subdivisions, float lumps) {
    MeshData m = primitives::icosphere(radius, subdivisions);
    displace(m, radius * lumps, 1.6f / radius, random.next());
    return m;
}

}  // namespace

MeshData terrain(const Terrain& t) {
    MeshData m;
    m.name = "terrain";
    const int n = std::max(t.resolution, 1);
    const float step = t.size / static_cast<float>(n), h = t.size * 0.5f;
    for (int z = 0; z <= n; ++z)
        for (int x = 0; x <= n; ++x) {
            float px = -h + static_cast<float>(x) * step, pz = -h + static_cast<float>(z) * step;
            m.vertices.push_back(Vertex{Vec3{px, t.height ? t.height(px, pz) : 0.0f, pz}, Vec3{0, 1, 0},
                                        Vec2{static_cast<float>(x) / static_cast<float>(n),
                                             static_cast<float>(z) / static_cast<float>(n)}});
        }
    const uint32_t stride = static_cast<uint32_t>(n + 1);
    for (uint32_t z = 0; z < static_cast<uint32_t>(n); ++z)
        for (uint32_t x = 0; x < static_cast<uint32_t>(n); ++x) {
            uint32_t a = z * stride + x, b = a + 1, c = a + stride, d = c + 1;
            m.indices.insert(m.indices.end(), {a, c, b, b, c, d});
        }
    m.recomputeNormals();
    if (t.color)
        for (Vertex& v : m.vertices) v.color = packColor(t.color(v.position, v.normal));
    m.recomputeBounds();
    return m;
}

void displace(MeshData& mesh, float amount, float frequency, uint32_t seed) {
    for (Vertex& v : mesh.vertices) v.position += v.normal * (noise::fbm(v.position * frequency, 3, seed) * amount);
    mesh.recomputeNormals();
    mesh.recomputeBounds();
}

void gradient(MeshData& mesh, const Vec3& bottom, const Vec3& top) {
    const float lo = mesh.bounds.min.y, span = std::max(mesh.bounds.max.y - lo, 1e-5f);
    for (Vertex& v : mesh.vertices) v.color = packColor(mixColor(bottom, top, (v.position.y - lo) / span));
}

MeshData tree(Random& random, const Vec3& leaves, const Vec3& bark) {
    MeshData m;
    m.name = "tree";
    const float height = random.uniform(1.4f, 2.0f);
    MeshData trunk = primitives::cylinder(0.16f, height, 10);
    for (Vertex& v : trunk.vertices) {
        float t = (v.position.y + height * 0.5f) / height;
        v.position.x *= 1.3f - 0.5f * t;
        v.position.z *= 1.3f - 0.5f * t;
    }
    gradient(trunk, bark * 0.6f, bark);
    m.append(trunk, place(Vec3{0, height * 0.5f, 0}, Vec3{1, 1, 1}));

    MeshData canopy;
    const int puffs = random.range(3, 5);
    const Vec3 tone = jitter(random, leaves, 0.08f);
    for (int i = 0; i < puffs; ++i) {
        float r = random.uniform(0.9f, 1.2f) * (i == 0 ? 1.3f : 1.0f);
        float a = random.uniform(0.0f, 2.0f * PI), d = i == 0 ? 0.0f : random.uniform(0.45f, 0.8f);
        canopy.append(blob(random, r, 2, 0.18f),
                      place(Vec3{std::cos(a) * d, height + 0.4f + random.uniform(-0.2f, 0.5f) + (i == 0 ? 0.35f : 0.0f),
                                 std::sin(a) * d},
                            Vec3{1, 0.88f, 1}));
    }
    gradient(canopy, tone * 0.45f, tone * 1.15f);
    m.append(canopy, Mat4::identity());
    return m;
}

MeshData pine(Random& random, const Vec3& needles, const Vec3& bark) {
    MeshData m;
    m.name = "pine";
    const float height = random.uniform(3.0f, 4.4f);
    MeshData trunk = primitives::cylinder(0.14f, height * 0.4f, 8);
    gradient(trunk, bark * 0.6f, bark);
    m.append(trunk, place(Vec3{0, height * 0.2f, 0}, Vec3{1, 1, 1}));
    const int tiers = 3;
    const Vec3 tone = jitter(random, needles, 0.08f);
    for (int i = 0; i < tiers; ++i) {
        float t = static_cast<float>(i) / tiers;
        float r = 1.25f * (1.0f - t * 0.55f), h = height * 0.42f;
        MeshData tier = primitives::cone(r, h, 14);
        gradient(tier, mixColor(tone * 0.4f, tone, t * 0.5f), tone * (1.05f + t * 0.15f));
        m.append(tier, place(Vec3{0, height * (0.32f + t * 0.42f) + h * 0.5f, 0}, Vec3{1, 1, 1}, random.uniform(0, PI)));
    }
    return m;
}

MeshData bush(Random& random, const Vec3& leaves) {
    MeshData m;
    m.name = "bush";
    const int puffs = random.range(2, 4);
    for (int i = 0; i < puffs; ++i) {
        float r = random.uniform(0.35f, 0.55f), a = random.uniform(0.0f, 2.0f * PI);
        float d = i == 0 ? 0.0f : random.uniform(0.3f, 0.5f);
        m.append(blob(random, r, 2, 0.2f), place(Vec3{std::cos(a) * d, r * 0.75f, std::sin(a) * d}, Vec3{1, 0.85f, 1}));
    }
    m.recomputeBounds();
    const Vec3 tone = jitter(random, leaves, 0.1f);
    gradient(m, tone * 0.45f, tone * 1.15f);
    return m;
}

MeshData rock(Random& random, const Vec3& color) {
    MeshData m = primitives::icosphere(0.5f, 2);
    displace(m, 0.16f, 2.2f, random.next());
    for (Vertex& v : m.vertices) v.position.y = std::max(v.position.y * 0.62f, -0.12f);
    m.recomputeNormals();
    m.recomputeBounds();
    const Vec3 tone = jitter(random, color, 0.06f);
    gradient(m, tone * 0.6f, tone * 1.1f);
    m.name = "rock";
    return m;
}

MeshData flower(Random& random, const Vec3& petals) {
    MeshData m;
    m.name = "flower";
    const float h = random.uniform(0.25f, 0.45f);
    MeshData stem = primitives::cylinder(0.015f, h, 5);
    m.append(stem, place(Vec3{0, h * 0.5f, 0}, Vec3{1, 1, 1}), packColor(Vec3{0.25f, 0.55f, 0.18f}));
    MeshData petal = primitives::icosphere(0.05f, 1);
    for (int i = 0; i < 5; ++i) {
        float a = static_cast<float>(i) / 5.0f * 2.0f * PI;
        m.append(petal, place(Vec3{std::cos(a) * 0.05f, h, std::sin(a) * 0.05f}, Vec3{1.1f, 0.45f, 1.1f}),
                 packColor(petals));
    }
    m.append(primitives::icosphere(0.035f, 1), place(Vec3{0, h + 0.012f, 0}, Vec3{1, 0.7f, 1}),
             packColor(Vec3{1.0f, 0.8f, 0.25f}));
    return m;
}

MeshData grassTuft(Random& random, const Vec3& base, const Vec3& tip, int blades) {
    MeshData m;
    m.name = "grass";
    for (int i = 0; i < blades; ++i) {
        float a = random.uniform(0.0f, 2.0f * PI), lean = random.uniform(0.05f, 0.25f);
        float h = random.uniform(0.18f, 0.36f), w = random.uniform(0.025f, 0.04f);
        Vec3 root{std::cos(a) * random.uniform(0.0f, 0.12f), 0, std::sin(a) * random.uniform(0.0f, 0.12f)};
        Vec3 side{-std::sin(a) * w, 0, std::cos(a) * w};
        Vec3 top = root + Vec3{std::cos(a) * lean * h, h, std::sin(a) * lean * h};
        const uint32_t b = static_cast<uint32_t>(m.vertices.size());
        const Vec3 up{0, 1, 0};
        const Vec3 c = jitter(random, tip, 0.08f);
        m.vertices.push_back(Vertex{root - side, up, Vec2{0, 0}, packColor(base)});
        m.vertices.push_back(Vertex{root + side, up, Vec2{1, 0}, packColor(base)});
        m.vertices.push_back(Vertex{top, up, Vec2{0.5f, 1}, packColor(c)});
        m.indices.insert(m.indices.end(), {b, b + 1, b + 2, b, b + 2, b + 1});
    }
    m.recomputeBounds();
    return m;
}

MeshData cloud(Random& random) {
    MeshData m;
    m.name = "cloud";
    const int puffs = random.range(4, 7);
    for (int i = 0; i < puffs; ++i) {
        float x = (static_cast<float>(i) - static_cast<float>(puffs - 1) * 0.5f) * random.uniform(0.9f, 1.3f);
        float r = random.uniform(1.0f, 1.8f) * (1.0f - std::fabs(x) * 0.08f);
        m.append(blob(random, r, 2, 0.1f), place(Vec3{x, r * 0.35f, random.uniform(-0.6f, 0.6f)}, Vec3{1, 0.8f, 1}));
    }
    for (Vertex& v : m.vertices) v.position.y = std::max(v.position.y, 0.0f);
    m.recomputeNormals();
    m.recomputeBounds();
    gradient(m, Vec3{0.82f, 0.86f, 0.95f}, Vec3{1, 1, 1});
    return m;
}

MeshData chair(const Vec3& wood, const Vec3& cushion) {
    MeshData m;
    m.name = "chair";
    const uint32_t light = packColor(wood), dark = packColor(wood * 0.72f);
    MeshData leg = primitives::cylinder(0.05f, 0.4f, 10);
    for (Vertex& v : leg.vertices) {
        const float t = (v.position.y + 0.2f) / 0.4f;
        v.position.x *= 0.8f + 0.25f * t;
        v.position.z *= 0.8f + 0.25f * t;
    }
    for (int i = 0; i < 4; ++i) {
        const float x = (i & 1) ? 0.33f : -0.33f, z = (i & 2) ? 0.33f : -0.33f;
        m.append(leg, place(Vec3{x, 0.2f, z}, Vec3{1, 1, 1}), dark);
    }
    m.append(primitives::cube(1.0f), place(Vec3{0, 0.43f, 0}, Vec3{0.86f, 0.08f, 0.86f}), light);
    MeshData pad = primitives::icosphere(0.5f, 3);
    for (Vertex& v : pad.vertices) v.position.y = std::max(v.position.y, -0.1f);
    pad.recomputeNormals();
    m.append(pad, place(Vec3{0, 0.47f, 0.02f}, Vec3{0.74f, 0.1f, 0.72f}), packColor(cushion));
    for (int side = 0; side < 2; ++side) {
        const float x = side ? 0.33f : -0.33f;
        m.append(primitives::cylinder(0.045f, 0.62f, 10), place(Vec3{x, 0.78f, -0.36f}, Vec3{1, 1, 1}), dark);
    }
    m.append(primitives::capsule(0.07f, 0.76f, 16, 8),
             composeTRS(Vec3{0, 1.1f, -0.36f}, Quat::axisAngle(Vec3{0, 0, 1}, PI * 0.5f), Vec3{1, 1, 1}), light);
    m.append(primitives::cube(1.0f), place(Vec3{0, 0.8f, -0.36f}, Vec3{0.62f, 0.3f, 0.05f}), light);
    m.recomputeBounds();
    return m;
}

MeshData beanBody(float radius, float height) {
    MeshData m;
    m.name = "bean";
    const int rings = 48, segments = 40;
    for (int i = 0; i <= rings; ++i) {
        const float t = static_cast<float>(i) / rings;
        const float y = t * height;
        const float s = std::fabs(2.0f * t - 1.0f);
        const float r = radius * std::pow(std::max(1.0f - std::pow(s, 2.6f), 0.0f), 1.0f / 2.6f) * (1.06f - 0.1f * t);
        for (int j = 0; j <= segments; ++j) {
            const float a = static_cast<float>(j) / segments * 2.0f * PI;
            m.vertices.push_back(Vertex{Vec3{std::sin(a) * r, y, std::cos(a) * r}, Vec3{0, 1, 0},
                                        Vec2{static_cast<float>(j) / segments, t}});
        }
    }
    const uint32_t stride = segments + 1;
    for (uint32_t i = 0; i < static_cast<uint32_t>(rings); ++i)
        for (uint32_t j = 0; j < static_cast<uint32_t>(segments); ++j) {
            uint32_t a = i * stride + j, b = a + 1, c = a + stride, d = c + 1;
            m.indices.insert(m.indices.end(), {a, b, c, b, d, c});
        }
    m.recomputeNormals();
    for (int j = 0; j <= segments; ++j) {
        m.vertices[static_cast<size_t>(j)].normal = Vec3{0, -1, 0};
        m.vertices[static_cast<size_t>(rings) * stride + static_cast<size_t>(j)].normal = Vec3{0, 1, 0};
    }
    for (int i = 0; i <= rings; ++i) {
        Vertex& first = m.vertices[static_cast<size_t>(i) * stride];
        Vertex& last = m.vertices[static_cast<size_t>(i) * stride + segments];
        first.normal = last.normal = normalize(first.normal + last.normal);
    }
    m.recomputeBounds();
    return m;
}

MeshData eye(float radius) {
    MeshData m;
    m.name = "eye";
    m.append(primitives::sphere(radius, 20, 14), Mat4::identity(), packColor(Vec3{1, 1, 1}));
    m.append(primitives::sphere(radius * 0.6f, 20, 14), place(Vec3{0, 0, radius * 0.84f}, Vec3{1, 1, 0.32f}),
             packColor(Vec3{0.04f, 0.04f, 0.06f}));
    m.append(primitives::sphere(radius * 0.2f, 12, 8),
             place(Vec3{radius * 0.2f, radius * 0.24f, radius * 1.0f}, Vec3{1, 1, 0.3f}), packColor(Vec3{1, 1, 1}));
    return m;
}

}  // namespace chairs::kit
