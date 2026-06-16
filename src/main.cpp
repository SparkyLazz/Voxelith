#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#pragma warning(push)
#pragma warning(disable : 4100 4127 4201 4244 4245 4267 4305)
#include <imgui.h>
#pragma warning(pop)

#include <spdlog/spdlog.h>

#include "camera.h"
#include "input.h"
#include "renderer.h"
#include "voxel/model.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>

static void glfwErrorCallback(int error, const char* description) {
    spdlog::error("GLFW error {}: {}", error, description);
}

// File-static used by framebufferResizeCallback. Input's constructor takes the
// window user pointer for its own callbacks, so we can't stash Renderer there.
static Renderer* s_renderer = nullptr;

static void framebufferResizeCallback(GLFWwindow*, [[maybe_unused]] int w,
                                      [[maybe_unused]] int h) {
    if (s_renderer) s_renderer->notifyResize();
}

int main() {
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    spdlog::set_level(spdlog::level::debug);

#ifdef VOXEL_SMOKE_TEST
    {
        vox::Model m;
        m.name = "smoke_test";
        m.palette.materials[1].albedo = {0.8f, 0.2f, 0.2f};
        vox::Chunk chunk;
        chunk.set(5, 5, 5, 1);
        m.chunks[glm::ivec3{0, 0, 0}] = chunk;
        m.recompute_bounds();
        spdlog::info("[smoke] name={} id={} chunks={} voxels={} bounds=[{},{},{}]-[{},{},{}]",
                     m.name, m.id.to_string(), m.chunk_count(), m.non_empty_voxel_count(),
                     m.bounds_min.x, m.bounds_min.y, m.bounds_min.z,
                     m.bounds_max.x, m.bounds_max.y, m.bounds_max.z);
    }
#endif

    glfwSetErrorCallback(glfwErrorCallback);

    if (glfwInit() == GLFW_FALSE) {
        spdlog::critical("glfwInit() failed");
        return EXIT_FAILURE;
    }

    spdlog::info("GLFW {}", glfwGetVersionString());

    if (glfwVulkanSupported() == GLFW_FALSE) {
        spdlog::critical("Vulkan is not supported by this GLFW build or driver");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(1280, 720,
                                          "VoxelEditor \xe2\x80\x94 Phase 0",
                                          nullptr, nullptr);
    if (window == nullptr) {
        spdlog::critical("glfwCreateWindow() failed");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

    // Input installs its GLFW callbacks first. Renderer's initImGui (called
    // inside the Renderer constructor) then chains on top of them, so ImGui
    // sees events first and sets WantCapture* flags before our handlers run.
    std::unique_ptr<Input>    input;
    std::unique_ptr<Renderer> renderer;
    Camera camera;

    try {
        input    = std::make_unique<Input>(window);
        renderer = std::make_unique<Renderer>(window);
    } catch (const std::exception& e) {
        spdlog::critical("Startup failed: {}", e.what());
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    s_renderer = renderer.get();
    spdlog::info("Entering main loop");

    auto lastTime = std::chrono::steady_clock::now();

    while (glfwWindowShouldClose(window) == GLFW_FALSE) {
        input->beginFrame();    // reset per-frame mouse deltas before event pump
        glfwPollEvents();

        if (input->state().keyEscape) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        const auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;
        dt = std::min(dt, 0.1f);  // clamp huge dt (e.g. debugger pauses)

        // Feed camera a copy of input with ImGui-captured axes zeroed.
        InputState camInput = input->state();
        const ImGuiIO& io   = ImGui::GetIO();
        if (io.WantCaptureKeyboard) {
            camInput.keyW = camInput.keyA = camInput.keyS = camInput.keyD =
            camInput.keyE = camInput.keyQ = camInput.keyShift = false;
        }
        if (io.WantCaptureMouse) {
            camInput.mouseDeltaX = camInput.mouseDeltaY = 0.0f;
        }

        camera.update(camInput, dt);

        const glm::vec3 pos = camera.position();
        renderer->beginImGuiFrame({pos.x, pos.y, pos.z});

        glm::mat4 view = camera.viewMatrix();
        Renderer::ViewMatrix vm{};
        std::memcpy(vm.m, glm::value_ptr(view), sizeof(vm.m));
        renderer->drawFrame(vm);
    }

    s_renderer = nullptr;
    renderer.reset();   // GPU idle + subsystem teardown
    input.reset();      // cursor mode restored

    glfwDestroyWindow(window);
    glfwTerminate();

    spdlog::info("Shutdown clean");
    return EXIT_SUCCESS;
}
