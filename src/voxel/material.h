#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace vox {

enum class MaterialFlag : uint32_t {
    Glass   = 1u << 0,
    Foliage = 1u << 1,
};

struct Material {
    glm::vec3 albedo            = {1.0f, 1.0f, 1.0f}; // offset  0, 12 bytes
    float     roughness         = 0.5f;                 // offset 12,  4 bytes
    float     metallic          = 0.0f;                 // offset 16,  4 bytes
    glm::vec3 emission          = {0.0f, 0.0f, 0.0f}; // offset 20, 12 bytes
    float     emission_strength = 0.0f;                 // offset 32,  4 bytes
    float     transparency      = 0.0f;                 // offset 36,  4 bytes
    float     ior               = 1.0f;                 // offset 40,  4 bytes
    uint32_t  flags             = 0;                    // offset 44,  4 bytes
                                                        // total:     48 bytes

    bool operator==(const Material&) const = default;
};

static_assert(sizeof(Material) == 48, "Material must be 48 bytes for on-disk layout");

} // namespace vox
