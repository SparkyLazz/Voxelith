#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

struct GLFWwindow;

class Renderer {
public:
    struct ViewMatrix { float m[16]; };  // column-major, glm-compatible layout
    struct Vec3       { float x, y, z; };

    // Layout-identical to vox::MesherVertex — main.cpp memcpy's between them.
    struct RendererVertex {
        glm::vec3 position;
        glm::vec3 color;
    };
    static_assert(sizeof(RendererVertex) == 24);

    explicit Renderer(GLFWwindow* window);
    ~Renderer();

    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&)                 = delete;
    Renderer& operator=(Renderer&&)      = delete;

    void beginImGuiFrame(Vec3 cameraPos);
    void drawFrame(const ViewMatrix& view);
    void notifyResize();

    // Replace the active mesh on the GPU. Safe to call between frames.
    // Empty vectors clear the mesh (next frame renders nothing).
    void set_mesh(const std::vector<RendererVertex>& vertices,
                  const std::vector<uint32_t>&        indices);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
