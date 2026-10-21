#include "bots.hpp"

#include <algorithm>

namespace chairs {

namespace {

Vec3 flat(Vec3 v) {
    v.y = 0;
    return v;
}

}  // namespace

Bot Bot::random(Random& r) {
    Bot b;
    b.reaction = r.uniform(0.12f, 0.45f);
    b.aggression = r.uniform(0.2f, 1.0f);
    b.pace = r.uniform(0.6f, 0.9f);
    b.wobble = r.uniform(0.0f, 6.28f);
    return b;
}

Bot Bot::fresh() {
    Bot b;
    b.untrained = true;
    return b;
}

Bot Bot::trained(const brain::Policy& policy) {
    Bot b;
    b.policy = &policy;
    return b;
}

Input Bot::think(const Game& game, int self, Random& r) {
    Input in;
    const Contestant& me = game.beans[static_cast<size_t>(self)];
    if (!me.alive) return in;
    if (policy) {
        if (--hold <= 0) {
            hold = brain::REPEAT;
            held = policy->decide(game, self, r);
        }
        return held;
    }
    if (untrained) {
        if (--hold <= 0) {
            hold = 8;
            const int dir = static_cast<int>(r.below(9));
            const float a = static_cast<float>(dir) * 0.785398f;
            held.moveX = dir == 8 ? 0.0f : std::cos(a);
            held.moveZ = dir == 8 ? 0.0f : std::sin(a);
            held.jump = r.chance(0.5f);
            held.shove = r.chance(0.5f);
        }
        return held;
    }
    if (game.round != lastRound) {
        lastRound = game.round;
        reacted = false;
        target = -1;
        waited = 0;
    }
    const Vec3 p = flat(me.position);
    const float r0 = std::max(length(p), 1e-3f);

    if (game.phase == Game::Music || game.phase == Game::Results) {
        const Vec3 radial = p * (1.0f / r0);
        const Vec3 tangent{radial.z, 0, -radial.x};
        const float want = game.ring + game.rules.ringMargin + 0.95f + 0.25f * std::sin(game.time * 0.7f + wobble);
        const Vec3 move = tangent * pace + radial * std::clamp((want - r0) * 1.5f, -0.6f, 0.6f);
        in.moveX = move.x;
        in.moveZ = move.z;
        return in;
    }
    if (game.phase != Game::Scramble || me.seat >= 0) {
        if (me.seat >= 0 && !me.locked) {
            for (size_t j = 0; j < game.beans.size(); ++j) {
                const Contestant& o = game.beans[j];
                if (static_cast<int>(j) == self || !o.alive || o.seat >= 0) continue;
                const Vec3 d = flat(o.position - me.position);
                if (length(d) < 1.1f && dot(normalize(d), Vec3{std::sin(me.yaw), 0, std::cos(me.yaw)}) > 0.3f &&
                    r.chance(aggression * 0.08f))
                    in.shove = true;
            }
        }
        return in;
    }

    waited += 1.0f / 240.0f;
    if (waited < reaction) {
        const Vec3 radial = p * (1.0f / r0);
        const Vec3 tangent{radial.z, 0, -radial.x};
        in.moveX = tangent.x * pace * 0.5f;
        in.moveZ = tangent.z * pace * 0.5f;
        return in;
    }

    auto score = [&](size_t k) {
        const Chair& c = game.chairs[k];
        const Vec3 approach = flat(c.position + c.outward * 0.55f);
        float s = length(approach - p);
        if (c.occupant >= 0) {
            const Contestant& o = game.beans[static_cast<size_t>(c.occupant)];
            if (o.locked) return 1e9f;
            s += 1.5f - aggression;
        }
        return s;
    };
    if (target < 0 || score(static_cast<size_t>(target)) > 1e8f || r.chance(0.01f)) {
        float best = 1e9f;
        for (size_t k = 0; k < game.chairs.size(); ++k) {
            const float s = score(k);
            if (s < best) {
                best = s;
                target = static_cast<int>(k);
            }
        }
    }
    if (target < 0) return in;
    const Chair& c = game.chairs[static_cast<size_t>(target)];
    const Vec3 approach = flat(c.position + c.outward * 1.1f);
    const Vec3 seatPoint = flat(c.position + c.outward * 0.08f);
    const Vec3 to = dot(p - flat(c.position), c.outward) < 0.75f ? approach : seatPoint;
    Vec3 move = normalize(to - p);
    in.moveX = move.x;
    in.moveZ = move.z;

    const Vec3 facing{std::sin(me.yaw), 0, std::cos(me.yaw)};
    for (size_t j = 0; j < game.beans.size(); ++j) {
        const Contestant& o = game.beans[j];
        if (static_cast<int>(j) == self || !o.alive || o.locked) continue;
        const Vec3 d = flat(o.position - me.position);
        const float dist = length(d);
        if (dist > game.rules.shoveRange * 0.9f || dist < 1e-3f) continue;
        const bool onMine = o.seat == target;
        const bool rival = length(flat(o.position) - seatPoint) < length(p - seatPoint) + 0.3f;
        if (dot(d * (1.0f / dist), facing) > 0.5f && (onMine || (rival && r.chance(aggression * 0.15f)))) in.shove = true;
    }
    if (me.stun <= 0.0f && length(flat(me.velocity)) < 0.3f && r.chance(0.02f)) in.jump = true;
    return in;
}

}  // namespace chairs
