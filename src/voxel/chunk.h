#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

namespace vox {

constexpr int    CHUNK_SIZE        = 32;
constexpr size_t CHUNK_VOXEL_COUNT = static_cast<size_t>(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE);

struct Chunk {
    std::array<uint8_t, CHUNK_VOXEL_COUNT> voxels{};  // palette index; 0 = empty
    glm::ivec3                             position{0}; // chunk-space coordinates

    static size_t index(int x, int y, int z) {
        assert(x >= 0 && x < CHUNK_SIZE);
        assert(y >= 0 && y < CHUNK_SIZE);
        assert(z >= 0 && z < CHUNK_SIZE);
        constexpr auto S = static_cast<size_t>(CHUNK_SIZE);
        return static_cast<size_t>(x) + static_cast<size_t>(y) * S + static_cast<size_t>(z) * S * S;
    }

    uint8_t at(int x, int y, int z) const  { return voxels[index(x, y, z)]; }
    void    set(int x, int y, int z, uint8_t v) { voxels[index(x, y, z)] = v; }

    bool is_empty() const {
        return std::all_of(voxels.begin(), voxels.end(), [](uint8_t v) { return v == 0; });
    }

    bool operator==(const Chunk&) const = default;
};

} // namespace vox
