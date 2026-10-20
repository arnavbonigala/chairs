#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "core/jobs.hpp"
#include "meadow.hpp"
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
};

Orbit orbit;

void keyCallback(GLFWwindow* window, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, GLFW_TRUE);
    if (key == GLFW_KEY_SPACE) orbit.spin = !orbit.spin;
    if (key == GLFW_KEY_H) orbit.hud = !orbit.hud;
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
    bool lineup = false;
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
        else if (!std::strcmp(argv[i], "--lineup")) lineup = true;
        else if (!std::strcmp(argv[i], "--hud")) orbit.hud = true;
    }

    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(1600, 900, "chairs", nullptr, nullptr);
    if (!window) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
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
    meadow.build(seed, beans);
    if (lineup) meadow.lineup();
    renderer.uploadAssets(meadow.assets);
    style(renderer, meadow.palette);

    Overlay overlay;
    if (!overlay.init("", error)) std::fprintf(stderr, "overlay: %s\n", error.c_str());

    VideoRecorder recorder;
    if (record) {
        int w = width, h = height;
        if (w <= 0 || h <= 0) glfwGetFramebufferSize(window, &w, &h);
        if (!recorder.open(record, w, h, 60, error)) std::fprintf(stderr, "record: %s\n", error.c_str());
    }

    double last = glfwGetTime();
    float time = 0;
    for (int frame = 0; !glfwWindowShouldClose(window) && (frames <= 0 || frame < frames); ++frame) {
        double now = glfwGetTime();
        float dt = record ? 1.0f / 60.0f : static_cast<float>(std::min(now - last, 0.1));
        last = now;
        time += dt;
        glfwPollEvents();
        if (orbit.spin) orbit.angle += dt * 0.08f;
        meadow.update(dt);

        int windowWidth = 0, windowHeight = 0;
        glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
        const int w = width > 0 ? width : windowWidth, h = height > 0 ? height : windowHeight;
        renderer.resize(w, h);
        renderer.options.time = time;

        const Vec3 eye = orbit.focus + Vec3{std::sin(orbit.angle) * std::cos(orbit.pitch), std::sin(orbit.pitch),
                                            std::cos(orbit.angle) * std::cos(orbit.pitch)} *
                                           orbit.distance;
        const Mat4 view = lookAt(eye, orbit.focus, Vec3{0, 1, 0});
        const Mat4 projection = perspective(radians(50.0f), static_cast<float>(w) / static_cast<float>(h), 0.1f, 600.0f);
        renderer.render(meadow.scene, meadow.assets, view, projection, eye, &jobs);

        if (orbit.hud && overlay.font().loaded()) {
            const RenderStats& s = renderer.stats();
            char text[256];
            std::snprintf(text, sizeof(text), "%.2f ms gpu  %u draws  %u / %u visible  %.1fM tris", s.gpuMs,
                          s.drawCalls + s.shadowDrawCalls, s.visible, s.candidates, static_cast<double>(s.triangles) * 1e-6);
            const float scale = static_cast<float>(h) / 1080.0f;
            overlay.text(text, 24 * scale, 24 * scale, 26 * scale, Vec4{1, 1, 1, 1}, 3 * scale);
            overlay.flush(renderer.outputFramebuffer(), w, h);
        }

        recorder.capture(renderer.outputFramebuffer());
        renderer.present(windowWidth, windowHeight);
        glfwSwapBuffers(window);
    }
    recorder.close();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
