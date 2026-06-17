#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "voxel/model.h"

namespace vox {

struct MesherVertex {
    glm::vec3 position;
    glm::vec3 color;
};
static_assert(sizeof(MesherVertex) == 24);

struct MesherOutput {
    std::vector<MesherVertex> vertices;
    std::vector<uint32_t>     indices;
};

MesherOutput build_naive_mesh(const Model& model);

} // namespace vox
