#include "fx.hpp"

namespace chairs {

Effects::Effects() {
    confetti.floor = 0.01f;
    confetti.drag = 1.6f;
    sparks.additive = true;
    sparks.round = true;
    sparks.drag = 1.2f;
    sparks.floor = 0.0f;
}

void Effects::celebrate(const Vec3& at) {
    ParticleBurst c;
    c.position = at + Vec3{0, 1.6f, 0};
    c.velocity = Vec3{0, 1, 0};
    c.speed = 7.0f;
    c.spread = 0.9f;
    c.count = 1600;
    c.life = 5.0f;
    c.size = 0.05f;
    c.spin = 12.0f;
    c.colorCount = 4;
    c.colors[0] = Vec3{0.95f, 0.3f, 0.35f};
    c.colors[1] = Vec3{0.3f, 0.75f, 0.95f};
    c.colors[2] = Vec3{1.0f, 0.82f, 0.25f};
    c.colors[3] = Vec3{0.45f, 0.9f, 0.5f};
    confetti.burst(c, random_);
    puff(c.position, Vec3{1.0f, 0.7f, 0.3f}, 300, 6.0f, 0.045f);
}

void Effects::puff(const Vec3& at, const Vec3& color, int count, float speed, float size) {
    ParticleBurst p;
    p.position = at;
    p.velocity = Vec3{0, 1, 0};
    p.speed = speed;
    p.spread = 2.5f;
    p.count = count;
    p.life = 0.8f;
    p.size = size;
    p.spin = 0.0f;
    p.glow = 5.0f;
    p.colors[0] = color;
    sparks.burst(p, random_);
}

void Effects::launch(Meadow& meadow, Bean& bean, float power) {
    Vec3 at = meadow.beanPosition(bean);
    Vec3 out{at.x, 0, at.z};
    out = length(out) > 0.01f ? normalize(out) : Vec3{0, 0, 1};
    bean.flying = true;
    bean.velocity = out * (5.5f * power) + Vec3{0, 16.0f * power, 0} + Vec3{-out.z, 0, out.x} * 1.5f;
    bean.spinAxis = normalize(Vec3{-out.z, 0.3f, out.x});
    bean.spinRate = 15.0f;
    bean.gazeSet = true;
    bean.gaze[0] = 0.0f;
    bean.gaze[1] = 0.25f;
    puff(at + Vec3{0, 0.2f, 0}, Vec3{1.0f, 0.85f, 0.55f}, 400, 5.0f, 0.07f);
}

void Effects::trail(const Vec3& at) {
    ParticleBurst t;
    t.position = at;
    t.speed = 0.6f;
    t.spread = 3.0f;
    t.count = 6;
    t.life = 0.9f;
    t.size = 0.08f;
    t.spin = 0.0f;
    t.glow = 7.0f;
    t.colors[0] = Vec3{1.0f, 0.9f, 0.5f};
    sparks.burst(t, random_);
}

void Effects::twinkle(const Vec3& at) {
    ParticleBurst s;
    s.position = at;
    s.speed = 9.0f;
    s.spread = 3.0f;
    s.count = 500;
    s.life = 1.0f;
    s.size = 0.35f;
    s.spin = 0.0f;
    s.glow = 12.0f;
    s.colors[0] = Vec3{1.0f, 0.95f, 0.7f};
    sparks.burst(s, random_);
}

bool Effects::fly(Meadow& meadow, Bean& bean) {
    if (!bean.flying || bean.hidden) return false;
    const Vec3 at = meadow.beanPosition(bean);
    trail(at + Vec3{0, 0.75f, 0});
    if (at.y > 34.0f) {
        twinkle(at);
        meadow.hide(bean);
        return true;
    }
    return false;
}

void Effects::update(float dt) {
    confetti.update(dt);
    sparks.update(dt);
}

}  // namespace chairs
