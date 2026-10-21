#include "view.hpp"

#include <algorithm>

#include "kit.hpp"
#include "scene/components.hpp"

namespace chairs {

namespace {

Vec3 flat(Vec3 v) {
    v.y = 0;
    return v;
}

float ease(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

}  // namespace

GameView::GameView(Meadow& meadow, Game& game) : meadow_(meadow), game_(game) {
    chairMesh_ = meadow.assets.addMesh(kit::chair(Vec3{0.78f, 0.52f, 0.32f}, Vec3{0.92f, 0.3f, 0.32f}));
    Material wood;
    wood.name = "chair";
    wood.roughness = 0.85f;
    chairMaterial_ = meadow.assets.addMaterial(wood);
    layout();
    for (size_t i = 0; i < game.beans.size(); ++i) {
        Bean& b = meadow.beans[i];
        b.driven = true;
        b.drive = game.beans[i].position;
        b.driveYaw = b.yaw = game.beans[i].yaw;
    }
}

void GameView::layout() {
    for (Prop& p : props_) p.target = 0.0f;
    for (const Chair& c : game_.chairs) {
        Entity e = meadow_.place(chairMesh_, chairMaterial_, c.position, c.yaw, 0.0f);
        props_.push_back(Prop{e, 0.0f, 1.0f});
        if (round_ > 0) fx.puff(c.position + Vec3{0, 0.5f, 0}, Vec3{1.0f, 0.92f, 0.75f}, 120, 2.5f, 0.05f);
    }
    if (round_ == 0)
        for (Prop& p : props_) p.scale = 1.0f;
    round_ = game_.round;
}

void GameView::sync(float dt) {
    if (game_.round != round_) layout();
    for (Prop& p : props_) {
        const float speed = p.target > p.scale ? 4.0f : 5.0f;
        p.scale = p.target > p.scale ? std::min(p.target, p.scale + dt * speed) : std::max(p.target, p.scale - dt * speed);
        const float t = p.scale;
        const float s = t <= 0.0f ? 0.0f : ease(t) * (1.0f + 0.25f * std::sin(t * PI));
        meadow_.scene.world.get<Transform>(p.entity).scale = Vec3{s, s, s};
        meadow_.scene.world.get<Renderable>(p.entity).visible = s > 0.001f ? 1u : 0u;
    }

    for (const Event& e : game_.events) {
        if ((e.kind == Event::Hit || e.kind == Event::KnockedOff || e.kind == Event::Sit) && e.a >= 0) {
            interest_ = flat(game_.beans[static_cast<size_t>(e.a)].position);
            interestTime_ = game_.time;
        }
        Bean* a = e.a >= 0 ? &meadow_.beans[static_cast<size_t>(e.a)] : nullptr;
        Bean* b = e.b >= 0 ? &meadow_.beans[static_cast<size_t>(e.b)] : nullptr;
        switch (e.kind) {
            case Event::Hit: {
                a->tiltVel -= 11.0f;
                a->gazeSet = true;
                a->gaze[0] = 0.0f;
                a->gaze[1] = 0.3f;
                b->tiltVel += 9.0f;
                const Vec3 at = (game_.beans[static_cast<size_t>(e.a)].position + game_.beans[static_cast<size_t>(e.b)].position) * 0.5f;
                fx.puff(at + Vec3{0, 0.9f, 0}, Vec3{1.0f, 1.0f, 0.95f}, 160, 3.5f, 0.06f);
                break;
            }
            case Event::Shove:
                a->tiltVel += 6.0f;
                break;
            case Event::KnockedOff:
                fx.puff(game_.beans[static_cast<size_t>(e.a)].position + Vec3{0, 0.8f, 0}, Vec3{1.0f, 0.6f, 0.3f}, 260, 4.5f, 0.07f);
                break;
            case Event::Sit:
                a->land = 1.0f;
                break;
            case Event::Locked: {
                a->land = 1.0f;
                const Chair& c = game_.chairs[static_cast<size_t>(e.b)];
                fx.puff(c.position + Vec3{0, 0.5f, 0}, Vec3{1.0f, 0.85f, 0.35f}, 90, 2.0f, 0.05f);
                break;
            }
            case Event::Eliminated:
                if (a && !a->flying) {
                    a->driven = false;
                    fx.launch(meadow_, *a, 1.6f);
                }
                break;
            case Event::Winner:
                if (a) fx.celebrate(meadow_.beanPosition(*a));
                cheer_ = 0.6f;
                break;
            default:
                break;
        }
    }
    game_.events.clear();

    for (size_t i = 0; i < game_.beans.size(); ++i) {
        Bean& b = meadow_.beans[i];
        const Contestant& c = game_.beans[i];
        if (b.flying) {
            fx.fly(meadow_, b);
            continue;
        }
        if (!c.alive || !b.driven) continue;
        b.drive = c.position;
        b.driveYaw = c.yaw;
        b.driveGrounded = c.grounded;
        if (b.gazeSet && c.stun <= 0.0f) b.gazeSet = false;
    }

    if (game_.phase == Game::Over) {
        overTime_ += dt;
        cheer_ -= dt;
        if (game_.winner >= 0 && cheer_ <= 0.0f) {
            Bean& w = meadow_.beans[static_cast<size_t>(game_.winner)];
            w.land = 1.0f;
            w.tiltVel += 4.0f;
            cheer_ = 0.5f;
        }
    }
    fx.update(dt);
}

bool GameView::camera(const std::string& shot, int focus, CameraKey& out) {
    const float t = game_.time;
    const float ring = std::max(game_.ring, 1.0f);
    const Vec3 up{0, 1, 0};
    if (shot == "wide") {
        const float a = 0.8f + t * 0.03f, r = ring + 8.0f;
        out = CameraKey{Vec3{std::sin(a) * r, 5.0f + ring * 0.4f, std::cos(a) * r}, Vec3{0, 0.5f, 0}, 40};
        return true;
    }
    if (shot == "top") {
        const float a = t * 0.04f;
        out = CameraKey{Vec3{std::sin(a) * 1.0f, 11.0f + ring * 1.5f, std::cos(a) * 1.0f}, Vec3{0, 0, 0}, 42};
        return true;
    }
    if (shot == "low") {
        const float a = 2.0f - t * 0.04f, r = ring + 5.5f;
        out = CameraKey{Vec3{std::sin(a) * r, 0.45f, std::cos(a) * r}, Vec3{0, 0.9f, 0}, 38};
        return true;
    }
    if (shot == "chairs") {
        const float a = 1.3f + t * 0.02f, r = ring + 5.6f;
        out = CameraKey{Vec3{std::sin(a) * r, 3.0f, std::cos(a) * r}, Vec3{0, 0.5f, 0}, 38};
        return true;
    }
    if (shot == "action") {
        const bool hot = game_.phase == Game::Scramble || t - interestTime_ < 2.5f;
        const Vec3 focus = hot ? interest_ * 0.8f : Vec3{};
        interestSmoothed_ = interestSmoothed_ + (focus - interestSmoothed_) * 0.012f;
        const float a = 0.6f + t * 0.05f, r = hot ? 4.4f : ring + 6.0f;
        if (actionRadius_ <= 0.0f) actionRadius_ = r;
        actionRadius_ += (r - actionRadius_) * 0.006f;
        const float radius = actionRadius_;
        out = CameraKey{interestSmoothed_ + Vec3{std::sin(a) * radius, 1.0f + radius * 0.3f, std::cos(a) * radius},
                        interestSmoothed_ + up * 0.7f, 40};
        return true;
    }
    if (shot == "follow") {
        const size_t who = static_cast<size_t>(std::clamp(focus, 0, static_cast<int>(game_.beans.size()) - 1));
        Bean& bean = meadow_.beans[who];
        const Vec3 p = flat(meadow_.beanPosition(bean));
        if (!smoothedSet_) {
            smoothed_ = p;
            look_ = p + up;
            smoothedSet_ = true;
        }
        if (bean.flying || bean.hidden) {
            const Vec3 sky = meadow_.beanPosition(bean) + up * 0.75f;
            if (!bean.hidden) look_ = look_ + (sky - look_) * 0.05f;
            out = CameraKey{eye_, look_, 44};
            return true;
        }
        smoothed_ = smoothed_ + (p - smoothed_) * 0.03f;
        const Vec3 radial = length(smoothed_) > 0.01f ? normalize(smoothed_) : Vec3{0, 0, 1};
        const Vec3 side{-radial.z, 0, radial.x};
        eye_ = smoothed_ + radial * 3.6f + side * 1.6f + up * 1.6f;
        look_ = smoothed_ + up * 0.8f - radial * 0.5f;
        out = CameraKey{eye_, look_, 40};
        return true;
    }
    return false;
}

}  // namespace chairs
