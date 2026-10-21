#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "bots.hpp"
#include "names.hpp"

using namespace chairs;

namespace {

std::vector<int> parseCast(const std::string& cast, int players, uint64_t seed) {
    std::vector<int> ids;
    size_t start = 0;
    while (!cast.empty() && start <= cast.size()) {
        const size_t comma = cast.find(',', start);
        const std::string name = cast.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        for (int k = 0; k < NAME_COUNT; ++k)
            if (name == NAMES[k]) ids.push_back(k);
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    Random pick(seed * 31 + 7);
    while (static_cast<int>(ids.size()) < players) {
        const int id = static_cast<int>(pick.below(NAME_COUNT));
        bool dup = false;
        for (int x : ids) dup |= x == id;
        if (!dup) ids.push_back(id);
    }
    return ids;
}

}  // namespace

int main(int argc, char** argv) {
    int players = 6, games = 20;
    uint64_t seed = 1;
    bool verbose = false, quiet = false;
    int gen = -1, vs = -2;
    std::string cast;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--players") && i + 1 < argc) players = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--games") && i + 1 < argc) games = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--seed") && i + 1 < argc) seed = std::strtoull(argv[++i], nullptr, 10);
        else if (!std::strcmp(argv[i], "--cast") && i + 1 < argc) cast = argv[++i];
        else if (!std::strcmp(argv[i], "--gen") && i + 1 < argc) gen = brain::parseGen(argv[++i]);
        else if (!std::strcmp(argv[i], "--vs") && i + 1 < argc) vs = brain::parseGen(argv[++i]);
        else if (!std::strcmp(argv[i], "-v")) verbose = true;
        else if (!std::strcmp(argv[i], "-q")) quiet = true;
    }
    brain::Policy policies[2];
    bool genFouls = true, unused = true;
    std::string error;
    if ((gen > 0 && !brain::loadGen(gen, policies[0], genFouls, error)) || (vs > 0 && !brain::loadGen(vs, policies[1], unused, error))) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    const bool foulsOn = gen == 0 ? false : gen > 0 ? genFouls : true;
    auto make = [&](int which, Random& rng) {
        const int g = which == 0 ? gen : vs;
        return g > 0 ? Bot::trained(policies[which]) : g == 0 ? Bot::fresh() : Bot::random(rng);
    };
    double wins[2] = {0, 0};
    long steps = 0, hits = 0, knocks = 0, fouls = 0;
    const auto start = std::chrono::steady_clock::now();
    for (int g = 0; g < games; ++g) {
        const uint64_t s = seed + static_cast<uint64_t>(g);
        const std::vector<int> ids = parseCast(cast, players, s);
        Rules rules;
        rules.ringFoul = rules.stallFoul = foulsOn;
        Game game(ids, s, rules);
        Random rng(s * 1000 + 1);
        std::vector<Bot> bots;
        for (size_t i = 0; i < ids.size(); ++i) bots.push_back(make(vs > -2 ? static_cast<int>(i % 2) : 0, rng));
        std::vector<Input> inputs(ids.size());
        std::string perRound;
        int roundKnocks = 0, roundHits = 0, lastRound = 1;
        while (game.phase != Game::Over && game.time < 300.0f) {
            for (size_t i = 0; i < ids.size(); ++i) inputs[i] = bots[i].think(game, static_cast<int>(i), rng);
            game.step(inputs);
            ++steps;
            if (game.round != lastRound) {
                perRound += " r" + std::to_string(lastRound) + ":" + std::to_string(roundHits) + "h" + std::to_string(roundKnocks) + "k";
                roundHits = roundKnocks = 0;
                lastRound = game.round;
            }
            for (const Event& e : game.events) {
                hits += e.kind == Event::Hit;
                knocks += e.kind == Event::KnockedOff;
                fouls += e.kind == Event::Foul;
                roundHits += e.kind == Event::Hit;
                roundKnocks += e.kind == Event::KnockedOff;
                if (verbose && (e.kind == Event::KnockedOff || e.kind == Event::Eliminated || e.kind == Event::MusicStop))
                    std::printf("  seed %llu r%d %6.2f %s %s\n", static_cast<unsigned long long>(s), game.round, e.time,
                                e.kind == Event::KnockedOff ? "knocked off" : e.kind == Event::Eliminated ? "out" : "stop",
                                e.a >= 0 ? NAMES[game.beans[static_cast<size_t>(e.a)].id] : "");
            }
            game.events.clear();
        }
        perRound += " r" + std::to_string(lastRound) + ":" + std::to_string(roundHits) + "h" + std::to_string(roundKnocks) + "k";
        if (game.winner >= 0) wins[vs > -2 ? game.winner % 2 : 0] += 1;
        if (!quiet) std::printf("seed %llu: %s wins, %d rounds, %.1f s |%s\n", static_cast<unsigned long long>(s),
                    game.winner >= 0 ? NAMES[game.beans[static_cast<size_t>(game.winner)].id] : "nobody", game.round,
                    game.time, perRound.c_str());
    }
    if (vs > -2)
        std::printf("gen %d won %.0f, gen %d won %.0f of %d games (%.1f%% vs %.1f%%)\n", gen, wins[0], vs, wins[1], games,
                    100.0 * wins[0] / games, 100.0 * wins[1] / games);
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::printf("%ld steps in %.2f s (%.0f steps/s); per game %.1f hits, %.1f knock-offs, %.1f fouls\n", steps, secs,
                static_cast<double>(steps) / secs, static_cast<double>(hits) / games, static_cast<double>(knocks) / games,
                static_cast<double>(fouls) / games);
}
