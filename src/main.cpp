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

#ifdef VXM_ROUNDTRIP_TEST
#include "voxel/vxm_io.h"
#include <filesystem>
#endif

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

#ifdef VXM_ROUNDTRIP_TEST
    {
        vox::Model m;
        m.name          = "rt_test";
        m.palette.name  = "rt_palette";

        // material[1]: red, emissive, roughness 0.3
        m.palette.materials[1].albedo            = {0.8f, 0.2f, 0.2f};
        m.palette.materials[1].emission          = {1.0f, 0.1f, 0.1f};
        m.palette.materials[1].emission_strength = 1.0f;
        m.palette.materials[1].roughness         = 0.3f;

        // material[7]: blue glass
        m.palette.materials[7].albedo       = {0.1f, 0.2f, 0.9f};
        m.palette.materials[7].transparency = 0.6f;
        m.palette.materials[7].ior          = 1.5f;
        m.palette.materials[7].flags        = static_cast<uint32_t>(vox::MaterialFlag::Glass);

        {
            vox::Chunk c;
            c.position = glm::ivec3{0, 0, 0};
            c.set(5,  5,  5,  1);
            c.set(10, 10, 10, 7);
            m.chunks[c.position] = c;
        }
        {
            vox::Chunk c;
            c.position = glm::ivec3{1, 0, 0};
            c.set(2,  3,  4,  1);
            c.set(31, 31, 31, 7);
            m.chunks[c.position] = c;
        }
        {
            vox::Chunk c;
            c.position = glm::ivec3{-1, 2, 3};
            c.set(0, 0, 0, 7);
            m.chunks[c.position] = c;
        }

        m.recompute_bounds();

        const auto rtPath = std::filesystem::temp_directory_path() / "vxm_roundtrip.vxm";
        vox::VxmError err  = vox::VxmError::Ok;

        if (!vox::write_vxm(m, rtPath, &err)) {
            spdlog::error("[vxm_roundtrip] write failed: {}", vox::to_string(err));
        } else {
            auto m2 = vox::read_vxm(rtPath, &err);
            if (!m2) {
                spdlog::error("[vxm_roundtrip] read failed: {}", vox::to_string(err));
            } else if (m == *m2) {
                std::error_code ec;
                const auto fsize = std::filesystem::file_size(rtPath, ec);
                spdlog::info("[vxm_roundtrip] OK — file={} bytes, chunks={}, voxels={}",
                             fsize, m.chunk_count(), m.non_empty_voxel_count());
            } else {
                if (m.name != m2->name)
                    spdlog::error("[vxm_roundtrip] MISMATCH name: '{}' vs '{}'", m.name, m2->name);
                if (m.palette.name != m2->palette.name)
                    spdlog::error("[vxm_roundtrip] MISMATCH palette.name: '{}' vs '{}'", m.palette.name, m2->palette.name);
                if (m.id != m2->id)
                    spdlog::error("[vxm_roundtrip] MISMATCH id: {} vs {}", m.id.to_string(), m2->id.to_string());
                if (m.chunks.size() != m2->chunks.size())
                    spdlog::error("[vxm_roundtrip] MISMATCH chunk_count: {} vs {}", m.chunks.size(), m2->chunks.size());
                if (m.bounds_min != m2->bounds_min || m.bounds_max != m2->bounds_max)
                    spdlog::error("[vxm_roundtrip] MISMATCH bounds: [{},{},{}]-[{},{},{}] vs [{},{},{}]-[{},{},{}]",
                                  m.bounds_min.x,  m.bounds_min.y,  m.bounds_min.z,
                                  m.bounds_max.x,  m.bounds_max.y,  m.bounds_max.z,
                                  m2->bounds_min.x, m2->bounds_min.y, m2->bounds_min.z,
                                  m2->bounds_max.x, m2->bounds_max.y, m2->bounds_max.z);
                spdlog::error("[vxm_roundtrip] FAILED");
            }
        }
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
