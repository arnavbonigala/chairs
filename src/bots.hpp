#pragma once
#include <vector>

#include "brain.hpp"
#include "game.hpp"

namespace chairs {

struct Bot {
    float reaction = 0.25f;
    float aggression = 0.6f;
    float pace = 0.8f;
    float wobble = 0.0f;
    float waited = 0;
    int target = -1;
    int lastRound = 0;
    bool reacted = false;
    bool untrained = false;
    int hold = 0;
    Input held;
    const brain::Policy* policy = nullptr;

    static Bot random(Random& r);
    static Bot fresh();
    static Bot trained(const brain::Policy& policy);
    Input think(const Game& game, int self, Random& r);
};

}  // namespace chairs
