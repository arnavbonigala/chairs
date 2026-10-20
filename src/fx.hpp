#pragma once
#include "meadow.hpp"
#include "scene/particles.hpp"

namespace chairs {

class Effects {
public:
    ParticleSystem confetti;
    ParticleSystem sparks;

    Effects();
    void celebrate(const Vec3& at);
    void launch(Meadow& meadow, Bean& bean, float power = 1.0f);
    void trail(const Vec3& at);
    void twinkle(const Vec3& at);
    void puff(const Vec3& at, const Vec3& color, int count, float speed, float size);
    bool fly(Meadow& meadow, Bean& bean);
    void update(float dt);

private:
    Random random_{77};
};

}  // namespace chairs
