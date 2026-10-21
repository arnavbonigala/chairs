#pragma once
#include <string>
#include <vector>

#include "game.hpp"

namespace chairs::brain {

constexpr int NEAR_BEANS = 8;
constexpr int NEAR_CHAIRS = 6;
constexpr int SELF = 22;
constexpr int BEAN = 11;
constexpr int CHAIR = 9;
constexpr int OBS = SELF + NEAR_BEANS * BEAN + NEAR_CHAIRS * CHAIR;
constexpr int MOVES = 9;
constexpr int LOGITS = MOVES + 2 + 2;
constexpr int REPEAT = 8;

void observe(const Game& game, int self, float* out);
Input act(const Game& game, int self, int move, bool jump, bool shove);

class Policy;
bool loadGen(int gen, Policy& policy, bool& fouls, std::string& error);
int parseGen(const char* text);

class Policy {
public:
    bool load(const std::string& path, std::string& error);
    void logits(const float* obs, float* out) const;
    Input decide(const Game& game, int self, Random& random) const;

private:
    struct Layer {
        int in = 0, out = 0;
        std::vector<float> weight, bias;
    };
    std::vector<Layer> layers_;
};

}  // namespace chairs::brain
