#pragma once
#include <string>
#include <vector>

#include "meadow.hpp"
#include "fx.hpp"
#include "shots.hpp"

namespace chairs {

struct Seat {
    Entity entity = NULL_ENTITY;
    Vec3 position;
    Vec3 outward;
    float yaw = 0;
    int occupant = -1;
    float scale = 1;
    bool removing = false;
};

struct Player {
    enum State { Walk, Dash, Sit, Bonked, Out, Launch, Gone, Leave };
    size_t bean = 0;
    float angle = 0;
    float delay = 0.2f;
    int chair = -1;
    State state = Walk;
    float timer = 0;
    std::vector<int> tried;
};

struct Routine {
    std::vector<int> beans;
    std::vector<float> delays;
    int chairs = 5;
    float stopAt = 6.0f;
    float clearAt = 0.0f;
    float length = 12.0f;
    bool final = false;
    int doomed = -1;
};

const Routine* findRoutine(const std::string& name);

class Stage {
public:
    Meadow& meadow;
    Routine script;
    std::vector<Seat> seats;
    std::vector<Player> players;
    Effects fx;
    float time = 0;
    bool music = true;
    int loser = -1;
    int winner = -1;
    float ring = 1.6f;
    float walk = 3.0f;

    Stage(Meadow& m, const Routine& s);
    void update(float dt);
    bool camera(const std::string& shot, CameraKey& out);
    static std::vector<std::string> shots(const std::string& scene);

private:
    Bean& bean(const Player& p) { return meadow.beans[p.bean]; }
    void dash(Player& p);
    void sit(Player& p, int chair);
    void celebrate(const Vec3& at);
    void launch(Player& p);

    Vec3 smoothed_;
    bool smoothedSet_ = false;
    float nextCheer_ = 0;
    int lockedSeat_ = -1;
    Vec3 launchEye_;
    Vec3 launchLook_;
};

}  // namespace chairs
