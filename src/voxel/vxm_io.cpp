#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>

#include <spdlog/spdlog.h>

#include "voxel/vxm_io.h"

static_assert(sizeof(float)    == 4);
static_assert(sizeof(uint32_t) == 4 && sizeof(int32_t) == 4);

namespace {

bool write_bytes(std::ostream& s, const void* data, size_t n) {
    s.write(static_cast<const char*>(data), static_cast<std::streamsize>(n));
    return s.good();
}

bool write_u32(std::ostream& s, uint32_t v) { return write_bytes(s, &v, sizeof(v)); }
bool write_i32(std::ostream& s, int32_t  v) { return write_bytes(s, &v, sizeof(v)); }
bool write_f32(std::ostream& s, float    v) { return write_bytes(s, &v, sizeof(v)); }

bool read_bytes(std::istream& s, void* data, size_t n) {
    s.read(static_cast<char*>(data), static_cast<std::streamsize>(n));
    return s.good();
}

bool read_u32(std::istream& s, uint32_t& v) { return read_bytes(s, &v, sizeof(v)); }
bool read_i32(std::istream& s, int32_t&  v) { return read_bytes(s, &v, sizeof(v)); }
bool read_f32(std::istream& s, float&    v) { return read_bytes(s, &v, sizeof(v)); }

} // anonymous namespace

namespace vox {

const char* to_string(VxmError e) {
    switch (e) {
        case VxmError::Ok:                 return "Ok";
        case VxmError::OpenFailed:         return "OpenFailed";
        case VxmError::WriteFailed:        return "WriteFailed";
        case VxmError::ReadFailed:         return "ReadFailed";
        case VxmError::BadMagic:           return "BadMagic";
        case VxmError::UnsupportedVersion: return "UnsupportedVersion";
        case VxmError::Malformed:          return "Malformed";
        default:                           return "Unknown";
    }
}

bool write_vxm(const Model& m, const std::filesystem::path& path, VxmError* out_err) {
    auto fail = [&path, out_err](VxmError e, const char* msg) -> bool {
        if (out_err) { *out_err = e; }
        spdlog::error("write_vxm '{}': {} ({})", path.string(), msg, to_string(e));
        return false;
    };

    if (m.name.size()         > 1024)       { return fail(VxmError::Malformed, "name too long");         }
    if (m.palette.name.size() > 1024)       { return fail(VxmError::Malformed, "palette name too long"); }
    if (m.chunks.size()       > 1'000'000u) { return fail(VxmError::Malformed, "too many chunks");       }

    std::ofstream f(path, std::ios::binary);
    if (!f) { return fail(VxmError::OpenFailed, "cannot open for writing"); }

    // ---- Header (16 bytes) ----
    static constexpr char k_magic[4] = {'V', 'X', 'M', 'F'};
    if (!write_bytes(f, k_magic, 4))  { return fail(VxmError::WriteFailed, "magic");    }
    if (!write_u32(f, 1u))            { return fail(VxmError::WriteFailed, "version");  }
    if (!write_u32(f, 0u))            { return fail(VxmError::WriteFailed, "flags");    }
    if (!write_u32(f, 0u))            { return fail(VxmError::WriteFailed, "reserved"); }

    // ---- UUID ----
    if (!write_bytes(f, m.id.bytes.data(), 16)) { return fail(VxmError::WriteFailed, "uuid"); }

    // ---- Bounds ----
    if (!write_i32(f, m.bounds_min.x)) { return fail(VxmError::WriteFailed, "bounds_min.x"); }
    if (!write_i32(f, m.bounds_min.y)) { return fail(VxmError::WriteFailed, "bounds_min.y"); }
    if (!write_i32(f, m.bounds_min.z)) { return fail(VxmError::WriteFailed, "bounds_min.z"); }
    if (!write_i32(f, m.bounds_max.x)) { return fail(VxmError::WriteFailed, "bounds_max.x"); }
    if (!write_i32(f, m.bounds_max.y)) { return fail(VxmError::WriteFailed, "bounds_max.y"); }
    if (!write_i32(f, m.bounds_max.z)) { return fail(VxmError::WriteFailed, "bounds_max.z"); }

    // ---- Model name ----
    if (!write_u32(f, static_cast<uint32_t>(m.name.size()))) { return fail(VxmError::WriteFailed, "name_len"); }
    if (!m.name.empty() && !write_bytes(f, m.name.data(), m.name.size())) { return fail(VxmError::WriteFailed, "name"); }

    // ---- Palette ----
    if (!write_u32(f, static_cast<uint32_t>(m.palette.name.size()))) { return fail(VxmError::WriteFailed, "palette_name_len"); }
    if (!m.palette.name.empty() && !write_bytes(f, m.palette.name.data(), m.palette.name.size())) {
        return fail(VxmError::WriteFailed, "palette_name");
    }

    for (const auto& mat : m.palette.materials) {
        bool ok = write_f32(f, mat.albedo.x)
               && write_f32(f, mat.albedo.y)
               && write_f32(f, mat.albedo.z)
               && write_f32(f, mat.roughness)
               && write_f32(f, mat.metallic)
               && write_f32(f, mat.emission.x)
               && write_f32(f, mat.emission.y)
               && write_f32(f, mat.emission.z)
               && write_f32(f, mat.emission_strength)
               && write_f32(f, mat.transparency)
               && write_f32(f, mat.ior)
               && write_u32(f, mat.flags);
        if (!ok) { return fail(VxmError::WriteFailed, "material"); }
    }

    // ---- Chunks ----
    if (!write_u32(f, static_cast<uint32_t>(m.chunks.size()))) { return fail(VxmError::WriteFailed, "chunk_count"); }

    for (const auto& kv : m.chunks) {
        const Chunk& chunk = kv.second;
        bool ok = write_i32(f, chunk.position.x)
               && write_i32(f, chunk.position.y)
               && write_i32(f, chunk.position.z)
               && write_bytes(f, chunk.voxels.data(), chunk.voxels.size());
        if (!ok) { return fail(VxmError::WriteFailed, "chunk"); }
    }

    if (!f) { return fail(VxmError::WriteFailed, "stream error after write"); }
    if (out_err) { *out_err = VxmError::Ok; }
    return true;
}

std::optional<Model> read_vxm(const std::filesystem::path& path, VxmError* out_err) {
    auto fail = [&path, out_err](VxmError e, const char* msg) -> std::optional<Model> {
        if (out_err) { *out_err = e; }
        spdlog::error("read_vxm '{}': {} ({})", path.string(), msg, to_string(e));
        return std::nullopt;
    };

    std::ifstream f(path, std::ios::binary);
    if (!f) { return fail(VxmError::OpenFailed, "cannot open for reading"); }

    // ---- Header ----
    char magic[4];
    if (!read_bytes(f, magic, 4)) { return fail(VxmError::ReadFailed, "magic"); }
    if (magic[0] != 'V' || magic[1] != 'X' || magic[2] != 'M' || magic[3] != 'F') {
        return fail(VxmError::BadMagic, "invalid magic bytes");
    }

    uint32_t version{};
    if (!read_u32(f, version)) { return fail(VxmError::ReadFailed, "version"); }
    if (version != 1u) { return fail(VxmError::UnsupportedVersion, "unsupported version"); }

    uint32_t hdr_flags{};
    if (!read_u32(f, hdr_flags)) { return fail(VxmError::ReadFailed, "flags"); }
    static_cast<void>(hdr_flags); // reserved for future LZ4 flag

    uint32_t hdr_reserved{};
    if (!read_u32(f, hdr_reserved)) { return fail(VxmError::ReadFailed, "reserved"); }
    static_cast<void>(hdr_reserved);

    Model m;

    // ---- UUID ----
    if (!read_bytes(f, m.id.bytes.data(), 16)) { return fail(VxmError::ReadFailed, "uuid"); }

    // ---- Bounds ----
    if (!read_i32(f, m.bounds_min.x)) { return fail(VxmError::ReadFailed, "bounds_min.x"); }
    if (!read_i32(f, m.bounds_min.y)) { return fail(VxmError::ReadFailed, "bounds_min.y"); }
    if (!read_i32(f, m.bounds_min.z)) { return fail(VxmError::ReadFailed, "bounds_min.z"); }
    if (!read_i32(f, m.bounds_max.x)) { return fail(VxmError::ReadFailed, "bounds_max.x"); }
    if (!read_i32(f, m.bounds_max.y)) { return fail(VxmError::ReadFailed, "bounds_max.y"); }
    if (!read_i32(f, m.bounds_max.z)) { return fail(VxmError::ReadFailed, "bounds_max.z"); }

    // ---- Model name ----
    uint32_t name_len{};
    if (!read_u32(f, name_len)) { return fail(VxmError::ReadFailed, "name_len"); }
    if (name_len > 1024u) { return fail(VxmError::Malformed, "name too long"); }
    m.name.resize(name_len);
    if (name_len > 0u && !read_bytes(f, m.name.data(), name_len)) { return fail(VxmError::ReadFailed, "name"); }

    // ---- Palette ----
    uint32_t pal_name_len{};
    if (!read_u32(f, pal_name_len)) { return fail(VxmError::ReadFailed, "palette_name_len"); }
    if (pal_name_len > 1024u) { return fail(VxmError::Malformed, "palette name too long"); }
    m.palette.name.resize(pal_name_len);
    if (pal_name_len > 0u && !read_bytes(f, m.palette.name.data(), pal_name_len)) {
        return fail(VxmError::ReadFailed, "palette_name");
    }

    for (auto& mat : m.palette.materials) {
        bool ok = read_f32(f, mat.albedo.x)
               && read_f32(f, mat.albedo.y)
               && read_f32(f, mat.albedo.z)
               && read_f32(f, mat.roughness)
               && read_f32(f, mat.metallic)
               && read_f32(f, mat.emission.x)
               && read_f32(f, mat.emission.y)
               && read_f32(f, mat.emission.z)
               && read_f32(f, mat.emission_strength)
               && read_f32(f, mat.transparency)
               && read_f32(f, mat.ior)
               && read_u32(f, mat.flags);
        if (!ok) { return fail(VxmError::ReadFailed, "material"); }
    }

    // ---- Chunks ----
    uint32_t chunk_count{};
    if (!read_u32(f, chunk_count)) { return fail(VxmError::ReadFailed, "chunk_count"); }
    if (chunk_count > 1'000'000u) { return fail(VxmError::Malformed, "too many chunks"); }

    m.chunks.reserve(chunk_count);
    for (uint32_t i = 0; i < chunk_count; ++i) {
        Chunk chunk;
        bool ok = read_i32(f, chunk.position.x)
               && read_i32(f, chunk.position.y)
               && read_i32(f, chunk.position.z)
               && read_bytes(f, chunk.voxels.data(), chunk.voxels.size());
        if (!ok) { return fail(VxmError::ReadFailed, "chunk"); }
        m.chunks.emplace(chunk.position, std::move(chunk));
    }

    if (out_err) { *out_err = VxmError::Ok; }
    return m;
}

} // namespace vox
