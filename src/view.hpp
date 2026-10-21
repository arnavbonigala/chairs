#pragma once
#include <string>
#include <vector>

#include "fx.hpp"
#include "game.hpp"
#include "meadow.hpp"
#include "shots.hpp"

namespace chairs {

class GameView {
public:
    Effects fx;

    GameView(Meadow& meadow, Game& game);
    void sync(float dt);
    bool camera(const std::string& shot, int focus, CameraKey& out);
    float overTime() const { return overTime_; }

private:
    struct Prop {
        Entity entity;
        float scale;
        float target;
    };

    void layout();

    Meadow& meadow_;
    Game& game_;
    int round_ = 0;
    std::vector<Prop> props_;
    uint32_t chairMesh_ = 0;
    uint32_t chairMaterial_ = 0;
    float overTime_ = 0;
    float cheer_ = 0;
    Vec3 smoothed_;
    Vec3 look_;
    Vec3 eye_;
    bool smoothedSet_ = false;
    Vec3 interest_;
    Vec3 interestSmoothed_;
    float interestTime_ = -10;
    float actionRadius_ = 0;
};

}  // namespace chairs
