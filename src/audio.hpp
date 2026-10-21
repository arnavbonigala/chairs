#pragma once
#include <memory>
#include <string>
#include <vector>

#include "game.hpp"

namespace chairs {

class Audio {
public:
    Audio();
    ~Audio();
    bool init(const std::string& dir, std::string& error);
    bool ready() const { return ready_; }
    void event(const Event& e, const Game& game);
    void jump(int bean, float time);
    void twinkle();
    void music(bool playing, const Game& game);

private:
    void play(const std::string& name);
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool ready_ = false;
    std::vector<float> lastJump_, lastShove_;
    std::string track_;
    bool playing_ = false;
};

}  // namespace chairs
