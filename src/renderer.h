#pragma once

#include <memory>

struct GLFWwindow;

class Renderer {
public:
    struct ViewMatrix { float m[16]; };  // column-major, glm-compatible layout
    struct Vec3       { float x, y, z; };

    explicit Renderer(GLFWwindow* window);
    ~Renderer();

    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&)                 = delete;
    Renderer& operator=(Renderer&&)      = delete;

    void beginImGuiFrame(Vec3 cameraPos);
    void drawFrame(const ViewMatrix& view);
    void notifyResize();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
