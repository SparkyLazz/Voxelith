#pragma once

#include <cstddef>
#include <limits>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

#include "voxel/chunk.h"
#include "voxel/palette.h"
#include "voxel/voxel_types.h"

namespace vox {

struct Model {
    std::unordered_map<glm::ivec3, Chunk> chunks;
    Palette                               palette;
    glm::ivec3                            bounds_min{0};
    glm::ivec3                            bounds_max{0};
    std::string                           name;
    UUID                                  id = UUID::generate();

    size_t non_empty_voxel_count() const {
        size_t count = 0;
        for (const auto& kv : chunks) {
            for (auto v : kv.second.voxels) {
                if (v != 0) { ++count; }
            }
        }
        return count;
    }

    size_t chunk_count() const { return chunks.size(); }

    void recompute_bounds() {
        if (chunks.empty()) {
            bounds_min = bounds_max = glm::ivec3{0};
            return;
        }
        bounds_min = glm::ivec3(std::numeric_limits<int>::max());
        bounds_max = glm::ivec3(std::numeric_limits<int>::min());
        for (const auto& kv : chunks) {
            glm::ivec3 vmin = kv.first * CHUNK_SIZE;
            glm::ivec3 vmax = vmin + glm::ivec3(CHUNK_SIZE - 1);
            bounds_min = glm::min(bounds_min, vmin);
            bounds_max = glm::max(bounds_max, vmax);
        }
    }

    bool operator==(const Model&) const = default;
};

} // namespace vox
