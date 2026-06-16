#pragma once

#include <glm/glm.hpp>

struct InputState;

class Camera {
public:
    Camera();

    void      update(const InputState& input, float dt);
    glm::mat4 viewMatrix() const;
    glm::vec3 position()   const { return m_position; }

private:
    glm::vec3 m_position{ 3.0f, 2.0f, 3.0f };
    float     m_yaw   = -135.0f;  // degrees — points roughly at origin from start
    float     m_pitch =  -25.0f;  // degrees

    float m_moveSpeed = 4.0f;
    float m_boostMult = 4.0f;
    float m_mouseSens = 0.1f;     // degrees per pixel

    glm::vec3 m_forward{};        // cached, recomputed each update()
};
