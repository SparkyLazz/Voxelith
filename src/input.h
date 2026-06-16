#pragma once

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

struct InputState {
    bool keyW      = false;
    bool keyA      = false;
    bool keyS      = false;
    bool keyD      = false;
    bool keyE      = false;
    bool keyQ      = false;
    bool keyShift  = false;
    bool keyEscape = false;

    float mouseDeltaX   = 0.0f;
    float mouseDeltaY   = 0.0f;
    bool  rightMouseHeld = false;
};

class Input {
public:
    explicit Input(GLFWwindow* window);
    ~Input();

    Input(const Input&)            = delete;
    Input& operator=(const Input&) = delete;

    void beginFrame();
    const InputState& state() const { return m_state; }

private:
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);

    void onKey(int key, int action);
    void onMouseButton(int button, int action);
    void onCursorPos(double xpos, double ypos);

    GLFWwindow* m_window;
    InputState  m_state;
    double      m_lastMouseX = 0.0;
    double      m_lastMouseY = 0.0;
    bool        m_firstMouse = true;
};
