#include "meadow.hpp"

#include "core/noise.hpp"
#include "kit.hpp"
#include "names.hpp"
#include "scene/components.hpp"

namespace chairs {

namespace {

constexpr float CLEARING = 13.0f;
constexpr float WORLD = 240.0f;

float smooth(float a, float b, float x) {
    float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

Vec3 mixColor(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * std::clamp(t, 0.0f, 1.0f); }

Material material(const char* name, float roughness, float wind = 0.0f, Vec3 emissive = {}, float ink = 1.0f) {
    Material m;
    m.ink = ink;
    m.name = name;
    m.albedo = Vec3{1, 1, 1};
    m.roughness = roughness;
    m.wind = wind;
    m.emissive = emissive;
    return m;
}

}  // namespace

float Meadow::height(float x, float z) const {
    const float r = std::sqrt(x * x + z * z);
    const float rolling = noise::fbm(Vec3{x * 0.025f, 0.0f, z * 0.025f}, 4, seed_) * 3.0f;
    const float ring = smooth(35.0f, 95.0f, r) * (10.0f + 8.0f * noise::fbm(Vec3{x * 0.012f, 3.0f, z * 0.012f}, 3, seed_ + 7));
    return smooth(CLEARING, CLEARING + 18.0f, r) * (rolling + 1.0f) + ring;
}

Entity Meadow::place(uint32_t mesh, uint32_t material, const Vec3& position, float yaw, float scale, uint32_t tint) {
    Transform t;
    t.position = position;
    t.rotation = Quat::axisAngle(Vec3{0, 1, 0}, yaw);
    t.scale = Vec3{scale, scale, scale};
    Entity e = scene.create(t);
    scene.world.add<Renderable>(e, Renderable{mesh, material, 1, tint});
    scene.world.add<CullBounds>(e, cullBoundsFor(assets.mesh(mesh).bounds));
    return e;
}

void Meadow::lineup(int columns) {
    const int cols = std::max(1, std::min(columns, static_cast<int>(beans.size())));
    const int rows = (static_cast<int>(beans.size()) + cols - 1) / cols;
    for (size_t i = 0; i < beans.size(); ++i) {
        Transform& t = scene.world.get<Transform>(beans[i].root);
        const int col = static_cast<int>(i) % cols, row = static_cast<int>(i) / cols;
        t.position = Vec3{(static_cast<float>(col) - static_cast<float>(cols - 1) * 0.5f) * 1.3f, 0.0f,
                          (static_cast<float>(rows - 1) * 0.5f - static_cast<float>(row)) * 1.5f};
        beans[i].target = t.position;
        beans[i].yaw = 0.0f;
        beans[i].idle = true;
        beans[i].nextHop = random_.uniform(1.0f, 8.0f);
    }
}

void Meadow::build(uint32_t seed, int beanCount) {
    seed_ = seed;
    random_ = Random(seed);
    Random& rng = random_;
    const Palette& p = palette;

    const uint32_t groundMat = assets.addMaterial(material("ground", 0.95f));
    const uint32_t foliageMat = assets.addMaterial(material("foliage", 1.0f, 0.006f));
    const uint32_t grassMat = assets.addMaterial(material("grass", 1.0f, 1.3f, {}, 0.0f));
    const uint32_t flowerMat = assets.addMaterial(material("flower", 1.0f, 0.7f, {}, 0.0f));
    const uint32_t rockMat = assets.addMaterial(material("rock", 0.9f));
    Material cloud = material("cloud", 1.0f, 0.0f, Vec3{0.95f, 0.97f, 1.0f}, 0.4f);
    cloud.albedo = Vec3{0.35f, 0.35f, 0.38f};
    const uint32_t cloudMat = assets.addMaterial(cloud);
    beanMaterial_ = assets.addMaterial(material("bean", 0.3f));
    eyeMaterial_ = assets.addMaterial(material("eye", 0.15f));

    kit::Terrain terrain;
    terrain.size = WORLD;
    terrain.resolution = 240;
    terrain.height = [this](float x, float z) { return height(x, z); };
    terrain.color = [&](const Vec3& pos, const Vec3& n) {
        const float r = std::sqrt(pos.x * pos.x + pos.z * pos.z);
        const float patch = noise::fbm(Vec3{pos.x * 0.08f, 0.0f, pos.z * 0.08f}, 3, seed + 11) * 0.5f + 0.5f;
        Vec3 c = mixColor(p.grassDark, p.grass, smooth(0.2f, 0.5f, patch));
        c = mixColor(c, p.grassLight, smooth(0.55f, 0.8f, patch));
        c = mixColor(c, p.grassDark, smooth(0.9f, 0.75f, n.y) * 0.6f);
        const float edge = 7.5f + noise::value(Vec3{pos.x * 0.4f, 0.0f, pos.z * 0.4f}, seed + 3) * 0.8f;
        return mixColor(c, p.dirt, smooth(edge + 0.6f, edge - 0.6f, r));
    };
    place(assets.addMesh(kit::terrain(terrain)), groundMat, Vec3{}, 0.0f, 1.0f);

    std::vector<uint32_t> trees, pines, bushes, rocks, flowers, grass, cloudMeshes;
    for (int i = 0; i < 6; ++i) trees.push_back(assets.addMesh(kit::tree(rng, i == 5 ? p.leavesAutumn : p.leaves, p.bark)));
    for (int i = 0; i < 4; ++i) pines.push_back(assets.addMesh(kit::pine(rng, p.needles, p.bark)));
    for (int i = 0; i < 4; ++i) bushes.push_back(assets.addMesh(kit::bush(rng, p.leaves)));
    for (int i = 0; i < 4; ++i) rocks.push_back(assets.addMesh(kit::rock(rng, p.rock)));
    const Vec3 petals[5] = {{1, 1, 1}, {1.0f, 0.45f, 0.6f}, {1.0f, 0.85f, 0.2f}, {0.6f, 0.5f, 1.0f}, {1.0f, 0.4f, 0.3f}};
    for (const Vec3& c : petals) flowers.push_back(assets.addMesh(kit::flower(rng, c)));
    for (int i = 0; i < 4; ++i) grass.push_back(assets.addMesh(kit::grassTuft(rng, p.grassDark, p.grassLight)));
    for (int i = 0; i < 5; ++i) cloudMeshes.push_back(assets.addMesh(kit::cloud(rng)));

    auto scatter = [&](float inner, float outer) {
        float a = rng.uniform(0.0f, 2.0f * PI), r = std::sqrt(rng.uniform(inner * inner, outer * outer));
        return Vec3{std::cos(a) * r, 0.0f, std::sin(a) * r};
    };
    auto grove = [&](const Vec3& at) { return noise::fbm(Vec3{at.x * 0.04f, 5.0f, at.z * 0.04f}, 2, seed + 21); };
    auto ground = [&](Vec3 at) {
        at.y = height(at.x, at.z) - 0.05f;
        return at;
    };

    for (int placed = 0, tries = 0; placed < 260 && tries < 20000; ++tries) {
        Vec3 at = scatter(CLEARING + 8.0f, 100.0f);
        if (grove(at) < 0.0f) continue;
        const float r = length(at);
        const bool pine = r > 50.0f ? rng.chance(0.65f) : rng.chance(0.15f);
        const auto& set = pine ? pines : trees;
        place(set[rng.below(static_cast<uint32_t>(set.size()))], foliageMat, ground(at), rng.uniform(0, 2 * PI),
              rng.uniform(0.85f, 1.35f) * (1.0f + r * 0.004f));
        ++placed;
    }
    for (int i = 0; i < 140; ++i) {
        Vec3 at = scatter(CLEARING + 2.0f, 70.0f);
        place(bushes[rng.below(4)], foliageMat, ground(at), rng.uniform(0, 2 * PI), rng.uniform(0.8f, 1.5f));
    }
    for (int i = 0; i < 70; ++i) {
        Vec3 at = scatter(CLEARING, 90.0f);
        place(rocks[rng.below(4)], rockMat, ground(at), rng.uniform(0, 2 * PI), rng.uniform(0.6f, 2.2f));
    }
    for (int i = 0; i < 1400; ++i) {
        Vec3 at = scatter(9.0f, 55.0f);
        if (noise::fbm(Vec3{at.x * 0.1f, 9.0f, at.z * 0.1f}, 2, seed + 31) < 0.05f) continue;
        place(flowers[rng.below(5)], flowerMat, ground(at), rng.uniform(0, 2 * PI), rng.uniform(0.8f, 1.3f));
    }
    for (int i = 0; i < 26000; ++i) {
        Vec3 at = scatter(8.6f, 60.0f);
        place(grass[rng.below(4)], grassMat, ground(at) + Vec3{0, 0.04f, 0}, rng.uniform(0, 2 * PI),
              rng.uniform(0.8f, 1.6f));
    }
    for (int i = 0; i < 18; ++i) {
        Vec3 at = scatter(70.0f, 170.0f);
        at.y = rng.uniform(26.0f, 40.0f);
        clouds_.push_back(Cloud{place(cloudMeshes[rng.below(5)], cloudMat, at, rng.uniform(-0.3f, 0.3f), rng.uniform(1.6f, 3.0f)),
                                rng.uniform(0.6f, 1.4f)});
    }

    beanMesh_ = assets.addMesh(kit::beanBody());
    eyeMesh_ = assets.addMesh(kit::eye());
    for (int i = 0; i < beanCount; ++i) {
        float a = static_cast<float>(i) / static_cast<float>(beanCount) * 2.0f * PI;
        addBean(i, Vec3{std::cos(a) * 4.0f, 0.0f, std::sin(a) * 4.0f});
    }

    Entity sun = scene.create(Transform{});
    Light light;
    light.kind = static_cast<uint32_t>(LightKind::Directional);
    light.direction = normalize(Vec3{-0.55f, -0.7f, -0.45f});
    light.color = p.sun;
    light.intensity = 2.2f;
    scene.world.add<Light>(sun, light);
    scene.updateTransforms(nullptr);
}

Vec3 Meadow::beanColor(int id) {
    const float hue = std::fmod(static_cast<float>(id) * 0.618034f, 1.0f) * 6.0f;
    const float sat = 0.6f + 0.22f * static_cast<float>((id * 7) % 3) / 2.0f;
    const float value = 0.96f - 0.12f * static_cast<float>((id * 5) % 4) / 3.0f;
    const float x = 1.0f - std::fabs(std::fmod(hue, 2.0f) - 1.0f);
    Vec3 rgb = hue < 1 ? Vec3{1, x, 0} : hue < 2 ? Vec3{x, 1, 0} : hue < 3 ? Vec3{0, 1, x}
             : hue < 4 ? Vec3{0, x, 1} : hue < 5 ? Vec3{x, 0, 1} : Vec3{1, 0, x};
    return (Vec3{1, 1, 1} * (1.0f - sat) + rgb * sat) * value;
}

const char* Meadow::beanName(int id) { return NAMES[((id % NAME_COUNT) + NAME_COUNT) % NAME_COUNT]; }

void Meadow::addBean(int id, const Vec3& position) {
    const Vec3 color = beanColor(id);
    Bean bean;
    bean.id = id;
    Transform rootT;
    rootT.position = position;
    bean.root = scene.create(rootT);
    bean.body = scene.create(Transform{}, bean.root);
    scene.world.add<Renderable>(bean.body, Renderable{beanMesh_, beanMaterial_, 1, packTint(color)});
    scene.world.add<CullBounds>(bean.body, cullBoundsFor(assets.mesh(beanMesh_).bounds));
    for (int side = 0; side < 2; ++side) {
        Transform t;
        t.position = Vec3{side ? 0.14f : -0.14f, 0.98f, 0.31f};
        t.rotation = Quat::axisAngle(Vec3{0, 1, 0}, side ? 0.2f : -0.2f);
        bean.eyes[side] = scene.create(t, bean.body);
        scene.world.add<Renderable>(bean.eyes[side], Renderable{eyeMesh_, eyeMaterial_, 1, 0});
        scene.world.add<CullBounds>(bean.eyes[side], cullBoundsFor(assets.mesh(eyeMesh_).bounds));
    }
    bean.target = position;
    bean.yaw = random_.uniform(0, 2 * PI);
    bean.nextBlink = random_.uniform(1.0f, 4.0f);
    bean.rest = random_.uniform(0.0f, 1.5f);
    bean.phase = random_.uniform(0.0f, 2.0f * PI);
    bean.nextLook = random_.uniform(0.5f, 3.0f);
    beans.push_back(bean);
}

void Meadow::updateBean(Bean& bean, float dt) {
    Transform& root = scene.world.get<Transform>(bean.root);
    Transform& body = scene.world.get<Transform>(bean.body);
    Vec3 to = bean.target - root.position;
    to.y = 0;
    const float dist = length(to);

    if (bean.idle) {
        bean.nextHop -= dt;
        if (bean.nextHop <= 0.0f && bean.hop <= 0.0f) {
            bean.hop = 1.0f;
            bean.nextHop = random_.uniform(4.0f, 12.0f);
        }
    } else if (bean.hop <= 0.0f) {
        bean.rest -= dt;
        if (dist < 0.3f && bean.rest <= 0.0f) {
            float a = random_.uniform(0, 2 * PI), r = std::sqrt(random_.unit()) * 9.0f;
            bean.target = Vec3{std::cos(a) * r, 0.0f, std::sin(a) * r};
            bean.rest = random_.uniform(0.3f, 2.0f);
        } else if (dist >= 0.3f && bean.rest <= 0.0f) {
            bean.hop = 1.0f;
        }
    }

    const float desired = std::atan2(to.x, to.z);
    float turn = desired - bean.yaw;
    while (turn > PI) turn -= 2 * PI;
    while (turn < -PI) turn += 2 * PI;
    if (dist > 0.05f && !bean.idle) bean.yaw += turn * std::min(1.0f, dt * 8.0f);
    root.rotation = Quat::axisAngle(Vec3{0, 1, 0}, bean.yaw);

    float lift = 0, squash = 0;
    if (bean.hop > 0.0f) {
        const float duration = 0.36f;
        bean.hop -= dt / duration;
        const float t = 1.0f - std::max(bean.hop, 0.0f);
        lift = 4.0f * t * (1.0f - t) * 0.32f;
        squash = -0.12f * std::sin(t * PI);
        if (dist > 0.01f) root.position += to * (std::min(dist, 2.4f * dt) / dist);
        if (bean.hop <= 0.0f) {
            bean.land = 1.0f;
            bean.rest = random_.uniform(0.0f, 0.12f);
        }
    }
    if (bean.land > 0.0f) {
        bean.land = std::max(0.0f, bean.land - dt * 5.0f);
        squash += 0.2f * std::sin(bean.land * PI);
    }
    root.position.y = height(root.position.x, root.position.z);
    body.position = Vec3{0, lift, 0};
    squash += 0.018f * std::sin(time_ * 2.4f + bean.phase);
    body.scale = Vec3{1.0f + squash * 0.6f, 1.0f - squash, 1.0f + squash * 0.6f};

    bean.nextBlink -= dt;
    if (bean.nextBlink <= 0.0f) {
        bean.blink = 1.0f;
        bean.nextBlink = random_.uniform(1.5f, 5.0f);
    }
    bean.blink = std::max(0.0f, bean.blink - dt / 0.14f);
    const float lid = 1.0f - 0.9f * std::sin(bean.blink * PI);

    bean.nextLook -= dt;
    if (bean.nextLook <= 0.0f) {
        const bool ahead = random_.chance(0.4f);
        bean.lookTarget[0] = ahead ? 0.0f : random_.uniform(-0.6f, 0.6f);
        bean.lookTarget[1] = ahead ? 0.0f : random_.uniform(-0.25f, 0.2f);
        bean.nextLook = random_.uniform(0.8f, 3.5f);
    }
    const float follow = std::min(1.0f, dt * 14.0f);
    for (int k = 0; k < 2; ++k) bean.look[k] += (bean.lookTarget[k] - bean.look[k]) * follow;
    for (int side = 0; side < 2; ++side) {
        Transform& eye = scene.world.get<Transform>(bean.eyes[side]);
        eye.scale = Vec3{1, lid, 1};
        eye.rotation = Quat::axisAngle(Vec3{0, 1, 0}, (side ? 0.2f : -0.2f) + bean.look[0]) *
                       Quat::axisAngle(Vec3{1, 0, 0}, -bean.look[1]);
    }
}

void Meadow::update(float dt) {
    time_ += dt;
    for (Bean& bean : beans) updateBean(bean, dt);
    for (Cloud& cloud : clouds_) {
        Transform& t = scene.world.get<Transform>(cloud.entity);
        t.position.x += cloud.speed * dt;
        if (t.position.x > 150.0f) t.position.x -= 300.0f;
    }
    scene.updateTransforms(nullptr);
}

}  // namespace chairs
