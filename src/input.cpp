#include "input.h"

Input::Input(GLFWwindow* window) : m_window(window) {
    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
}

Input::~Input() {
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

void Input::beginFrame() {
    m_state.mouseDeltaX = 0.0f;
    m_state.mouseDeltaY = 0.0f;
}

// ---------------------------------------------------------------------------
// Static GLFW dispatch
// ---------------------------------------------------------------------------

void Input::keyCallback(GLFWwindow* window, int key,
                        [[maybe_unused]] int scancode, int action,
                        [[maybe_unused]] int mods) {
    auto* self = static_cast<Input*>(glfwGetWindowUserPointer(window));
    if (self) self->onKey(key, action);
}

void Input::mouseButtonCallback(GLFWwindow* window, int button, int action,
                                [[maybe_unused]] int mods) {
    auto* self = static_cast<Input*>(glfwGetWindowUserPointer(window));
    if (self) self->onMouseButton(button, action);
}

void Input::cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    auto* self = static_cast<Input*>(glfwGetWindowUserPointer(window));
    if (self) self->onCursorPos(xpos, ypos);
}

// ---------------------------------------------------------------------------
// Instance handlers
// ---------------------------------------------------------------------------

void Input::onKey(int key, int action) {
    const bool pressed = (action == GLFW_PRESS || action == GLFW_REPEAT);

    switch (key) {
        case GLFW_KEY_W:           m_state.keyW      = pressed; break;
        case GLFW_KEY_A:           m_state.keyA      = pressed; break;
        case GLFW_KEY_S:           m_state.keyS      = pressed; break;
        case GLFW_KEY_D:           m_state.keyD      = pressed; break;
        case GLFW_KEY_E:           m_state.keyE      = pressed; break;
        case GLFW_KEY_Q:           m_state.keyQ      = pressed; break;
        case GLFW_KEY_LEFT_SHIFT:
        case GLFW_KEY_RIGHT_SHIFT: m_state.keyShift  = pressed; break;
        case GLFW_KEY_ESCAPE:      m_state.keyEscape = (action == GLFW_PRESS); break;
        default: break;
    }
}

void Input::onMouseButton(int button, int action) {
    if (button != GLFW_MOUSE_BUTTON_RIGHT) return;

    const bool wasHeld         = m_state.rightMouseHeld;
    m_state.rightMouseHeld     = (action == GLFW_PRESS);

    if (m_state.rightMouseHeld && !wasHeld) {
        m_firstMouse = true;   // suppress snap rotation on first cursor event
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    } else if (!m_state.rightMouseHeld && wasHeld) {
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

void Input::onCursorPos(double xpos, double ypos) {
    if (!m_state.rightMouseHeld) {
        // Track position so we have a sane baseline if RMB is pressed later
        m_lastMouseX = xpos;
        m_lastMouseY = ypos;
        return;
    }

    if (m_firstMouse) {
        m_lastMouseX = xpos;
        m_lastMouseY = ypos;
        m_firstMouse = false;
        return;
    }

    m_state.mouseDeltaX = static_cast<float>(xpos - m_lastMouseX);
    m_state.mouseDeltaY = static_cast<float>(ypos - m_lastMouseY);
    m_lastMouseX = xpos;
    m_lastMouseY = ypos;
}
