#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <ctime>
#include <filesystem>

#include "core/jobs.hpp"
#include "meadow.hpp"
#include "shots.hpp"
#include "stage.hpp"
#include "bots.hpp"
#include "names.hpp"
#include "view.hpp"
#include "take.hpp"
#include "audio.hpp"
#include "kit.hpp"
#include "render/gl.hpp"
#include "render/overlay.hpp"
#include "render/recorder.hpp"
#include "render/renderer.hpp"

using namespace chairs;

namespace {

struct Orbit {
    float angle = 0.6f;
    float pitch = 0.32f;
    float distance = 17.0f;
    Vec3 focus{0.0f, 1.0f, 0.0f};
    bool dragging = false;
    double lastX = 0, lastY = 0;
    bool spin = true;
    bool hud = false;
    bool tags = true;
};

Orbit orbit;

struct Fly {
    bool active = false;
    Vec3 eye;
    Vec3 velocity;
    float yaw = 0, pitch = 0, fov = 50.0f;
    float smoothYaw = 0, smoothPitch = 0, smoothFov = 50.0f;
    bool paused = false;
    bool start = false;
    bool toggleTake = false;
    bool locked = true;
    bool haveCursor = false;
    bool help = false;
};

Fly fly;

void keyCallback(GLFWwindow* window, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, GLFW_TRUE);
    if (key == GLFW_KEY_SPACE && fly.active) fly.paused = !fly.paused;
    else if (key == GLFW_KEY_SPACE) orbit.spin = !orbit.spin;
    if (key == GLFW_KEY_ENTER) fly.start = true;
    if (key == GLFW_KEY_R) fly.toggleTake = true;
    if (key == GLFW_KEY_SLASH) fly.help = !fly.help;
    if (key == GLFW_KEY_TAB && fly.active) {
        fly.locked = !fly.locked;
        fly.haveCursor = false;
        glfwSetInputMode(window, GLFW_CURSOR, fly.locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }
    if (key == GLFW_KEY_H) orbit.hud = !orbit.hud;
    if (key == GLFW_KEY_N) orbit.tags = !orbit.tags;
}

void buttonCallback(GLFWwindow* window, int button, int action, int) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    orbit.dragging = action == GLFW_PRESS;
    glfwGetCursorPos(window, &orbit.lastX, &orbit.lastY);
}

void cursorCallback(GLFWwindow*, double x, double y) {
    if (fly.active && fly.locked) {
        if (!fly.haveCursor) {
            fly.haveCursor = true;
            orbit.lastX = x;
            orbit.lastY = y;
            return;
        }
        orbit.dragging = true;
    }
    if (!orbit.dragging) return;
    if (fly.active) {
        fly.yaw -= static_cast<float>(x - orbit.lastX) * 0.0035f * fly.fov / 50.0f;
        fly.pitch = std::clamp(fly.pitch - static_cast<float>(y - orbit.lastY) * 0.0035f * fly.fov / 50.0f, -1.5f, 1.5f);
        orbit.lastX = x;
        orbit.lastY = y;
        return;
    }
    orbit.angle -= static_cast<float>(x - orbit.lastX) * 0.005f;
    orbit.pitch = std::clamp(orbit.pitch + static_cast<float>(y - orbit.lastY) * 0.004f, -0.05f, 1.4f);
    orbit.lastX = x;
    orbit.lastY = y;
}

void scrollCallback(GLFWwindow*, double, double dy) {
    if (fly.active) {
        fly.fov = std::clamp(fly.fov * std::pow(0.93f, static_cast<float>(dy)), 12.0f, 90.0f);
        return;
    }
    orbit.distance = std::clamp(orbit.distance * std::pow(0.9f, static_cast<float>(dy)), 3.0f, 120.0f);
}

float segmentDistance2(const Vec3& p1, const Vec3& q1, const Vec3& p2, const Vec3& q2) {
    const Vec3 d1 = q1 - p1, d2 = q2 - p2, r = p1 - p2;
    const float a = dot(d1, d1), e = dot(d2, d2), f = dot(d2, r), c = dot(d1, r), b = dot(d1, d2);
    const float denom = a * e - b * b;
    float s = denom > 1e-8f ? std::clamp((b * f - c * e) / denom, 0.0f, 1.0f) : 0.0f;
    float t = (b * s + f) / e;
    if (t < 0.0f) {
        t = 0.0f;
        s = std::clamp(-c / a, 0.0f, 1.0f);
    } else if (t > 1.0f) {
        t = 1.0f;
        s = std::clamp((b - c) / a, 0.0f, 1.0f);
    }
    return length2(p1 + d1 * s - (p2 + d2 * t));
}

void nametags(Overlay& overlay, Meadow& meadow, const Mat4& viewProj, const Vec3& camera, int w, int h, float dt,
              std::vector<float>& shown) {
    struct Tag {
        float depth, x, y, alpha;
        int id;
    };
    const size_t n = meadow.beans.size();
    shown.resize(n, 0.0f);
    std::vector<Vec3> base(n);
    for (size_t i = 0; i < n; ++i) {
        const Mat4& m = meadow.scene.world.get<WorldTransform>(meadow.beans[i].body).matrix;
        base[i] = Vec3{m.m[3][0], m.m[3][1], m.m[3][2]};
    }
    std::vector<Tag> tags;
    for (size_t i = 0; i < n; ++i) {
        const Vec3 face = base[i] + Vec3{0, 1.0f, 0};
        bool hidden = meadow.beans[i].hidden || meadow.beans[i].flying;
        for (size_t j = 0; j < n && !hidden; ++j) {
            if (j == i || length2(base[j] - camera) >= length2(base[i] - camera)) continue;
            hidden = segmentDistance2(camera, face, base[j] + Vec3{0, 0.42f, 0}, base[j] + Vec3{0, 1.08f, 0}) < 0.42f * 0.42f;
        }
        shown[i] = std::clamp(shown[i] + (hidden ? -dt : dt) * 6.0f, 0.0f, 1.0f);
        if (shown[i] <= 0.0f) continue;
        const Vec4 clip = viewProj * Vec4{base[i].x, base[i].y + 1.62f, base[i].z, 1.0f};
        if (clip.w < 0.5f || clip.w > 45.0f) continue;
        const float fade = std::clamp((45.0f - clip.w) / 10.0f, 0.0f, 1.0f) * shown[i];
        tags.push_back(Tag{clip.w, (clip.x / clip.w * 0.5f + 0.5f) * static_cast<float>(w),
                           (0.5f - clip.y / clip.w * 0.5f) * static_cast<float>(h), fade, meadow.beans[i].id});
    }
    std::sort(tags.begin(), tags.end(), [](const Tag& a, const Tag& b) { return a.depth > b.depth; });
    const float scale = static_cast<float>(h) / 1080.0f;
    for (const Tag& t : tags) {
        const float size = std::clamp(420.0f / t.depth, 14.0f, 44.0f) * scale;
        const char* name = Meadow::beanName(t.id);
        const Vec2 extent = overlay.measure(name, size);
        const Vec3 c = Meadow::beanColor(t.id) * 0.35f;
        overlay.text(name, t.x - extent.x * 0.5f, t.y - extent.y, size, Vec4{1, 1, 1, t.alpha}, size * 0.12f,
                     Vec4{c.x, c.y, c.z, t.alpha});
    }
}

void style(Renderer& renderer, const Palette& p) {
    RenderOptions& o = renderer.options;
    o.toon = 1.0f;
    o.rim = 0.3f;
    o.skyGradient = true;
    o.skyZenith = p.skyZenith;
    o.skyHorizon = p.skyHorizon;
    o.skyGround = p.skyGround;
    o.skyColor = p.skyHorizon;
    o.ambientSky = Vec3{0.46f, 0.52f, 0.66f};
    o.ambientGround = Vec3{0.26f, 0.28f, 0.16f};
    o.fogDensity = 0.004f;
    o.shadowExtent = 26.0f;
    o.shadowDistance = 80.0f;
    o.post.exposure = 0.72f;
    o.post.saturation = 1.3f;
    o.post.contrast = 1.08f;
    o.post.outline = 1.0f;
    o.post.ssaoAmount = 0.6f;
    o.post.bloomThreshold = 2.5f;
}

}  // namespace

int main(int argc, char** argv) {
    int width = 0, height = 0, frames = 0, beans = 8;
    uint32_t seed = 3;
    const char* record = nullptr;
    int lineup = 0;
    std::string shotName, sceneName, cast, focusName;
    bool playGame = false;
    int empty = 0;
    std::vector<int> recordRounds;
    int players = 6;
    bool gen0 = false;
    int gen = -1;
    Take replay;
    bool replaying = false;
    float warmup = 0.0f;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--resolution") && i + 1 < argc) std::sscanf(argv[++i], "%dx%d", &width, &height);
        else if (!std::strcmp(argv[i], "--record") && i + 1 < argc) record = argv[++i];
        else if (!std::strcmp(argv[i], "--frames") && i + 1 < argc) frames = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--seed") && i + 1 < argc) seed = static_cast<uint32_t>(std::atoi(argv[++i]));
        else if (!std::strcmp(argv[i], "--beans") && i + 1 < argc) beans = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--angle") && i + 1 < argc) orbit.angle = std::strtof(argv[++i], nullptr);
        else if (!std::strcmp(argv[i], "--pitch") && i + 1 < argc) orbit.pitch = std::strtof(argv[++i], nullptr);
        else if (!std::strcmp(argv[i], "--distance") && i + 1 < argc) orbit.distance = std::strtof(argv[++i], nullptr);
        else if (!std::strcmp(argv[i], "--still")) orbit.spin = false;
        else if (!std::strcmp(argv[i], "--lineup")) lineup = 1000;
        else if (!std::strcmp(argv[i], "--grid") && i + 1 < argc) lineup = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--hud")) orbit.hud = true;
        else if (!std::strcmp(argv[i], "--warmup") && i + 1 < argc) warmup = std::strtof(argv[++i], nullptr);
        else if (!std::strcmp(argv[i], "--shot") && i + 1 < argc) shotName = argv[++i];
        else if (!std::strcmp(argv[i], "--scene") && i + 1 < argc) sceneName = argv[++i];
        else if (!std::strcmp(argv[i], "--no-tags")) orbit.tags = false;
        else if (!std::strcmp(argv[i], "--game")) playGame = true;
        else if (!std::strcmp(argv[i], "--players") && i + 1 < argc) players = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--gen") && i + 1 < argc) gen = brain::parseGen(argv[++i]);
        else if (!std::strcmp(argv[i], "--replay") && i + 1 < argc) {
            if (!replay.load(argv[++i])) {
                std::fprintf(stderr, "can't read take %s\n", argv[i]);
                return 1;
            }
            replaying = playGame = true;
            seed = replay.seed;
            gen = replay.gen;
        }
        else if (!std::strcmp(argv[i], "--cast") && i + 1 < argc) cast = argv[++i];
        else if (!std::strcmp(argv[i], "--focus") && i + 1 < argc) focusName = argv[++i];
        else if (!std::strcmp(argv[i], "--empty") && i + 1 < argc) empty = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--rounds") && i + 1 < argc) {
            const char* r = argv[++i];
            while (*r) {
                recordRounds.push_back(std::atoi(r));
                while (*r && *r != ',') ++r;
                if (*r == ',') ++r;
            }
        }
    }
    brain::Policy policy;
    brain::GenInfo genInfo;
    if (gen > 0) {
        std::string error;
        if (!loadGen(gen, policy, genInfo, error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
        playGame = true;
    }
    if (gen == 0) gen0 = playGame = true;
    std::vector<int> castIds = replay.cast;
    if (replaying) players = static_cast<int>(castIds.size());
    if (playGame && !replaying) {
        size_t start = 0;
        while (!cast.empty() && start <= cast.size()) {
            const size_t comma = cast.find(',', start);
            const std::string name = cast.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
            int found = -1;
            for (int k = 0; k < NAME_COUNT; ++k)
                if (name == NAMES[k]) found = k;
            if (found < 0) {
                std::fprintf(stderr, "no bean named %s\n", name.c_str());
                return 1;
            }
            castIds.push_back(found);
            if (comma == std::string::npos) break;
            start = comma + 1;
        }
        Random pick(seed * 31 + 7);
        while (static_cast<int>(castIds.size()) < players) {
            const int id = static_cast<int>(pick.below(NAME_COUNT));
            if (std::find(castIds.begin(), castIds.end(), id) == castIds.end()) castIds.push_back(id);
        }
    }

    const Routine* script = nullptr;
    if (!sceneName.empty() && !(script = findRoutine(sceneName))) {
        std::fprintf(stderr, "unknown scene %s; scenes: round final\n", sceneName.c_str());
        return 1;
    }
    const Shot* shot = nullptr;
    if (!shotName.empty()) {
        const auto sceneShots = script ? Stage::shots(sceneName)
                                : playGame ? std::vector<std::string>{"wide", "top", "low", "chairs", "follow", "action"}
                                           : std::vector<std::string>{};
        const bool sceneShot = std::find(sceneShots.begin(), sceneShots.end(), shotName) != sceneShots.end();
        if (!sceneShot && !(shot = findShot(shotName))) {
            std::fprintf(stderr, "unknown shot %s; shots:", shotName.c_str());
            for (const auto& name : sceneShots) std::fprintf(stderr, " %s", name.c_str());
            if (!script)
                for (const Shot& s : shots()) std::fprintf(stderr, " %s", s.name.c_str());
            std::fprintf(stderr, "\n");
            return 1;
        }
    }

    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(1600, 900, "chairs", nullptr, nullptr);
    if (!window) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(record ? 0 : 1);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, buttonCallback);
    glfwSetCursorPosCallback(window, cursorCallback);
    glfwSetScrollCallback(window, scrollCallback);

    JobSystem jobs;
    Renderer renderer;
    std::string error;
    if (!renderer.init(error)) {
        std::fprintf(stderr, "renderer: %s\n", error.c_str());
        return 1;
    }
    Meadow meadow;
    if (script) meadow.build(seed, script->beans);
    else if (playGame) meadow.build(seed, castIds);
    else meadow.build(seed, empty > 0 ? 0 : beans);
    if (empty > 0) {
        const uint32_t mesh = meadow.assets.addMesh(kit::chair(Vec3{0.78f, 0.52f, 0.32f}, Vec3{0.92f, 0.3f, 0.32f}));
        Material wood;
        wood.name = "chair";
        wood.roughness = 0.85f;
        const uint32_t material = meadow.assets.addMaterial(wood);
        const float ring = std::max(1.6f, static_cast<float>(empty) * 1.7f / (2.0f * PI));
        for (int i = 0; i < empty; ++i) {
            const float a = static_cast<float>(i) / static_cast<float>(empty) * 2.0f * PI;
            const Vec3 out{std::cos(a), 0, std::sin(a)};
            meadow.place(mesh, material, out * ring, std::atan2(out.x, out.z), 1.0f);
        }
        meadow.scene.updateTransforms(nullptr);
    }
    if (lineup > 0) meadow.lineup(lineup);
    std::unique_ptr<Stage> stage;
    if (script) {
        stage = std::make_unique<Stage>(meadow, *script);
        renderer.particles = {&stage->fx.confetti, &stage->fx.sparks};
        meadow.scene.updateTransforms(nullptr);
    }
    std::unique_ptr<Game> game;
    std::unique_ptr<GameView> view;
    std::vector<Bot> bots;
    std::vector<Input> inputs;
    Random botRandom(seed * 1000 + 1);
    int focus = 0;
    if (playGame) {
        Rules rules = rulesFor(gen > 0 ? genInfo.rules : 1);
        rules.ringFoul = rules.stallFoul = !gen0 && genInfo.fouls;
        game = std::make_unique<Game>(castIds, seed, rules);
        view = std::make_unique<GameView>(meadow, *game);
        renderer.particles = {&view->fx.confetti, &view->fx.sparks};
        for (size_t i = 0; i < castIds.size(); ++i) bots.push_back(gen > 0 ? Bot::trained(policy) : gen0 ? Bot::fresh() : Bot::random(botRandom));
        inputs.resize(castIds.size());
        for (size_t i = 0; i < castIds.size(); ++i)
            if (!focusName.empty() && focusName == NAMES[castIds[i]]) focus = static_cast<int>(i);
        meadow.update(0.0f);
    }
    renderer.uploadAssets(meadow.assets);
    style(renderer, meadow.palette);

    for (float t = 0; t < warmup; t += 1.0f / 60.0f) meadow.update(1.0f / 60.0f);
    if (shot && frames <= 0) frames = static_cast<int>(shot->seconds * 60.0f + 0.5f);
    if (stage && frames <= 0) frames = static_cast<int>(stage->script.length * 60.0f + 0.5f);
    const int substeps = stage || game ? 4 : 1;

    Overlay overlay;
    if (!overlay.init("", error)) std::fprintf(stderr, "overlay: %s\n", error.c_str());

    int w = width, h = height;
    if (w <= 0 || h <= 0) glfwGetFramebufferSize(window, &w, &h);
    VideoRecorder recorder;
    if (record && !recorder.open(record, w, h, 60, error)) std::fprintf(stderr, "record: %s\n", error.c_str());
    FILE* cues = nullptr;
    if (record && game) {
        std::string path = record;
        const size_t dot = path.find_last_of('.');
        cues = std::fopen((path.substr(0, dot) + ".cues").c_str(), "w");
    }
    int captured = 0;
    std::vector<char> airborne, gone;
    bool playing = false;

    const bool interactive = game && !record && shotName.empty() && !replaying;
    fly.active = interactive;
    if (interactive) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if (glfwRawMouseMotionSupported()) glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }
    if (interactive) {
        fly.eye = orbit.focus + Vec3{std::sin(orbit.angle) * std::cos(orbit.pitch), std::sin(orbit.pitch),
                                     std::cos(orbit.angle) * std::cos(orbit.pitch)} *
                                    orbit.distance;
        const Vec3 d = normalize(orbit.focus - fly.eye);
        fly.yaw = fly.smoothYaw = std::atan2(d.x, d.z);
        fly.pitch = fly.smoothPitch = std::asin(d.y);
    }
    const float stepDt = 1.0f / 240.0f;
    long stepsDone = 0;
    bool holding = interactive;
    int heldRound = 1;
    double accumulator = 0;
    Take take;
    bool taking = false;
    double takeStart = 0;
    std::string takePath;
    const double replayEnd = replay.length();
    Audio audio;
    if (interactive) {
        std::string audioError;
        if (!audio.init("sounds", audioError)) std::fprintf(stderr, "audio: %s\n", audioError.c_str());
    }

    auto inRounds = [&]() {
        return recordRounds.empty() || std::find(recordRounds.begin(), recordRounds.end(), game->round) != recordRounds.end();
    };
    auto finishTake = [&]() {
        if (!taking) return;
        taking = false;
        std::filesystem::create_directories("takes");
        if (take.save(takePath)) std::printf("saved %s (%.1f s)\n", takePath.c_str(), take.length());
        else std::fprintf(stderr, "couldn't write %s\n", takePath.c_str());
        std::fflush(stdout);
    };

    std::vector<float> shown;
    double last = glfwGetTime();
    float time = 0;
    for (int frame = 0; !glfwWindowShouldClose(window) && (frames <= 0 || frame < frames); ++frame) {
        if (game && game->phase == Game::Over && view->overTime() > 4.0f && record && !replaying) break;
        if (replaying && frame / 60.0 > replayEnd) break;
        if (game && record && !recordRounds.empty() && game->round > *std::max_element(recordRounds.begin(), recordRounds.end())) break;
        double now = glfwGetTime();
        const float frameDt = record ? 1.0f / 60.0f : static_cast<float>(std::min(now - last, 0.1));
        last = now;
        glfwPollEvents();

        int steps = 0;
        float idle = 0;
        char mode = 'r';
        TakeFrame shotFrame;
        if (replaying) {
            shotFrame = replay.at(frame / 60.0);
            steps = static_cast<int>(std::max(0L, shotFrame.steps - stepsDone));
            mode = shotFrame.mode;
            if (mode == 'h') idle = frameDt;
            orbit.tags = shotFrame.tags;
        } else if (interactive) {
            if (fly.start) holding = fly.paused = false;
            fly.start = false;
            if (fly.toggleTake) {
                fly.toggleTake = false;
                if (taking) {
                    finishTake();
                } else {
                    char stamp[32];
                    const std::time_t clock = std::time(nullptr);
                    std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", std::localtime(&clock));
                    takePath = "takes/" + (gen >= 0 ? "gen" + std::to_string(gen) : std::string("bots")) + "-seed" + std::to_string(seed) + "-" + stamp + ".take";
                    take = Take{};
                    take.seed = seed;
                    take.gen = gen;
                    take.cast = castIds;
                    taking = true;
                    takeStart = now;
                }
            }
            if (fly.paused) {
                mode = 'p';
            } else if (holding) {
                mode = 'h';
                idle = frameDt;
            } else {
                accumulator += frameDt / stepDt;
                steps = std::min(static_cast<int>(accumulator), 24);
                accumulator -= static_cast<int>(accumulator);
            }
        } else if (game) {
            steps = substeps;
        }

        if (game) {
            for (int step = 0; step < steps; ++step) {
                for (size_t i = 0; i < bots.size(); ++i) inputs[i] = bots[i].think(*game, static_cast<int>(i), botRandom);
                game->step(inputs);
                ++stepsDone;
                time += stepDt;
                const bool logging = cues && inRounds() && !(replaying && frame == 0);
                if (logging || audio.ready()) {
                    const double t = (captured - static_cast<double>(steps - 1 - step) / steps) / 60.0;
                    for (const Event& e : game->events) {
                        if (logging) std::fprintf(cues, "ev %.4f %d %d %d\n", t, static_cast<int>(e.kind), e.a, e.b);
                        audio.event(e, *game);
                    }
                    airborne.resize(game->beans.size());
                    gone.resize(game->beans.size());
                    for (size_t i = 0; i < game->beans.size(); ++i) {
                        const Contestant& c = game->beans[i];
                        const bool up = !c.grounded && c.velocity.y > 1.0f;
                        if (up && !airborne[i] && c.alive) {
                            if (logging) std::fprintf(cues, "jump %.4f %zu\n", t, i);
                            audio.jump(static_cast<int>(i), game->time);
                        }
                        airborne[i] = !c.grounded;
                        if (meadow.beans[i].hidden && !gone[i]) {
                            if (logging) std::fprintf(cues, "twinkle %.4f %zu\n", t, i);
                            audio.twinkle();
                        }
                        gone[i] = meadow.beans[i].hidden;
                    }
                }
                view->sync(stepDt);
                meadow.update(stepDt);
                if (interactive && game->round != heldRound && game->phase == Game::Music) {
                    heldRound = game->round;
                    holding = true;
                    accumulator = 0;
                    break;
                }
            }
            if (idle > 0) {
                meadow.update(idle);
                time += idle;
            }
            audio.music(game->phase == Game::Music && mode == 'r', *game);
        } else {
            for (int step = 0; step < substeps; ++step) {
                const float dt = frameDt / static_cast<float>(substeps);
                time += dt;
                if (orbit.spin && !shot && !stage) orbit.angle += dt * 0.08f;
                if (stage) stage->update(dt);
                else meadow.update(dt);
            }
        }
        if (game && !inRounds()) continue;

        int windowWidth = 0, windowHeight = 0;
        glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
        if (width <= 0 || height <= 0) {
            if (interactive) glfwGetWindowSize(window, &w, &h);
            else {
                w = windowWidth;
                h = windowHeight;
            }
        }
        renderer.resize(w, h);
        renderer.options.time = time;

        Vec3 eye = orbit.focus + Vec3{std::sin(orbit.angle) * std::cos(orbit.pitch), std::sin(orbit.pitch),
                                      std::cos(orbit.angle) * std::cos(orbit.pitch)} *
                                     orbit.distance;
        Vec3 target = orbit.focus;
        float fov = 50.0f;
        CameraKey k;
        if (shot) k = shot->at(time);
        if (replaying) {
            eye = shotFrame.eye;
            target = shotFrame.target;
            fov = shotFrame.fov;
        } else if (fly.active) {
            const float ease = 1.0f - std::exp(-frameDt * 10.0f);
            fly.smoothYaw += (fly.yaw - fly.smoothYaw) * ease;
            fly.smoothPitch += (fly.pitch - fly.smoothPitch) * ease;
            fly.smoothFov += (fly.fov - fly.smoothFov) * ease;
            const Vec3 forward{std::sin(fly.smoothYaw) * std::cos(fly.smoothPitch), std::sin(fly.smoothPitch),
                               std::cos(fly.smoothYaw) * std::cos(fly.smoothPitch)};
            const Vec3 right = normalize(cross(forward, Vec3{0, 1, 0}));
            auto key = [&](int k) { return glfwGetKey(window, k) == GLFW_PRESS ? 1.0f : 0.0f; };
            Vec3 want = forward * (key(GLFW_KEY_W) - key(GLFW_KEY_S)) + right * (key(GLFW_KEY_D) - key(GLFW_KEY_A)) +
                        Vec3{0, 1, 0} * (key(GLFW_KEY_E) - key(GLFW_KEY_Q));
            float speed = 5.0f;
            if (key(GLFW_KEY_LEFT_SHIFT) > 0) speed *= 3.0f;
            if (key(GLFW_KEY_LEFT_ALT) > 0 || key(GLFW_KEY_LEFT_CONTROL) > 0) speed *= 0.25f;
            if (length(want) > 1e-3f) want = normalize(want) * speed;
            fly.velocity = fly.velocity + (want - fly.velocity) * (1.0f - std::exp(-frameDt * 6.0f));
            fly.eye = fly.eye + fly.velocity * frameDt;
            fly.eye.y = std::max(fly.eye.y, meadow.height(fly.eye.x, fly.eye.z) + 0.25f);
            eye = fly.eye;
            target = fly.eye + forward;
            fov = fly.smoothFov;
        } else if (shot || (stage && !shotName.empty() && stage->camera(shotName, k)) ||
                   (view && !shotName.empty() && view->camera(shotName, focus, k))) {
            eye = k.eye;
            target = k.target;
            fov = k.fov;
        }
        const Mat4 view = lookAt(eye, target, Vec3{0, 1, 0});
        const Mat4 projection = perspective(radians(fov), static_cast<float>(w) / static_cast<float>(h), 0.1f, 600.0f);
        renderer.render(meadow.scene, meadow.assets, view, projection, eye, &jobs);

        if (orbit.tags && overlay.font().loaded()) nametags(overlay, meadow, projection * view, eye, w, h, frameDt, shown);
        const float scale = static_cast<float>(h) / 1080.0f;
        if (orbit.hud && overlay.font().loaded()) {
            const RenderStats& s = renderer.stats();
            char text[256];
            std::snprintf(text, sizeof(text), "%.2f ms gpu  %u draws  %u / %u visible  %.1fM tris", s.gpuMs,
                          s.drawCalls + s.shadowDrawCalls, s.visible, s.candidates,
                          static_cast<double>(s.triangles) * 1e-6);
            overlay.text(text, 24 * scale, 24 * scale, 26 * scale, Vec4{1, 1, 1, 1}, 3 * scale);
        }
        if (interactive && overlay.font().loaded()) {
            char text[128];
            float y = 60 * scale;
            if (taking) {
                const int secs = static_cast<int>(now - takeStart);
                std::snprintf(text, sizeof(text), "REC %d:%02d", secs / 60, secs % 60);
                overlay.text(text, 24 * scale, y, 34 * scale, Vec4{1.0f, 0.25f, 0.2f, 1}, 4 * scale);
                y += 44 * scale;
            }
            if (fly.paused) {
                overlay.text("PAUSED", 24 * scale, y, 30 * scale, Vec4{1, 1, 1, 1}, 4 * scale);
                y += 40 * scale;
            } else if (holding) {
                std::snprintf(text, sizeof(text), fly.help ? "Enter starts round %d" : "Enter starts round %d   (? for controls)", game->round);
                overlay.text(text, 24 * scale, y, 30 * scale, Vec4{1, 0.95f, 0.6f, 1}, 4 * scale);
                y += 40 * scale;
            }
            if (fly.help)
                overlay.text("WASD move  Q/E down/up  mouse look  Tab free cursor  scroll zoom  Shift fast  Alt slow  Enter start round  Space pause  R record  N names  ? hide help",
                         24 * scale, static_cast<float>(h) - 40 * scale, 20 * scale, Vec4{1, 1, 1, 0.85f}, 3 * scale);
        }
        overlay.flush(renderer.outputFramebuffer(), w, h);

        if (taking) take.frames.push_back(TakeFrame{now - takeStart, stepsDone, mode, eye, target, fov, orbit.tags});
        recorder.capture(renderer.outputFramebuffer());
        if (cues && (game->phase == Game::Music && steps > 0) != playing) {
            playing = !playing;
            if (playing)
                std::fprintf(cues, "play %.4f %.4f %d %d\n", captured / 60.0, game->phaseTime, game->round, game->aliveCount());
            else
                std::fprintf(cues, "stop %.4f\n", captured / 60.0);
        }
        ++captured;
        renderer.present(windowWidth, windowHeight);
        glfwSwapBuffers(window);
    }
    finishTake();
    if (game && game->winner >= 0) std::printf("winner %s after %d rounds\n", NAMES[castIds[static_cast<size_t>(game->winner)]], game->round);
    if (stage && stage->loser >= 0)
        std::printf("loser %s\n", Meadow::beanName(meadow.beans[stage->players[static_cast<size_t>(stage->loser)].bean].id));
    recorder.close();
    if (cues) {
        std::fprintf(cues, "end %.4f\n", captured / 60.0);
        std::fclose(cues);
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
