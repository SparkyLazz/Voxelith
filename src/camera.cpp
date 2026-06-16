#include "camera.h"
#include "input.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

Camera::Camera() {
    // Initialise m_forward from starting yaw/pitch so viewMatrix() is valid
    // before the first update() call.
    const float yr = glm::radians(m_yaw);
    const float pr = glm::radians(m_pitch);
    m_forward = glm::normalize(glm::vec3{
        std::cos(yr) * std::cos(pr),
        std::sin(pr),
        std::sin(yr) * std::cos(pr)
    });
}

void Camera::update(const InputState& input, float dt) {
    // Rotation from mouse
    m_yaw   += input.mouseDeltaX * m_mouseSens;
    m_pitch -= input.mouseDeltaY * m_mouseSens;    // invert Y: up → look up
    m_pitch  = std::clamp(m_pitch, -89.0f, 89.0f);
    m_yaw    = std::fmod(m_yaw, 360.0f);

    const float yr = glm::radians(m_yaw);
    const float pr = glm::radians(m_pitch);
    m_forward = glm::normalize(glm::vec3{
        std::cos(yr) * std::cos(pr),
        std::sin(pr),
        std::sin(yr) * std::cos(pr)
    });

    // Movement
    const glm::vec3 worldUp{0.0f, 1.0f, 0.0f};
    const glm::vec3 right = glm::normalize(glm::cross(m_forward, worldUp));

    glm::vec3 move{0.0f};
    if (input.keyW) move += m_forward;
    if (input.keyS) move -= m_forward;
    if (input.keyD) move += right;
    if (input.keyA) move -= right;
    if (input.keyE) move += worldUp;   // elevator — always world-space up
    if (input.keyQ) move -= worldUp;

    if (glm::length(move) > 0.0f) {
        move = glm::normalize(move);
        const float speed = m_moveSpeed * (input.keyShift ? m_boostMult : 1.0f);
        m_position += move * speed * dt;
    }
}

glm::mat4 Camera::viewMatrix() const {
    const glm::vec3 worldUp{0.0f, 1.0f, 0.0f};
    return glm::lookAt(m_position, m_position + m_forward, worldUp);
}
