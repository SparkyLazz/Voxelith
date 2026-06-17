#pragma once

#include <filesystem>
#include <optional>

#include "voxel/model.h"

namespace vox {

enum class VoxError {
    Ok,
    OpenFailed,
    ReadFailed,
    BadMagic,
    Malformed,
};

const char* to_string(VoxError e);

std::optional<Model> import_vox(const std::filesystem::path& path,
                                VoxError* out_err = nullptr);

} // namespace vox
