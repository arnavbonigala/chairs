#include "brain.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace chairs::brain {

namespace {

Vec3 flat(Vec3 v) {
    v.y = 0;
    return v;
}

struct Frame {
    Vec3 radial, tangent;
    float r(const Vec3& v) const { return dot(v, radial); }
    float t(const Vec3& v) const { return dot(v, tangent); }
};

Frame frameAt(const Vec3& p) {
    const Vec3 f = flat(p);
    const float r = length(f);
    Frame fr;
    fr.radial = r > 1e-3f ? f * (1.0f / r) : Vec3{1, 0, 0};
    fr.tangent = Vec3{fr.radial.z, 0, -fr.radial.x};
    return fr;
}

Vec3 facing(float yaw) { return Vec3{std::sin(yaw), 0, std::cos(yaw)}; }

}  // namespace

void observe(const Game& game, int self, float* out) {
    std::memset(out, 0, sizeof(float) * OBS);
    const Contestant& me = game.beans[static_cast<size_t>(self)];
    if (!me.alive) return;
    const Rules& rules = game.rules;
    const Frame fr = frameAt(me.position);
    const Vec3 p = flat(me.position);
    float* s = out;
    s[game.phase] = 1.0f;
    s[4] = std::min(game.phaseTime / 10.0f, 2.0f);
    s[5] = (length(p) - game.ring) / 3.0f;
    s[6] = game.ring / 5.0f;
    s[7] = fr.r(me.velocity) / 3.0f;
    s[8] = fr.t(me.velocity) / 3.0f;
    s[9] = me.velocity.y / 5.0f;
    s[10] = me.grounded ? 1.0f : 0.0f;
    s[11] = me.shoveTimer / rules.shoveCooldown;
    s[12] = me.stun / rules.stunTime;
    s[13] = me.seat >= 0 ? 1.0f : 0.0f;
    s[14] = me.locked ? 1.0f : me.lock / rules.lockTime;
    s[15] = me.stall / rules.stallTime;
    s[16] = game.inGrace() ? 1.0f : 0.0f;
    const Vec3 f = facing(me.yaw);
    s[17] = fr.r(f);
    s[18] = fr.t(f);
    s[19] = static_cast<float>(game.aliveCount()) / 10.0f;
    s[20] = static_cast<float>(game.chairs.size()) / 10.0f;
    s[21] = rules.ringFoul || rules.stallFoul ? 1.0f : 0.0f;

    std::vector<std::pair<float, int>> near;
    for (size_t j = 0; j < game.beans.size(); ++j) {
        const Contestant& o = game.beans[j];
        if (static_cast<int>(j) == self || !o.alive) continue;
        near.push_back({length2(o.position - me.position), static_cast<int>(j)});
    }
    std::sort(near.begin(), near.end());
    float* b = out + SELF;
    for (size_t k = 0; k < near.size() && k < NEAR_BEANS; ++k, b += BEAN) {
        const Contestant& o = game.beans[static_cast<size_t>(near[k].second)];
        const Vec3 d = o.position - me.position;
        const Vec3 dv = o.velocity - me.velocity;
        const float dist = std::max(length(flat(d)), 1e-3f);
        b[0] = 1.0f;
        b[1] = fr.r(d) / 5.0f;
        b[2] = fr.t(d) / 5.0f;
        b[3] = d.y;
        b[4] = fr.r(dv) / 3.0f;
        b[5] = fr.t(dv) / 3.0f;
        b[6] = o.seat >= 0 ? 1.0f : 0.0f;
        b[7] = o.locked ? 1.0f : o.lock / rules.lockTime;
        b[8] = o.shoveTimer / rules.shoveCooldown;
        b[9] = o.stun / rules.stunTime;
        b[10] = -dot(facing(o.yaw), flat(d) * (1.0f / dist));
    }

    std::vector<std::pair<float, int>> seats;
    for (size_t k = 0; k < game.chairs.size(); ++k)
        seats.push_back({length2(flat(game.chairs[k].position) - p), static_cast<int>(k)});
    std::sort(seats.begin(), seats.end());
    float* c = out + SELF + NEAR_BEANS * BEAN;
    for (size_t k = 0; k < seats.size() && k < NEAR_CHAIRS; ++k, c += CHAIR) {
        const Chair& ch = game.chairs[static_cast<size_t>(seats[k].second)];
        const Vec3 d = flat(ch.position) - p;
        c[0] = 1.0f;
        c[1] = fr.r(d) / 5.0f;
        c[2] = fr.t(d) / 5.0f;
        c[3] = fr.r(ch.outward);
        c[4] = fr.t(ch.outward);
        if (ch.occupant >= 0) {
            const Contestant& o = game.beans[static_cast<size_t>(ch.occupant)];
            c[5] = 1.0f;
            c[6] = o.locked ? 1.0f : 0.0f;
            c[7] = o.locked ? 1.0f : o.lock / rules.lockTime;
            c[8] = ch.occupant == self ? 1.0f : 0.0f;
        }
    }
}

Input act(const Game& game, int self, int move, bool jump, bool shove) {
    Input in;
    in.jump = jump;
    in.shove = shove;
    if (move <= 0 || move >= MOVES) return in;
    const Frame fr = frameAt(game.beans[static_cast<size_t>(self)].position);
    const float a = static_cast<float>(move - 1) * (PI / 4.0f);
    const Vec3 d = fr.radial * std::cos(a) + fr.tangent * std::sin(a);
    in.moveX = d.x;
    in.moveZ = d.z;
    return in;
}

int parseGen(const char* text) {
    if (std::strcmp(text, "best")) return std::atoi(text);
    const char* env = std::getenv("CHAIRS_GENS");
    FILE* f = std::fopen((std::string(env ? env : "gens") + "/best.json").c_str(), "r");
    if (!f) return 0;
    char buf[256] = {0};
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = 0;
    std::fclose(f);
    const char* k = std::strstr(buf, "\"gen\"");
    return k ? std::atoi(std::strchr(k, ':') + 1) : 0;
}

bool loadGen(int gen, Policy& policy, bool& fouls, std::string& error) {
    const char* env = std::getenv("CHAIRS_GENS");
    const std::string base = std::string(env ? env : "gens") + "/gen" + std::to_string(gen);
    if (!policy.load(base + ".bin", error)) {
        error = "gen " + std::to_string(gen) + " isn't saved (" + error + ")";
        return false;
    }
    fouls = true;
    if (FILE* f = std::fopen((base + ".json").c_str(), "r")) {
        char text[4096];
        const size_t n = std::fread(text, 1, sizeof(text) - 1, f);
        text[n] = 0;
        std::fclose(f);
        if (const char* k = std::strstr(text, "\"fouls\"")) fouls = std::strstr(k, "true") && std::strstr(k, "true") < std::strchr(k, ',');
    }
    return true;
}

bool Policy::load(const std::string& path, std::string& error) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        error = "can't open " + path;
        return false;
    }
    char magic[4];
    int32_t count = 0;
    bool ok = std::fread(magic, 1, 4, f) == 4 && !std::memcmp(magic, "CHB1", 4) && std::fread(&count, 4, 1, f) == 1;
    layers_.clear();
    for (int i = 0; ok && i < count; ++i) {
        Layer l;
        int32_t dims[2];
        ok = std::fread(dims, 4, 2, f) == 2;
        if (!ok) break;
        l.in = dims[0];
        l.out = dims[1];
        l.weight.resize(static_cast<size_t>(l.in) * static_cast<size_t>(l.out));
        l.bias.resize(static_cast<size_t>(l.out));
        ok = std::fread(l.weight.data(), 4, l.weight.size(), f) == l.weight.size() &&
             std::fread(l.bias.data(), 4, l.bias.size(), f) == l.bias.size();
        layers_.push_back(std::move(l));
    }
    std::fclose(f);
    if (!ok || layers_.empty() || layers_.front().in != OBS || layers_.back().out != LOGITS) {
        error = path + " isn't a chairs policy for this build";
        layers_.clear();
        return false;
    }
    return true;
}

void Policy::logits(const float* obs, float* out) const {
    std::vector<float> a(obs, obs + OBS), b;
    for (size_t i = 0; i < layers_.size(); ++i) {
        const Layer& l = layers_[i];
        b.assign(l.bias.begin(), l.bias.end());
        for (int o = 0; o < l.out; ++o) {
            const float* w = l.weight.data() + static_cast<size_t>(o) * static_cast<size_t>(l.in);
            float sum = 0;
            for (int k = 0; k < l.in; ++k) sum += w[k] * a[static_cast<size_t>(k)];
            b[static_cast<size_t>(o)] += sum;
        }
        if (i + 1 < layers_.size())
            for (float& v : b) v = std::tanh(v);
        a.swap(b);
    }
    std::copy(a.begin(), a.end(), out);
}

Input Policy::decide(const Game& game, int self, Random& random) const {
    float obs[OBS], z[LOGITS];
    observe(game, self, obs);
    logits(obs, z);
    auto sample = [&](const float* l, int n) {
        float m = l[0];
        for (int i = 1; i < n; ++i) m = std::max(m, l[i]);
        float p[MOVES], total = 0;
        for (int i = 0; i < n; ++i) total += p[i] = std::exp(l[i] - m);
        float u = random.unit() * total;
        for (int i = 0; i < n; ++i)
            if ((u -= p[i]) <= 0) return i;
        return n - 1;
    };
    const int move = sample(z, MOVES);
    const bool jump = sample(z + MOVES, 2) == 1;
    const bool shove = sample(z + MOVES + 2, 2) == 1;
    return act(game, self, move, jump, shove);
}

}  // namespace chairs::brain
