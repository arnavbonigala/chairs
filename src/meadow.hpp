#pragma once
#include <vector>

#include "assets/mesh.hpp"
#include "core/random.hpp"
#include "scene/scene.hpp"

namespace chairs {

using namespace skein;

struct Palette {
    Vec3 grass{0.3f, 0.6f, 0.17f};
    Vec3 grassLight{0.5f, 0.76f, 0.24f};
    Vec3 grassDark{0.18f, 0.42f, 0.16f};
    Vec3 dirt{0.78f, 0.6f, 0.4f};
    Vec3 leaves{0.32f, 0.66f, 0.24f};
    Vec3 leavesAutumn{0.95f, 0.62f, 0.22f};
    Vec3 needles{0.16f, 0.46f, 0.32f};
    Vec3 bark{0.5f, 0.32f, 0.2f};
    Vec3 rock{0.66f, 0.64f, 0.62f};
    Vec3 skyZenith{0.12f, 0.4f, 0.92f};
    Vec3 skyHorizon{0.62f, 0.84f, 1.0f};
    Vec3 skyGround{0.5f, 0.66f, 0.42f};
    Vec3 sun{1.0f, 0.93f, 0.8f};
};

struct Bean {
    int id = 0;
    Entity root = NULL_ENTITY;
    Entity body = NULL_ENTITY;
    Entity eyes[2] = {NULL_ENTITY, NULL_ENTITY};
    Vec3 target;
    float yaw = 0;
    float hop = 0;
    float land = 0;
    float blink = 0;
    float nextBlink = 2;
    float rest = 0;
};

class Meadow {
public:
    Scene scene;
    Assets assets;
    Palette palette;
    std::vector<Bean> beans;

    void build(uint32_t seed, int beanCount);
    void update(float dt);
    void lineup();
    float height(float x, float z) const;
    static Vec3 beanColor(int id);
    static const char* beanName(int id);

private:
    struct Cloud {
        Entity entity;
        float speed;
    };

    Entity place(uint32_t mesh, uint32_t material, const Vec3& position, float yaw, float scale, uint32_t tint = 0);
    void addBean(int id, const Vec3& position);
    void updateBean(Bean& bean, float dt);

    std::vector<Cloud> clouds_;
    Random random_;
    uint32_t seed_ = 1;
    uint32_t beanMesh_ = 0, eyeMesh_ = 0, beanMaterial_ = 0, eyeMaterial_ = 0;
};

}  // namespace chairs
