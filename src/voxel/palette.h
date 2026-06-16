#pragma once

#include <array>
#include <cstddef>
#include <string>

#include "voxel/material.h"

namespace vox {

constexpr size_t PALETTE_SIZE = 256;

struct Palette {
    std::array<Material, PALETTE_SIZE> materials{}; // index 0 reserved for "empty"
    std::string name;

    bool operator==(const Palette&) const = default;
};

} // namespace vox
