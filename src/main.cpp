#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>

#include "core/jobs.hpp"
#include "meadow.hpp"
#include "shots.hpp"
#include "stage.hpp"
#include "bots.hpp"
#include "names.hpp"
#include "view.hpp"
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

void keyCallback(GLFWwindow* window, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, GLFW_TRUE);
    if (key == GLFW_KEY_SPACE) orbit.spin = !orbit.spin;
    if (key == GLFW_KEY_H) orbit.hud = !orbit.hud;
    if (key == GLFW_KEY_N) orbit.tags = !orbit.tags;
}

void buttonCallback(GLFWwindow* window, int button, int action, int) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    orbit.dragging = action == GLFW_PRESS;
    glfwGetCursorPos(window, &orbit.lastX, &orbit.lastY);
}

void cursorCallback(GLFWwindow*, double x, double y) {
    if (!orbit.dragging) return;
    orbit.angle -= static_cast<float>(x - orbit.lastX) * 0.005f;
    orbit.pitch = std::clamp(orbit.pitch + static_cast<float>(y - orbit.lastY) * 0.004f, -0.05f, 1.4f);
    orbit.lastX = x;
    orbit.lastY = y;
}

void scrollCallback(GLFWwindow*, double, double dy) {
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
    std::vector<int> castIds;
    if (playGame) {
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
        game = std::make_unique<Game>(castIds, seed);
        view = std::make_unique<GameView>(meadow, *game);
        renderer.particles = {&view->fx.confetti, &view->fx.sparks};
        for (size_t i = 0; i < castIds.size(); ++i) bots.push_back(Bot::random(botRandom));
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

    std::vector<float> shown;
    double last = glfwGetTime();
    float time = 0, sinceRender = 0;
    for (int frame = 0; !glfwWindowShouldClose(window) && (frames <= 0 || frame < frames); ++frame) {
        if (game && game->phase == Game::Over && view->overTime() > 4.0f && record) break;
        if (game && record && !recordRounds.empty() && game->round > *std::max_element(recordRounds.begin(), recordRounds.end())) break;
        double now = glfwGetTime();
        const float frameDt = record ? 1.0f / 60.0f : static_cast<float>(std::min(now - last, 0.1));
        last = now;
        glfwPollEvents();
        for (int step = 0; step < substeps; ++step) {
            const float dt = frameDt / static_cast<float>(substeps);
            time += dt;
            sinceRender += dt;
            if (orbit.spin && !shot && !stage) orbit.angle += dt * 0.08f;
            if (stage) {
                stage->update(dt);
            } else if (game) {
                for (size_t i = 0; i < bots.size(); ++i) inputs[i] = bots[i].think(*game, static_cast<int>(i), botRandom);
                game->step(inputs);
                view->sync(dt);
                meadow.update(dt);
            } else {
                meadow.update(dt);
            }

            const bool wanted = !game || recordRounds.empty() ||
                                std::find(recordRounds.begin(), recordRounds.end(), game->round) != recordRounds.end();
            if (step != substeps - 1 || !wanted) continue;

            int windowWidth = 0, windowHeight = 0;
            glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
            if (width <= 0 || height <= 0) {
                w = windowWidth;
                h = windowHeight;
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
            if (shot || (stage && !shotName.empty() && stage->camera(shotName, k)) ||
                (view && !shotName.empty() && view->camera(shotName, focus, k))) {
                eye = k.eye;
                target = k.target;
                fov = k.fov;
            }
            const Mat4 view = lookAt(eye, target, Vec3{0, 1, 0});
            const Mat4 projection = perspective(radians(fov), static_cast<float>(w) / static_cast<float>(h), 0.1f, 600.0f);
            renderer.render(meadow.scene, meadow.assets, view, projection, eye, &jobs);

            if (orbit.tags && overlay.font().loaded())
                nametags(overlay, meadow, projection * view, eye, w, h, sinceRender, shown);
            sinceRender = 0;
            if (orbit.hud && overlay.font().loaded()) {
                const RenderStats& s = renderer.stats();
                char text[256];
                std::snprintf(text, sizeof(text), "%.2f ms gpu  %u draws  %u / %u visible  %.1fM tris", s.gpuMs,
                              s.drawCalls + s.shadowDrawCalls, s.visible, s.candidates,
                              static_cast<double>(s.triangles) * 1e-6);
                const float scale = static_cast<float>(h) / 1080.0f;
                overlay.text(text, 24 * scale, 24 * scale, 26 * scale, Vec4{1, 1, 1, 1}, 3 * scale);
            }
            overlay.flush(renderer.outputFramebuffer(), w, h);

            recorder.capture(renderer.outputFramebuffer());
            renderer.present(windowWidth, windowHeight);
            glfwSwapBuffers(window);
        }
    }
    if (game && game->winner >= 0) std::printf("winner %s after %d rounds\n", NAMES[castIds[static_cast<size_t>(game->winner)]], game->round);
    if (stage && stage->loser >= 0)
        std::printf("loser %s\n", Meadow::beanName(meadow.beans[stage->players[static_cast<size_t>(stage->loser)].bean].id));
    recorder.close();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
