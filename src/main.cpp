#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#pragma warning(push)
#pragma warning(disable : 4100 4127 4201 4244 4245 4267 4305)
#include <imgui.h>
#pragma warning(pop)

#include <spdlog/spdlog.h>

#include "camera.h"
#include "editor/editor_state.h"
#include "editor/menu_bar.h"
#include "editor/palette_panel.h"
#include "input.h"
#include "renderer.h"
#include "voxel/naive_mesher.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>

// ---------------------------------------------------------------------------
// File-statics for GLFW callbacks
// ---------------------------------------------------------------------------

static Renderer*        s_renderer     = nullptr;
static ed::EditorState* s_editor_state = nullptr;

static void glfwErrorCallback(int error, const char* description) {
    spdlog::error("GLFW error {}: {}", error, description);
}

static void framebufferResizeCallback(GLFWwindow*, [[maybe_unused]] int w,
                                      [[maybe_unused]] int h) {
    if (s_renderer) s_renderer->notifyResize();
}

// ---------------------------------------------------------------------------
// Mesh-rebuild helper
// ---------------------------------------------------------------------------

static void rebuild_mesh_from_active(ed::EditorState& state, Renderer& renderer) {
    if (!state.has_user_loaded_model()) {
        renderer.set_mesh({}, {});
        return;
    }
    const auto t0   = std::chrono::steady_clock::now();
    auto       mesh = vox::build_naive_mesh(*state.active_model());
    const auto t1   = std::chrono::steady_clock::now();
    const auto ms   = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    spdlog::info("[naive_mesher] vertices={} indices={} build_ms={}",
                 mesh.vertices.size(), mesh.indices.size(), ms);
    std::vector<Renderer::RendererVertex> rv(mesh.vertices.size());
    std::memcpy(rv.data(), mesh.vertices.data(),
                mesh.vertices.size() * sizeof(Renderer::RendererVertex));
    renderer.set_mesh(rv, mesh.indices);
}

// ---------------------------------------------------------------------------
// Drag-and-drop callback
// ---------------------------------------------------------------------------

static void on_drop(GLFWwindow*, int count, const char** paths) {
    if (!s_editor_state || !s_renderer || count <= 0) return;

    std::filesystem::path p{paths[0]};
    auto ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    bool ok = false;
    if (ext == ".vox") {
        ok = s_editor_state->load_vox(p);
    } else if (ext == ".vxm") {
        ok = s_editor_state->load_vxm(p);
    } else {
        spdlog::warn("[drop] ignoring unsupported extension: \"{}\"", p.string());
        return;
    }
    if (ok) {
        rebuild_mesh_from_active(*s_editor_state, *s_renderer);
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    spdlog::set_level(spdlog::level::debug);

    ed::EditorState editor_state;
    editor_state.new_empty_model();
    s_editor_state = &editor_state;

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
    glfwSetDropCallback(window, on_drop);

    // Input installs its GLFW callbacks first (and takes the window user
    // pointer). Renderer's ImGui backend then chains on top.
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

    // No user model loaded yet — clears any stale GPU mesh.
    rebuild_mesh_from_active(editor_state, *renderer);

    spdlog::info("Entering main loop");

    auto lastTime = std::chrono::steady_clock::now();

    while (glfwWindowShouldClose(window) == GLFW_FALSE) {
        input->beginFrame();
        glfwPollEvents();

        if (input->state().keyEscape) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        const auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;
        dt = std::min(dt, 0.1f);

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

        // Menu bar + modals are drawn here, between NewFrame and Render.
        const ed::MenuResult menu = ed::draw_menu_bar(editor_state);
        if (menu.model_changed) {
            rebuild_mesh_from_active(editor_state, *renderer);
        }
        if (menu.quit_requested) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        const ed::PalettePanelResult pal = ed::draw_palette_panel(editor_state);
        if (pal.albedo_changed) {
            rebuild_mesh_from_active(editor_state, *renderer);
        }

        glm::mat4 view = camera.viewMatrix();
        Renderer::ViewMatrix vm{};
        std::memcpy(vm.m, glm::value_ptr(view), sizeof(vm.m));
        renderer->drawFrame(vm);
    }

    s_renderer     = nullptr;
    s_editor_state = nullptr;
    renderer.reset();
    input.reset();

    glfwDestroyWindow(window);
    glfwTerminate();

    spdlog::info("Shutdown clean");
    return EXIT_SUCCESS;
}
