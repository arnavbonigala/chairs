#pragma once
#include <cstdint>
#include <vector>

#include "core/random.hpp"
#include "physics/physics.hpp"
#include "scene/scene.hpp"

namespace chairs {

using namespace skein;

struct Rules {
    float stopMin = 4.0f;
    float stopMax = 11.0f;
    float lockTime = 0.6f;
    float scrambleTime = 5.0f;
    float resultTime = 2.0f;
    float walkSpeed = 2.4f;
    float accel = 20.0f;
    float airAccel = 5.0f;
    float jumpSpeed = 4.6f;
    float shoveCooldown = 1.1f;
    float shoveRange = 1.3f;
    float shoveCone = 0.35f;
    float shoveSpeed = 8.0f;
    float shoveLift = 3.0f;
    float stunTime = 0.5f;
    float ringMargin = 0.55f;
    float ringGrace = 1.5f;
    float stallSpeed = 0.15f;
    float stallTime = 2.0f;
    bool ringFoul = true;
    bool stallFoul = true;
};

struct Input {
    float moveX = 0;
    float moveZ = 0;
    bool jump = false;
    bool shove = false;
};

struct Chair {
    Vec3 position;
    Vec3 outward;
    float yaw = 0;
    int occupant = -1;
    Entity seat = NULL_ENTITY;
    Entity back = NULL_ENTITY;
};

struct Contestant {
    int id = 0;
    Entity body = NULL_ENTITY;
    bool alive = true;
    int seat = -1;
    float lock = 0;
    bool locked = false;
    float shoveTimer = 0;
    float stun = 0;
    float stall = 0;
    float yaw = 0;
    Vec3 position;
    Vec3 velocity;
    bool grounded = true;
};

struct Event {
    enum Kind { MusicStart, MusicStop, Shove, Hit, Sit, KnockedOff, Locked, Foul, Eliminated, RoundOver, Winner };
    Kind kind;
    int a = -1;
    int b = -1;
    float time = 0;
};

class Game {
public:
    enum Phase { Music, Scramble, Results, Over };

    Rules rules;
    std::vector<Contestant> beans;
    std::vector<Chair> chairs;
    std::vector<Event> events;
    Phase phase = Music;
    float time = 0;
    float phaseTime = 0;
    float stopAt = 0;
    int round = 1;
    float ring = 1.6f;
    int winner = -1;

    Game(const std::vector<int>& ids, uint64_t seed, const Rules& r = {}, float dt = 1.0f / 240.0f);
    void step(const std::vector<Input>& inputs);
    int aliveCount() const;
    bool inGrace() const { return phase == Music && phaseTime < rules.ringGrace; }

private:
    void layoutChairs(int count);
    void startRound();
    void eliminate(int i, Event::Kind why);
    void unseat(int i);
    void seat(int i, int chair);
    void shove(int i);

    Scene scene_;
    PhysicsWorld physics_;
    Random random_;
    float dt_;
};

}  // namespace chairs
