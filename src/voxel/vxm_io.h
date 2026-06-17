#pragma once

#include <filesystem>
#include <optional>

#include "voxel/model.h"

namespace vox {

enum class VxmError {
    Ok,
    OpenFailed,
    WriteFailed,
    ReadFailed,
    BadMagic,
    UnsupportedVersion,
    Malformed,        // bad sizes, truncated, out-of-range counts
};

const char* to_string(VxmError e);

bool             write_vxm(const Model& m, const std::filesystem::path& path,
                           VxmError* out_err = nullptr);
std::optional<Model> read_vxm(const std::filesystem::path& path,
                              VxmError* out_err = nullptr);

} // namespace vox
