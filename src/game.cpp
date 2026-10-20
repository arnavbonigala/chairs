#include "game.hpp"

#include <algorithm>

#include "scene/components.hpp"

namespace chairs {

namespace {

constexpr float RADIUS = 0.42f;
constexpr float HALF = 0.33f;
constexpr float CENTER = HALF + RADIUS;
constexpr float SEAT_TOP = 0.48f;

Vec3 flat(Vec3 v) {
    v.y = 0;
    return v;
}

}  // namespace

Game::Game(const std::vector<int>& ids, uint64_t seed, const Rules& r, float dt) : rules(r), random_(seed), dt_(dt) {
    physics_.settings.allowSleep = false;
    physics_.settings.boundsMin = Vec3{-40, -1, -40};
    physics_.settings.boundsMax = Vec3{40, 60, 40};
    physics_.settings.linearDamping = 0.0f;

    Entity ground = scene_.create(Transform{});
    Collider plane;
    plane.kind = static_cast<uint32_t>(ColliderKind::Plane);
    plane.invMass = 0.0f;
    plane.friction = 0.3f;
    scene_.world.add<Collider>(ground, plane);

    for (int id : ids) {
        Contestant c;
        c.id = id;
        Transform t;
        c.body = scene_.create(t);
        scene_.world.add<Velocity>(c.body, Velocity{});
        Collider col;
        col.kind = static_cast<uint32_t>(ColliderKind::Capsule);
        col.radius = RADIUS;
        col.halfExtents = Vec3{0, HALF, 0};
        col.restitution = 0.05f;
        col.friction = 0.1f;
        col.fixedRotation = 1;
        scene_.world.add<Collider>(c.body, col);
        beans.push_back(c);
    }
    layoutChairs(std::max(1, static_cast<int>(ids.size()) - 1));
    startRound();
}

int Game::aliveCount() const {
    int n = 0;
    for (const Contestant& c : beans) n += c.alive;
    return n;
}

void Game::layoutChairs(int count) {
    for (Chair& c : chairs) {
        scene_.destroy(c.seat);
        scene_.destroy(c.back);
    }
    chairs.clear();
    ring = count == 1 ? 0.0f : std::max(1.6f, static_cast<float>(count) * 1.7f / (2.0f * PI));
    const float offset = random_.uniform(0.0f, 2.0f * PI);
    for (int i = 0; i < count; ++i) {
        Chair chair;
        const float a = offset + static_cast<float>(i) / static_cast<float>(count) * 2.0f * PI;
        chair.outward = count == 1 ? Vec3{std::cos(a), 0, std::sin(a)} : Vec3{std::cos(a), 0, std::sin(a)};
        chair.position = chair.outward * ring;
        chair.yaw = std::atan2(chair.outward.x, chair.outward.z);
        const Quat q = Quat::axisAngle(Vec3{0, 1, 0}, chair.yaw);
        Transform seatT;
        seatT.position = chair.position + Vec3{0, SEAT_TOP * 0.5f, 0};
        seatT.rotation = q;
        chair.seat = scene_.create(seatT);
        Collider seat;
        seat.kind = static_cast<uint32_t>(ColliderKind::Box);
        seat.halfExtents = Vec3{0.43f, SEAT_TOP * 0.5f, 0.43f};
        seat.invMass = 0.0f;
        scene_.world.add<Collider>(chair.seat, seat);
        Transform backT;
        backT.position = chair.position + rotate(q, Vec3{0, 0.8f, -0.42f});
        backT.rotation = q;
        chair.back = scene_.create(backT);
        Collider back;
        back.kind = static_cast<uint32_t>(ColliderKind::Box);
        back.halfExtents = Vec3{0.4f, 0.34f, 0.04f};
        back.invMass = 0.0f;
        scene_.world.add<Collider>(chair.back, back);
        chairs.push_back(chair);
    }
}

void Game::startRound() {
    phase = Music;
    phaseTime = 0;
    stopAt = random_.uniform(rules.stopMin, rules.stopMax);
    const float walk = ring + 1.6f;
    int k = 0;
    const int alive = aliveCount();
    const float offset = random_.uniform(0.0f, 2.0f * PI);
    for (size_t i = 0; i < beans.size(); ++i) {
        Contestant& c = beans[i];
        if (!c.alive) continue;
        if (c.seat >= 0) unseat(static_cast<int>(i));
        const float a = offset + static_cast<float>(k++) / static_cast<float>(alive) * 2.0f * PI;
        Transform& t = scene_.world.get<Transform>(c.body);
        t.position = Vec3{std::cos(a) * walk, CENTER, std::sin(a) * walk};
        scene_.world.get<Velocity>(c.body) = Velocity{};
        c.yaw = std::atan2(std::sin(a), -std::cos(a));
        c.locked = false;
        c.lock = 0;
        c.stall = 0;
        c.stun = 0;
    }
    events.push_back(Event{Event::MusicStart, -1, -1, time});
}

void Game::unseat(int i) {
    Contestant& c = beans[static_cast<size_t>(i)];
    if (c.seat < 0) return;
    Chair& chair = chairs[static_cast<size_t>(c.seat)];
    chair.occupant = -1;
    Collider& col = scene_.world.get<Collider>(c.body);
    col.invMass = 1.0f;
    Transform& t = scene_.world.get<Transform>(c.body);
    t.position = chair.position + chair.outward * 0.62f + Vec3{0, CENTER + 0.05f, 0};
    c.seat = -1;
    c.locked = false;
    c.lock = 0;
}

void Game::seat(int i, int chairIndex) {
    Contestant& c = beans[static_cast<size_t>(i)];
    Chair& chair = chairs[static_cast<size_t>(chairIndex)];
    chair.occupant = i;
    c.seat = chairIndex;
    c.lock = 0;
    c.locked = false;
    Collider& col = scene_.world.get<Collider>(c.body);
    col.invMass = 0.0f;
    Transform& t = scene_.world.get<Transform>(c.body);
    t.position = chair.position + chair.outward * 0.08f + Vec3{0, SEAT_TOP + CENTER, 0};
    scene_.world.get<Velocity>(c.body) = Velocity{};
    c.yaw = chair.yaw;
    events.push_back(Event{Event::Sit, i, chairIndex, time});
}

void Game::eliminate(int i, Event::Kind why) {
    Contestant& c = beans[static_cast<size_t>(i)];
    if (!c.alive) return;
    if (c.seat >= 0) chairs[static_cast<size_t>(c.seat)].occupant = -1;
    c.seat = -1;
    c.alive = false;
    scene_.destroy(c.body);
    c.body = NULL_ENTITY;
    if (why == Event::Foul) events.push_back(Event{Event::Foul, i, -1, time});
    events.push_back(Event{Event::Eliminated, i, -1, time});
}

void Game::shove(int i) {
    Contestant& c = beans[static_cast<size_t>(i)];
    c.shoveTimer = rules.shoveCooldown;
    const Vec3 facing{std::sin(c.yaw), 0, std::cos(c.yaw)};
    int best = -1;
    float bestDist = rules.shoveRange;
    for (size_t j = 0; j < beans.size(); ++j) {
        const Contestant& o = beans[j];
        if (static_cast<int>(j) == i || !o.alive || o.locked) continue;
        const Vec3 d = flat(o.position - c.position);
        const float dist = length(d);
        if (dist > bestDist || dist < 1e-4f) continue;
        if (dot(d * (1.0f / dist), facing) < rules.shoveCone) continue;
        best = static_cast<int>(j);
        bestDist = dist;
    }
    events.push_back(Event{Event::Shove, i, best, time});
    if (best < 0) return;
    Contestant& o = beans[static_cast<size_t>(best)];
    const Vec3 dir = normalize(flat(o.position - c.position));
    if (o.seat >= 0) {
        unseat(best);
        events.push_back(Event{Event::KnockedOff, best, i, time});
    }
    Velocity& v = scene_.world.get<Velocity>(o.body);
    v.linear = dir * rules.shoveSpeed + Vec3{0, rules.shoveLift, 0};
    o.stun = rules.stunTime;
    Velocity& self = scene_.world.get<Velocity>(c.body);
    if (c.seat < 0) self.linear = self.linear - dir * 1.2f;
    events.push_back(Event{Event::Hit, best, i, time});
}

void Game::step(const std::vector<Input>& inputs) {
    if (phase == Over) return;
    time += dt_;
    phaseTime += dt_;

    for (size_t i = 0; i < beans.size(); ++i) {
        Contestant& c = beans[i];
        if (!c.alive) continue;
        const Input in = i < inputs.size() ? inputs[i] : Input{};
        c.shoveTimer = std::max(0.0f, c.shoveTimer - dt_);
        c.stun = std::max(0.0f, c.stun - dt_);
        if (c.seat >= 0) {
            if (!c.locked) {
                c.lock += dt_;
                if (c.lock >= rules.lockTime) {
                    c.locked = true;
                    events.push_back(Event{Event::Locked, static_cast<int>(i), c.seat, time});
                }
            }
            if (in.shove && c.shoveTimer <= 0.0f && !c.locked) shove(static_cast<int>(i));
            continue;
        }
        Velocity& v = scene_.world.get<Velocity>(c.body);
        Vec3 move{in.moveX, 0, in.moveZ};
        const float m = length(move);
        if (m > 1.0f) move = move * (1.0f / m);
        if (c.stun <= 0.0f) {
            const Vec3 want = move * rules.walkSpeed;
            const Vec3 have = flat(v.linear);
            Vec3 change = want - have;
            const float maxChange = (c.grounded ? rules.accel : rules.airAccel) * dt_;
            const float cl = length(change);
            if (cl > maxChange) change = change * (maxChange / cl);
            v.linear += change;
            if (m > 0.05f) {
                const float desired = std::atan2(move.x, move.z);
                float turn = desired - c.yaw;
                while (turn > PI) turn -= 2 * PI;
                while (turn < -PI) turn += 2 * PI;
                c.yaw += turn * std::min(1.0f, dt_ * 14.0f);
            }
            if (in.jump && c.grounded) v.linear.y = rules.jumpSpeed;
            if (in.shove && c.shoveTimer <= 0.0f) shove(static_cast<int>(i));
        }
    }

    physics_.step(scene_, dt_);

    for (size_t i = 0; i < beans.size(); ++i) {
        Contestant& c = beans[i];
        if (!c.alive) continue;
        Transform& t = scene_.world.get<Transform>(c.body);
        c.position = t.position - Vec3{0, CENTER, 0};
        c.velocity = scene_.world.get<Velocity>(c.body).linear;
        c.grounded = c.position.y < 0.04f || (c.position.y > SEAT_TOP - 0.04f && c.position.y < SEAT_TOP + 0.04f);
    }

    if (phase == Music) {
        for (size_t i = 0; i < beans.size(); ++i) {
            Contestant& c = beans[i];
            if (!c.alive) continue;
            const Vec3 p = flat(c.position);
            const float r = length(p);
            if (rules.ringFoul && !inGrace() && r < ring + rules.ringMargin) {
                eliminate(static_cast<int>(i), Event::Foul);
                continue;
            }
            if (rules.stallFoul && !inGrace() && r > 1e-3f) {
                const Vec3 tangent{p.z / r, 0, -p.x / r};
                const float around = dot(flat(c.velocity), tangent) / std::max(r, 1.0f);
                c.stall = around < rules.stallSpeed ? c.stall + dt_ : 0.0f;
                if (c.stall > rules.stallTime) eliminate(static_cast<int>(i), Event::Foul);
            }
        }
        if (aliveCount() <= 1) {
            phase = Results;
            phaseTime = 0;
        } else if (phaseTime >= stopAt) {
            phase = Scramble;
            phaseTime = 0;
            events.push_back(Event{Event::MusicStop, -1, -1, time});
        }
    } else if (phase == Scramble) {
        for (size_t i = 0; i < beans.size(); ++i) {
            Contestant& c = beans[i];
            if (!c.alive || c.seat >= 0 || c.stun > 0.0f || c.position.y > SEAT_TOP + 0.3f) continue;
            for (size_t k = 0; k < chairs.size(); ++k) {
                if (chairs[k].occupant >= 0) continue;
                const Vec3 rel = flat(c.position - chairs[k].position);
                const Vec3 side{-chairs[k].outward.z, 0, chairs[k].outward.x};
                const float forward = dot(rel, chairs[k].outward);
                if (forward > 0.2f && forward < 1.0f && std::fabs(dot(rel, side)) < 0.5f) {
                    seat(static_cast<int>(i), static_cast<int>(k));
                    break;
                }
            }
        }
        bool allLocked = true, allTaken = true;
        for (const Chair& ch : chairs) {
            if (ch.occupant < 0) allTaken = false;
            if (ch.occupant < 0 || !beans[static_cast<size_t>(ch.occupant)].locked) allLocked = false;
        }
        if (allLocked || (allTaken && phaseTime >= rules.scrambleTime) || phaseTime >= rules.scrambleTime * 3.0f) {
            for (size_t i = 0; i < beans.size(); ++i)
                if (beans[i].alive && beans[i].seat < 0) eliminate(static_cast<int>(i), Event::Eliminated);
            phase = Results;
            phaseTime = 0;
            events.push_back(Event{Event::RoundOver, -1, -1, time});
        }
    } else if (phase == Results && phaseTime >= rules.resultTime) {
        if (aliveCount() <= 1) {
            phase = Over;
            for (size_t i = 0; i < beans.size(); ++i)
                if (beans[i].alive) winner = static_cast<int>(i);
            events.push_back(Event{Event::Winner, winner, -1, time});
        } else {
            ++round;
            layoutChairs(aliveCount() - 1);
            startRound();
        }
    }
}

}  // namespace chairs
