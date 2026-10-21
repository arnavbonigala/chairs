#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "bots.hpp"
#include "brain.hpp"
#include "core/jobs.hpp"

using namespace chairs;

namespace {

struct Slot {
    std::unique_ptr<Game> game;
    int players = 0;
    std::vector<Input> inputs;
    std::vector<Bot> foes;
    std::vector<char> foe;
    bool pooled = false;
    Random rng{1};
};

struct Stats {
    double games = 0, rounds = 0, sits = 0, hits = 0, knocks = 0, fouls = 0, seconds = 0, finished = 0, most = 0, poolGames = 0, poolWins = 0;
};

struct Batch {
    int count = 0, minPlayers = 2, maxPlayers = 10;
    bool fouls = true;
    uint64_t seed = 1;
    uint64_t next = 0;
    std::vector<Slot> slots;
    std::vector<Stats> stats;
    std::unique_ptr<JobSystem> jobs;
    std::vector<std::unique_ptr<brain::Policy>> pool;
    float poolShare = 0.0f;
    bool poolBots = false;

    void start(int e) {
        Slot& s = slots[static_cast<size_t>(e)];
        Random r(seed * 7919 + next * 104729 + static_cast<uint64_t>(e));
        const uint64_t gameSeed = seed * 1000003 + next++;
        s.players = minPlayers + static_cast<int>(r.below(static_cast<uint32_t>(maxPlayers - minPlayers + 1)));
        std::vector<int> ids;
        while (static_cast<int>(ids.size()) < s.players) {
            const int id = static_cast<int>(r.below(100));
            bool dup = false;
            for (int x : ids) dup |= x == id;
            if (!dup) ids.push_back(id);
        }
        Rules rules;
        rules.ringFoul = rules.stallFoul = fouls;
        s.game = std::make_unique<Game>(ids, gameSeed, rules);
        s.inputs.assign(static_cast<size_t>(s.players), Input{});
        s.rng = Random(gameSeed * 31 + 17);
        s.foes.assign(static_cast<size_t>(s.players), Bot{});
        s.foe.assign(static_cast<size_t>(s.players), 0);
        const int choices = static_cast<int>(pool.size()) + (poolBots ? 1 : 0);
        s.pooled = choices > 0 && s.players >= 2 && r.unit() < poolShare;
        if (!s.pooled) return;
        const int pick = static_cast<int>(r.below(static_cast<uint32_t>(choices)));
        for (int a = 1; a < s.players; a += 2) {
            s.foe[static_cast<size_t>(a)] = 1;
            s.foes[static_cast<size_t>(a)] = pick < static_cast<int>(pool.size()) ? Bot::trained(*pool[static_cast<size_t>(pick)]) : Bot::random(r);
        }
    }

    void observe(int e, float* obs, uint8_t* mask) {
        const Slot& s = slots[static_cast<size_t>(e)];
        for (int a = 0; a < maxPlayers; ++a) {
            float* o = obs + (static_cast<size_t>(e) * maxPlayers + a) * brain::OBS;
            const bool live = a < s.players && !s.foe[static_cast<size_t>(a)] && s.game->beans[static_cast<size_t>(a)].alive &&
                              s.game->phase != Game::Over;
            mask[static_cast<size_t>(e) * maxPlayers + a] = live ? 1 : 0;
            if (live) brain::observe(*s.game, a, o);
            else std::memset(o, 0, sizeof(float) * brain::OBS);
        }
    }
};

}  // namespace

extern "C" {

int chairs_obs_size() { return brain::OBS; }
int chairs_logits() { return brain::LOGITS; }
int chairs_moves() { return brain::MOVES; }

void* chairs_env_new(int count, int minPlayers, int maxPlayers, uint64_t seed, int fouls, int threads) {
    auto* b = new Batch;
    b->count = count;
    b->minPlayers = minPlayers;
    b->maxPlayers = maxPlayers;
    b->seed = seed;
    b->fouls = fouls != 0;
    b->slots.resize(static_cast<size_t>(count));
    b->stats.resize(static_cast<size_t>(count));
    b->jobs = std::make_unique<JobSystem>(threads - 1);
    for (int e = 0; e < count; ++e) b->start(e);
    return b;
}

void chairs_env_free(void* h) { delete static_cast<Batch*>(h); }

void chairs_env_set_fouls(void* h, int fouls) { static_cast<Batch*>(h)->fouls = fouls != 0; }

int chairs_env_add_opponent(void* h, const char* path) {
    auto policy = std::make_unique<brain::Policy>();
    std::string error;
    if (!policy->load(path, error)) return 0;
    static_cast<Batch*>(h)->pool.push_back(std::move(policy));
    return 1;
}

void chairs_env_set_pool(void* h, float share, int bots) {
    auto* b = static_cast<Batch*>(h);
    b->poolShare = share;
    b->poolBots = bots != 0;
}

void chairs_env_observe(void* h, float* obs, uint8_t* mask) {
    auto* b = static_cast<Batch*>(h);
    for (int e = 0; e < b->count; ++e) b->observe(e, obs, mask);
}

void chairs_env_step(void* h, const int32_t* actions, float* obs, float* reward, uint8_t* done, uint8_t* mask) {
    auto* b = static_cast<Batch*>(h);
    const int A = b->maxPlayers;
    std::vector<uint8_t> over(static_cast<size_t>(b->count), 0);
    b->jobs->parallelFor(static_cast<size_t>(b->count), 4, [&](size_t lo, size_t hi) {
        for (size_t e = lo; e < hi; ++e) {
            Slot& s = b->slots[e];
            Stats& st = b->stats[e];
            Game& g = *s.game;
            float* r = reward + e * static_cast<size_t>(A);
            uint8_t* d = done + e * static_cast<size_t>(A);
            std::memset(r, 0, sizeof(float) * static_cast<size_t>(A));
            std::memset(d, 0, static_cast<size_t>(A));
            for (int a = 0; a < s.players; ++a) {
                const int32_t* act = actions + (e * static_cast<size_t>(A) + static_cast<size_t>(a)) * 3;
                if (g.beans[static_cast<size_t>(a)].alive && !s.foe[static_cast<size_t>(a)])
                    s.inputs[static_cast<size_t>(a)] = brain::act(g, a, act[0], act[1] != 0, act[2] != 0);
            }
            for (int k = 0; k < brain::REPEAT && g.phase != Game::Over; ++k) {
                for (int a = 0; a < s.players; ++a)
                    if (s.foe[static_cast<size_t>(a)]) s.inputs[static_cast<size_t>(a)] = s.foes[static_cast<size_t>(a)].think(g, a, s.rng);
                g.step(s.inputs);
                for (const Event& ev : g.events) {
                    switch (ev.kind) {
                        case Event::RoundOver:
                            st.rounds += 1;
                            for (int a = 0; a < s.players; ++a)
                                if (g.beans[static_cast<size_t>(a)].alive) r[a] += 1.0f;
                            break;
                        case Event::Eliminated:
                            r[ev.a] -= 1.0f;
                            d[ev.a] = 1;
                            break;
                        case Event::Winner:
                            if (s.pooled) {
                                st.poolGames += 1;
                                if (ev.a >= 0 && !s.foe[static_cast<size_t>(ev.a)]) st.poolWins += 1;
                            }
                            if (ev.a >= 0) {
                                r[ev.a] += 2.0f;
                                d[ev.a] = 1;
                            }
                            break;
                        case Event::Sit: st.sits += 1; break;
                        case Event::Hit: st.hits += 1; break;
                        case Event::KnockedOff: st.knocks += 1; break;
                        case Event::Foul: st.fouls += 1; break;
                        default: break;
                    }
                }
                g.events.clear();
            }
            if (g.phase == Game::Over || g.time > 240.0f) {
                for (int a = 0; a < s.players; ++a)
                    if (g.beans[static_cast<size_t>(a)].alive) d[a] = 1;
                st.games += 1;
                st.seconds += g.time;
                st.finished += g.round;
                st.most += s.players - 1;
                over[e] = 1;
            } else {
                b->observe(static_cast<int>(e), obs, mask);
            }
        }
    });
    for (int e = 0; e < b->count; ++e) {
        if (!over[static_cast<size_t>(e)]) continue;
        b->start(e);
        b->observe(e, obs, mask);
    }
}

void chairs_env_stats(void* h, double* out) {
    auto* b = static_cast<Batch*>(h);
    Stats t;
    for (Stats& s : b->stats) {
        t.games += s.games;
        t.rounds += s.rounds;
        t.sits += s.sits;
        t.hits += s.hits;
        t.knocks += s.knocks;
        t.fouls += s.fouls;
        t.seconds += s.seconds;
        t.finished += s.finished;
        t.most += s.most;
        t.poolGames += s.poolGames;
        t.poolWins += s.poolWins;
        s = Stats{};
    }
    const double v[11] = {t.games, t.rounds, t.sits, t.hits, t.knocks, t.fouls, t.seconds, t.finished, t.most, t.poolGames, t.poolWins};
    std::memcpy(out, v, sizeof(v));
}
}
