#include "stage.hpp"

#include <algorithm>

#include "kit.hpp"
#include "scene/components.hpp"

namespace chairs {

namespace {

const std::vector<std::pair<std::string, Routine>>& scripts() {
    static const std::vector<std::pair<std::string, Routine>> all = {
        {"round", Routine{{0, 7, 4, 1, 11, 99}, {0.14f, 0.2f, 0.12f, 0.24f, 0.18f, 0.7f}, 5, 6.0f, 12.0f, 14.5f, false, 5}},
        {"final", Routine{{4, 11}, {0.2f, 0.16f}, 1, 5.0f, 0.0f, 12.0f, true, 1}},
    };
    return all;
}

Vec3 flat(Vec3 v) {
    v.y = 0;
    return v;
}

float ease(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

}  // namespace

const Routine* findRoutine(const std::string& name) {
    for (const auto& [key, script] : scripts())
        if (key == name) return &script;
    return nullptr;
}

std::vector<std::string> Stage::shots(const std::string& scene) {
    if (scene == "final") return {"final-wide", "final-faceoff", "final-kevin", "final-steve", "final-winner", "final-loser", "final-top"};
    return {"round-wide", "round-low", "round-follow", "round-chair", "round-loser", "round-top"};
}

Stage::Stage(Meadow& m, const Routine& s) : meadow(m), script(s) {
    const int n = std::max(script.chairs, 1);
    ring = n == 1 ? 0.0f : std::max(1.8f, static_cast<float>(n) * 1.7f / (2.0f * PI));
    walk = ring + 1.7f;
    const uint32_t mesh = meadow.assets.addMesh(kit::chair(Vec3{0.78f, 0.52f, 0.32f}, Vec3{0.92f, 0.3f, 0.32f}));
    Material wood;
    wood.name = "chair";
    wood.roughness = 0.85f;
    const uint32_t material = meadow.assets.addMaterial(wood);
    for (int i = 0; i < n; ++i) {
        Seat seat;
        const float a = static_cast<float>(i) / static_cast<float>(n) * 2.0f * PI + 0.3f;
        seat.outward = n == 1 ? Vec3{0, 0, 1} : Vec3{std::cos(a), 0, std::sin(a)};
        seat.position = seat.outward * ring;
        seat.position.y = meadow.height(seat.position.x, seat.position.z);
        seat.yaw = std::atan2(seat.outward.x, seat.outward.z);
        seat.entity = meadow.place(mesh, material, seat.position, seat.yaw, 1.0f);
        seats.push_back(seat);
    }
    for (size_t i = 0; i < meadow.beans.size() && i < script.beans.size(); ++i) {
        Player p;
        p.bean = i;
        p.angle = static_cast<float>(i) / static_cast<float>(script.beans.size()) * 2.0f * PI;
        p.delay = i < script.delays.size() ? script.delays[i] : 0.25f;
        Bean& b = meadow.beans[i];
        b.puppet = true;
        b.speed = 1.9f;
        b.hopTime = 0.3f;
        b.hopHeight = 0.16f;
        Transform& t = meadow.scene.world.get<Transform>(b.root);
        t.position = Vec3{std::cos(p.angle) * walk, 0, std::sin(p.angle) * walk};
        b.target = t.position;
        b.yaw = std::atan2(std::sin(p.angle), -std::cos(p.angle));
        players.push_back(p);
    }
}

void Stage::dash(Player& p) {
    Bean& b = bean(p);
    const Vec3 at = flat(meadow.beanPosition(b));
    const bool doomed = static_cast<int>(&p - players.data()) == script.doomed;
    auto claimed = [&](size_t c) {
        if (seats[c].occupant >= 0) return true;
        for (const Player& o : players)
            if (&o != &p && o.state == Player::Dash && o.chair == static_cast<int>(c)) return true;
        return false;
    };
    int best = -1;
    float bestDist = 1e9f;
    for (size_t c = 0; c < seats.size(); ++c) {
        if (seats[c].removing) continue;
        if (doomed ? !claimed(c) : seats[c].occupant >= 0) continue;
        if (std::find(p.tried.begin(), p.tried.end(), static_cast<int>(c)) != p.tried.end()) continue;
        const float d = length(flat(seats[c].position + seats[c].outward * 0.7f) - at);
        if (d < bestDist) {
            bestDist = d;
            best = static_cast<int>(c);
        }
    }
    if (doomed && !p.tried.empty()) best = -1;
    if (best < 0) {
        p.state = Player::Out;
        p.timer = 0.0f;
        b.target = at;
        b.gazeSet = true;
        loser = static_cast<int>(&p - players.data());
        return;
    }
    p.state = Player::Dash;
    p.chair = best;
    b.speed = 4.6f;
    b.hopTime = 0.24f;
    b.hopHeight = 0.24f;
    b.faceSet = false;
    b.gazeSet = true;
    b.gaze[0] = 0.0f;
    b.gaze[1] = -0.1f;
    b.target = seats[static_cast<size_t>(best)].position + seats[static_cast<size_t>(best)].outward * 0.7f;
}

void Stage::sit(Player& p, int chair) {
    Bean& b = bean(p);
    Seat& seat = seats[static_cast<size_t>(chair)];
    seat.occupant = static_cast<int>(&p - players.data());
    p.state = Player::Sit;
    p.chair = chair;
    b.target = seat.position + seat.outward * 0.06f;
    b.speed = 3.0f;
    b.hopTime = 0.3f;
    b.hopHeight = 0.4f;
    b.lift = 0.47f;
    b.faceSet = true;
    b.face = seat.yaw;
    b.gazeSet = false;
}

void Stage::celebrate(const Vec3& at) { fx.celebrate(at); }

void Stage::launch(Player& p) {
    p.state = Player::Launch;
    p.timer = 0.0f;
    fx.launch(meadow, bean(p));
}

void Stage::update(float dt) {
    time += dt;
    const float walkSpeed = 1.25f;
    if (music && time >= script.stopAt) music = false;

    for (Player& p : players) {
        Bean& b = bean(p);
        const Vec3 at = flat(meadow.beanPosition(b));
        switch (p.state) {
            case Player::Walk: {
                if (music || time < script.stopAt + p.delay) {
                    p.angle -= walkSpeed / walk * dt;
                    const float ahead = p.angle - 0.3f;
                    b.target = Vec3{std::cos(ahead) * walk, 0, std::sin(ahead) * walk};
                    if (!music) b.target = at;
                    if (!music) {
                        b.gazeSet = true;
                        b.gaze[0] = 0.5f * std::sin(time * 9.0f + static_cast<float>(p.bean));
                        b.gaze[1] = 0.0f;
                    }
                } else {
                    dash(p);
                }
                break;
            }
            case Player::Dash: {
                Seat& seat = seats[static_cast<size_t>(p.chair)];
                if (length(flat(b.target) - at) < 0.12f) {
                    if (seat.occupant < 0) {
                        sit(p, p.chair);
                    } else {
                        p.state = Player::Bonked;
                        p.timer = 0.55f;
                        p.tried.push_back(p.chair);
                        b.knock = seat.outward * 3.4f + Vec3{-seat.outward.z, 0, seat.outward.x} * 0.8f;
                        b.tiltVel = -9.0f;
                        b.target = at + seat.outward * 0.9f;
                        b.gazeSet = true;
                        b.gaze[0] = 0.0f;
                        b.gaze[1] = 0.25f;
                    }
                }
                break;
            }
            case Player::Bonked: {
                p.timer -= dt;
                b.target = at;
                if (p.timer <= 0.0f) dash(p);
                break;
            }
            case Player::Sit:
                if (length(flat(b.target) - at) < 0.04f) b.target = at;
                break;
            case Player::Out: {
                p.timer += dt;
                const float t = p.timer;
                b.target = at;
                if (t < 1.1f) {
                    b.gaze[0] = t < 0.35f ? -0.55f : t < 0.7f ? 0.55f : 0.0f;
                    b.gaze[1] = t < 0.7f ? 0.0f : 0.3f;
                    b.shake = t > 0.7f ? 1.0f : 0.0f;
                } else {
                    b.shake = 0.0f;
                    b.droopTarget = 1.6f;
                    if (t > 1.45f) {
                        b.droopTarget = 0.0f;
                        b.droop = 0.0f;
                        launch(p);
                    }
                }
                break;
            }
            case Player::Launch: {
                p.timer += dt;
                if (fx.fly(meadow, b)) p.state = Player::Gone;
                break;
            }
            case Player::Gone:
            case Player::Leave:
                break;
        }
    }

    if (script.final && winner < 0) {
        for (size_t i = 0; i < players.size(); ++i)
            if (players[i].state == Player::Sit && bean(players[i]).elevation > 0.42f) {
                winner = static_cast<int>(i);
                celebrate(seats[static_cast<size_t>(players[i].chair)].position);
                nextCheer_ = time + 0.5f;
            }
    }
    if (winner >= 0 && time >= nextCheer_) {
        Bean& b = bean(players[static_cast<size_t>(winner)]);
        b.hop = 1.0f;
        b.hopHeight = 0.35f;
        b.hopTime = 0.34f;
        nextCheer_ = time + 0.55f;
    }

    if (script.clearAt > 0.0f && time >= script.clearAt) {
        for (Player& p : players) {
            Bean& b = bean(p);
            if (p.state == Player::Sit && time >= script.clearAt + 0.6f) {
                Seat& seat = seats[static_cast<size_t>(p.chair)];
                p.state = Player::Leave;
                b.lift = 0.0f;
                b.faceSet = false;
                b.speed = 2.0f;
                b.hopHeight = 0.3f;
                b.target = flat(seat.position + seat.outward * 1.0f);
                seat.occupant = -1;
            }
        }
        if (!seats.empty() && !seats[0].removing && time >= script.clearAt + 1.2f) {
            seats[0].removing = true;
            fx.puff(seats[0].position + Vec3{0, 0.6f, 0}, Vec3{1.0f, 0.9f, 0.7f}, 250, 3.0f, 0.06f);
        }
    }
    for (Seat& seat : seats) {
        if (!seat.removing) continue;
        seat.scale = std::max(0.0f, seat.scale - dt * 3.0f);
        Transform& t = meadow.scene.world.get<Transform>(seat.entity);
        const float s = seat.scale > 0.0f ? ease(seat.scale) * (1.0f + 0.25f * std::sin(seat.scale * PI)) : 0.0f;
        t.scale = Vec3{s, s, s};
        if (seat.scale <= 0.0f) meadow.scene.world.get<Renderable>(seat.entity).visible = 0;
    }

    fx.update(dt);
    meadow.update(dt);
}

bool Stage::camera(const std::string& shot, CameraKey& out) {
    const float t = time;
    auto track = [&](size_t player, float rate) {
        Vec3 p = flat(meadow.beanPosition(bean(players[player])));
        if (!smoothedSet_) {
            smoothed_ = p;
            smoothedSet_ = true;
        }
        smoothed_ = smoothed_ + (p - smoothed_) * std::min(1.0f, rate);
        return smoothed_;
    };
    auto facing = [&](size_t player) {
        const float y = bean(players[player]).yaw;
        return Vec3{std::sin(y), 0, std::cos(y)};
    };
    const float rate = 1.0f / 30.0f;
    const Vec3 up{0, 1, 0};
    const size_t intern = players.size() - 1;

    if (shot == "round-wide" || shot == "final-wide") {
        const float r = shot == "final-wide" ? 7.0f : 8.0f, h = shot == "final-wide" ? 3.4f : 4.6f;
        const float a = 0.9f + t * 0.035f;
        out = CameraKey{Vec3{std::sin(a) * r, h, std::cos(a) * r}, Vec3{0, 0.6f, 0}, 40};
        return true;
    }
    if (shot == "round-top" || shot == "final-top") {
        const float a = t * 0.05f;
        out = CameraKey{Vec3{std::sin(a) * 1.2f, 13.0f - t * 0.25f, std::cos(a) * 1.2f + 0.01f}, Vec3{0, 0, 0}, 40};
        return true;
    }
    if (shot == "round-low") {
        const float a = 2.2f, d = 8.5f - 2.0f * ease(t / script.length);
        out = CameraKey{Vec3{std::cos(a) * d, 0.45f, std::sin(a) * d}, Vec3{0, 0.85f, 0}, 38};
        return true;
    }
    if (shot == "round-follow") {
        const Vec3 p = track(intern, rate);
        const Vec3 o = normalize(p);
        const Vec3 side{-o.z, 0, o.x};
        out = CameraKey{p + o * 3.4f + side * 1.4f + up * 1.25f, p + up * 0.8f - o * 0.6f, 40};
        return true;
    }
    if (shot == "round-chair") {
        int c = loser >= 0 && !players[static_cast<size_t>(loser)].tried.empty() ? players[static_cast<size_t>(loser)].tried.front() : -1;
        if (c < 0) c = players[intern].chair >= 0 ? players[intern].chair : 1;
        if (t > script.stopAt + 0.3f && lockedSeat_ < 0) lockedSeat_ = c;
        const Seat& s = seats[static_cast<size_t>(lockedSeat_ >= 0 ? lockedSeat_ : c)];
        const Vec3 side{-s.outward.z, 0, s.outward.x};
        out = CameraKey{s.position + s.outward * 3.0f + side * 1.1f + up * 0.75f, s.position + up * 0.75f + s.outward * 0.4f, 36};
        return true;
    }
    if (shot == "round-loser" || shot == "final-loser") {
        const size_t who = loser >= 0 ? static_cast<size_t>(loser) : intern;
        const Player& p = players[who];
        if (p.state == Player::Launch || p.state == Player::Gone) {
            const Vec3 sky = meadow.beanPosition(bean(p)) + up * 0.75f;
            launchLook_ = launchLook_ + (sky - launchLook_) * std::min(1.0f, rate * 6.0f);
            out = CameraKey{launchEye_, launchLook_, 44};
            return true;
        }
        const Vec3 at = track(who, rate * 0.6f);
        const Vec3 f = facing(who);
        const float push = ease((t - script.stopAt) / 4.0f);
        launchEye_ = at + f * (3.6f - 1.4f * push) + up * (1.2f - 0.4f * push);
        launchLook_ = at + up * 0.95f;
        out = CameraKey{launchEye_, launchLook_, 36};
        return true;
    }
    if (shot == "final-faceoff") {
        const Seat& s = seats[0];
        const Vec3 side{-s.outward.z, 0, s.outward.x};
        out = CameraKey{s.position + s.outward * 4.2f + side * 0.3f + up * 0.7f, s.position + up * 0.75f, 38};
        return true;
    }
    if (shot == "final-kevin" || shot == "final-steve") {
        const size_t who = shot == "final-kevin" ? 0 : 1;
        const Vec3 p = track(who, rate);
        const Vec3 f = facing(who);
        out = CameraKey{p + f * 2.0f + up * 1.1f, p + up * 1.0f, 34};
        return true;
    }
    if (shot == "final-winner") {
        const Seat& s = seats[0];
        const float push = ease((t - script.stopAt) / 6.0f);
        out = CameraKey{s.position + s.outward * (5.0f - 2.0f * push) + up * (1.0f + 0.6f * push), s.position + up * 1.0f, 38};
        return true;
    }
    return false;
}

}  // namespace chairs
