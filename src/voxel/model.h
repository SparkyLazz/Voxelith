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

    // Tightly encloses all non-empty voxels in world-voxel coordinates (inclusive).
    // If no non-empty voxels exist, sets both bounds to ivec3(0).
    void recompute_bounds() {
        bool found = false;
        glm::ivec3 bmin(std::numeric_limits<int>::max());
        glm::ivec3 bmax(std::numeric_limits<int>::min());

        for (const auto& kv : chunks) {
            const Chunk& chunk = kv.second;
            if (chunk.is_empty()) { continue; }

            const glm::ivec3 origin = kv.first * CHUNK_SIZE;
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                for (int y = 0; y < CHUNK_SIZE; ++y) {
                    for (int x = 0; x < CHUNK_SIZE; ++x) {
                        if (chunk.at(x, y, z) != 0) {
                            glm::ivec3 world = origin + glm::ivec3(x, y, z);
                            bmin  = glm::min(bmin, world);
                            bmax  = glm::max(bmax, world);
                            found = true;
                        }
                    }
                }
            }
        }

        if (!found) {
            bounds_min = bounds_max = glm::ivec3{0};
        } else {
            bounds_min = bmin;
            bounds_max = bmax;
        }
    }

    bool operator==(const Model&) const = default;
};

} // namespace vox
