/*
 * MagicaVoxel .vox format reference
 * ==================================
 * Source: https://github.com/ephtracy/voxel-model/blob/master/MagicaVoxel-file-format-vox.txt
 *
 * File layout:
 *   char[4]  magic   = 'V','O','X',' '   (note trailing space)
 *   uint32   version
 *   Chunk    MAIN
 *
 * Chunk layout:
 *   char[4]  id
 *   uint32   content_size     bytes of THIS chunk's own content
 *   uint32   children_size    bytes of all child chunks combined
 *   uint8[content_size]   content
 *   uint8[children_size]  children (each is a Chunk, recursively)
 *
 * MAIN: content_size = 0; all data lives in its children.
 *
 * SIZE content (12 bytes):
 *   uint32 x, uint32 y, uint32 z   — model dimensions (MagicaVoxel is Z-up)
 *
 * XYZI content:
 *   uint32 num_voxels
 *   repeat num_voxels: uint8 x, uint8 y, uint8 z, uint8 color_index
 *   color_index is 1-based: values [1,255]; 0 is never written by MagicaVoxel.
 *
 * RGBA content (1024 bytes):
 *   uint8 r, uint8 g, uint8 b, uint8 a  × 256
 *   RGBA entry i (0-based) → color for XYZI color_index (i+1).
 *   Entry 255 (for color_index 256) is reserved/unused.
 *
 * PACK content (older multi-model files):
 *   uint32 num_models
 *
 * Coordinate system:
 *   MagicaVoxel is Z-up. We are Y-up (from Phase 0 camera). On import:
 *     vox_x → engine x
 *     vox_z → engine y   (Z-up becomes Y-up)
 *     vox_y → engine z
 *   Bounds and chunk positions use the same swapped coords.
 *
 * Palette mapping:
 *   XYZI color_index i → RGBA entry (i-1) → our Palette::materials[i]
 *   Our materials[0] is reserved for "empty" (default-constructed Material).
 *   kDefaultVoxPalette[i] (1-based) gives the packed 0xAABBGGRR color for
 *   color_index i when no RGBA chunk is present in the file.
 *   In 0xAABBGGRR: byte 3 (MSB) = alpha, byte 2 = blue, byte 1 = green,
 *   byte 0 (LSB) = red — matching what you get reading r,g,b,a from disk
 *   as a little-endian uint32 (r | g<<8 | b<<16 | a<<24).
 */

#include "voxel/vox_import.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

static_assert(sizeof(float)    == 4);
static_assert(sizeof(uint32_t) == 4 && sizeof(int32_t) == 4);

// ============================================================
// Anonymous namespace: helpers + default palette
// ============================================================
namespace {

// kDefaultVoxPalette[i] is the packed color (0xAABBGGRR) for XYZI color_index i.
// Index 0 is unused (color_index 0 is never written by MagicaVoxel).
// Source: canonical array from ephtracy/voxel-model, reproduced verbatim.
static constexpr uint32_t kDefaultVoxPalette[256] = {
    0x00000000,
    // --- 6×6×6 colour cube (indices 1-215, opaque black skipped) ---
    // Outer loop R ∈ {ff,cc,99,66,33,00}; middle G; inner B (all decreasing).
    0xffffffff, 0xffccffff, 0xff99ffff, 0xff66ffff, 0xff33ffff, 0xff00ffff,
    0xffffccff, 0xffccccff, 0xff99ccff, 0xff66ccff, 0xff33ccff, 0xff00ccff,
    0xffff99ff, 0xffcc99ff, 0xff9999ff, 0xff6699ff, 0xff3399ff, 0xff0099ff,
    0xffff66ff, 0xffcc66ff, 0xff9966ff, 0xff6666ff, 0xff3366ff, 0xff0066ff,
    0xffff33ff, 0xffcc33ff, 0xff9933ff, 0xff6633ff, 0xff3333ff, 0xff0033ff,
    0xffff00ff, 0xffcc00ff, 0xff9900ff, 0xff6600ff, 0xff3300ff, 0xff0000ff,
    0xffffffcc, 0xffccffcc, 0xff99ffcc, 0xff66ffcc, 0xff33ffcc, 0xff00ffcc,
    0xffffcccc, 0xffcccccc, 0xff99cccc, 0xff66cccc, 0xff33cccc, 0xff00cccc,
    0xffff99cc, 0xffcc99cc, 0xff9999cc, 0xff6699cc, 0xff3399cc, 0xff0099cc,
    0xffff66cc, 0xffcc66cc, 0xff9966cc, 0xff6666cc, 0xff3366cc, 0xff0066cc,
    0xffff33cc, 0xffcc33cc, 0xff9933cc, 0xff6633cc, 0xff3333cc, 0xff0033cc,
    0xffff00cc, 0xffcc00cc, 0xff9900cc, 0xff6600cc, 0xff3300cc, 0xff0000cc,
    0xffffff99, 0xffccff99, 0xff99ff99, 0xff66ff99, 0xff33ff99, 0xff00ff99,
    0xffffcc99, 0xffcccc99, 0xff99cc99, 0xff66cc99, 0xff33cc99, 0xff00cc99,
    0xffff9999, 0xffcc9999, 0xff999999, 0xff669999, 0xff339999, 0xff009999,
    0xffff6699, 0xffcc6699, 0xff996699, 0xff666699, 0xff336699, 0xff006699,
    0xffff3399, 0xffcc3399, 0xff993399, 0xff663399, 0xff333399, 0xff003399,
    0xffff0099, 0xffcc0099, 0xff990099, 0xff660099, 0xff330099, 0xff000099,
    0xffffff66, 0xffccff66, 0xff99ff66, 0xff66ff66, 0xff33ff66, 0xff00ff66,
    0xffffcc66, 0xffcccc66, 0xff99cc66, 0xff66cc66, 0xff33cc66, 0xff00cc66,
    0xffff9966, 0xffcc9966, 0xff999966, 0xff669966, 0xff339966, 0xff009966,
    0xffff6666, 0xffcc6666, 0xff996666, 0xff666666, 0xff336666, 0xff006666,
    0xffff3366, 0xffcc3366, 0xff993366, 0xff663366, 0xff333366, 0xff003366,
    0xffff0066, 0xffcc0066, 0xff990066, 0xff660066, 0xff330066, 0xff000066,
    0xffffff33, 0xffccff33, 0xff99ff33, 0xff66ff33, 0xff33ff33, 0xff00ff33,
    0xffffcc33, 0xffcccc33, 0xff99cc33, 0xff66cc33, 0xff33cc33, 0xff00cc33,
    0xffff9933, 0xffcc9933, 0xff999933, 0xff669933, 0xff339933, 0xff009933,
    0xffff6633, 0xffcc6633, 0xff996633, 0xff666633, 0xff336633, 0xff006633,
    0xffff3333, 0xffcc3333, 0xff993333, 0xff663333, 0xff333333, 0xff003333,
    0xffff0033, 0xffcc0033, 0xff990033, 0xff660033, 0xff330033, 0xff000033,
    0xffffff00, 0xffccff00, 0xff99ff00, 0xff66ff00, 0xff33ff00, 0xff00ff00,
    0xffffcc00, 0xffcccc00, 0xff99cc00, 0xff66cc00, 0xff33cc00, 0xff00cc00,
    0xffff9900, 0xffcc9900, 0xff999900, 0xff669900, 0xff339900, 0xff009900,
    0xffff6600, 0xffcc6600, 0xff996600, 0xff666600, 0xff336600, 0xff006600,
    0xffff3300, 0xffcc3300, 0xff993300, 0xff663300, 0xff333300, 0xff003300,
    0xffff0000, 0xffcc0000, 0xff990000, 0xff660000, 0xff330000,
    // --- Red ramp (R varies ee→11, G=0 B=0, indices 216-225) ---
    0xff0000ee, 0xff0000dd, 0xff0000bb, 0xff0000aa,
    0xff000088, 0xff000077, 0xff000055, 0xff000044, 0xff000022, 0xff000011,
    // --- Green ramp (G varies ee→11, R=0 B=0, indices 226-235) ---
    0xff00ee00, 0xff00dd00, 0xff00bb00, 0xff00aa00,
    0xff008800, 0xff007700, 0xff005500, 0xff004400, 0xff002200, 0xff001100,
    // --- Blue ramp (B varies ee→11, R=0 G=0, indices 236-245) ---
    0xffee0000, 0xffdd0000, 0xffbb0000, 0xffaa0000,
    0xff880000, 0xff770000, 0xff550000, 0xff440000, 0xff220000, 0xff110000,
    // --- Greyscale (R=G=B varies ee→11, indices 246-255) ---
    0xffeeeeee, 0xffdddddd, 0xffbbbbbb, 0xffaaaaaa,
    0xff888888, 0xff777777, 0xff555555, 0xff444444, 0xff222222, 0xff111111,
};

// ---- Low-level read helpers ----

static bool read_bytes(std::istream& s, void* data, size_t n) {
    s.read(static_cast<char*>(data), static_cast<std::streamsize>(n));
    return s.good();
}

static bool read_u8(std::istream& s, uint8_t& v) {
    return read_bytes(s, &v, 1);
}

static bool read_u32(std::istream& s, uint32_t& v) {
    return read_bytes(s, &v, sizeof(v));
}

// ---- Colour conversion ----

// packed = 0xAABBGGRR (r at LSB, a at MSB — matches little-endian read of r,g,b,a from disk)
static vox::Material packed_to_material(uint32_t packed) {
    const uint8_t r = static_cast<uint8_t>( packed        & 0xFFu);
    const uint8_t g = static_cast<uint8_t>((packed >>  8) & 0xFFu);
    const uint8_t b = static_cast<uint8_t>((packed >> 16) & 0xFFu);
    const uint8_t a = static_cast<uint8_t>((packed >> 24) & 0xFFu);

    vox::Material m;
    m.albedo            = { r / 255.0f, g / 255.0f, b / 255.0f };
    m.roughness         = 1.0f;
    m.metallic          = 0.0f;
    m.emission          = { 0.0f, 0.0f, 0.0f };
    m.emission_strength = 0.0f;
    m.transparency      = 1.0f - a / 255.0f;
    m.ior               = 1.0f;
    m.flags             = 0;
    return m;
}

// Temporary storage for voxels before we build chunks
struct RawVoxel {
    uint8_t x, y, z, ci; // vox-space coords + 1-based color index
};

} // anonymous namespace

// ============================================================
// Public API
// ============================================================
namespace vox {

const char* to_string(VoxError e) {
    switch (e) {
        case VoxError::Ok:         return "Ok";
        case VoxError::OpenFailed: return "OpenFailed";
        case VoxError::ReadFailed: return "ReadFailed";
        case VoxError::BadMagic:   return "BadMagic";
        case VoxError::Malformed:  return "Malformed";
    }
    return "Unknown";
}

std::optional<Model> import_vox(const std::filesystem::path& path, VoxError* out_err) {
    const auto t0 = std::chrono::steady_clock::now();

    auto fail = [&](VoxError e, const char* reason) -> std::optional<Model> {
        spdlog::error("[vox_import] {}: {}", path.string(), reason);
        if (out_err) { *out_err = e; }
        return std::nullopt;
    };

    std::ifstream file(path, std::ios::binary);
    if (!file) { return fail(VoxError::OpenFailed, "cannot open file"); }

    // ---- File header ----
    char magic[4] = {};
    if (!read_bytes(file, magic, 4)) { return fail(VoxError::ReadFailed, "failed to read magic"); }
    if (magic[0] != 'V' || magic[1] != 'O' || magic[2] != 'X' || magic[3] != ' ') {
        return fail(VoxError::BadMagic, "bad magic (expected 'VOX ')");
    }

    uint32_t version = 0;
    if (!read_u32(file, version)) { return fail(VoxError::ReadFailed, "failed to read version"); }
    spdlog::debug("[vox_import] version={}", version);

    // ---- MAIN chunk header ----
    char main_id[4] = {};
    uint32_t main_csz = 0, main_xsz = 0;
    if (!read_bytes(file, main_id, 4) || !read_u32(file, main_csz) || !read_u32(file, main_xsz)) {
        return fail(VoxError::ReadFailed, "failed to read MAIN chunk header");
    }
    if (main_id[0] != 'M' || main_id[1] != 'A' || main_id[2] != 'I' || main_id[3] != 'N') {
        return fail(VoxError::Malformed, "expected MAIN chunk");
    }
    // Skip any unexpected MAIN content (spec says 0 bytes, but be defensive)
    if (main_csz > 0) {
        file.seekg(static_cast<std::streamoff>(main_csz), std::ios::cur);
    }

    // ---- Walk MAIN's children (flat sweep) ----
    // Compute absolute file position of end of MAIN's children block.
    const auto children_end =
        static_cast<std::streamoff>(file.tellg()) + static_cast<std::streamoff>(main_xsz);

    // Parse state
    uint32_t size_x = 0, size_y = 0, size_z = 0;
    bool has_size  = false;
    bool has_xyzi  = false;
    bool warned_multimodel = false;

    std::vector<RawVoxel> raw_voxels;

    // raw_rgba[i] = packed color for RGBA chunk position i.
    // color_index ci → raw_rgba[ci-1] (so our palette[ci] uses raw_rgba[ci-1]).
    std::array<uint32_t, 256> raw_rgba{};
    bool has_rgba = false;

    std::map<std::string, uint32_t> skip_counts;

    while (file) {
        const auto pos_before_header = static_cast<std::streamoff>(file.tellg());
        if (pos_before_header + 12LL > children_end) { break; }

        char id[4] = {};
        uint32_t csz = 0, xsz = 0;
        if (!read_bytes(file, id, 4) || !read_u32(file, csz) || !read_u32(file, xsz)) {
            return fail(VoxError::ReadFailed, "truncated chunk header");
        }

        // Position of the first byte of this chunk's content (immediately after the 12-byte header)
        const auto content_start = static_cast<std::streamoff>(file.tellg());
        // Absolute end of this chunk (content + children)
        const auto chunk_end = content_start
                             + static_cast<std::streamoff>(csz)
                             + static_cast<std::streamoff>(xsz);
        if (chunk_end > children_end) {
            return fail(VoxError::Malformed, "chunk overruns MAIN children block");
        }

        // ---- Dispatch ----
        if (id[0]=='P' && id[1]=='A' && id[2]=='C' && id[3]=='K') {
            // PACK: num_models uint32
            if (csz >= 4) {
                uint32_t num_models = 0;
                if (!read_u32(file, num_models)) { return fail(VoxError::ReadFailed, "PACK read"); }
                if (num_models > 1 && !warned_multimodel) {
                    spdlog::warn("[vox_import] {}: {} models in PACK — importing only the first",
                                 path.filename().string(), num_models);
                    warned_multimodel = true;
                }
            }
        }
        else if (id[0]=='S' && id[1]=='I' && id[2]=='Z' && id[3]=='E') {
            if (!has_size) {
                if (csz < 12) { return fail(VoxError::Malformed, "SIZE chunk too small"); }
                if (!read_u32(file, size_x) || !read_u32(file, size_y) || !read_u32(file, size_z)) {
                    return fail(VoxError::ReadFailed, "SIZE read");
                }
                if (size_x > 2048u || size_y > 2048u || size_z > 2048u) {
                    return fail(VoxError::Malformed, "SIZE dimensions exceed 2048");
                }
                has_size = true;
            } else if (!warned_multimodel) {
                spdlog::warn("[vox_import] {}: multiple SIZE chunks — subsequent models skipped",
                             path.filename().string());
                warned_multimodel = true;
            }
        }
        else if (id[0]=='X' && id[1]=='Y' && id[2]=='Z' && id[3]=='I') {
            if (!has_size) {
                return fail(VoxError::Malformed, "XYZI before SIZE");
            }
            if (!has_xyzi) {
                if (csz < 4u) { return fail(VoxError::Malformed, "XYZI chunk too small"); }
                uint32_t num_voxels = 0;
                if (!read_u32(file, num_voxels)) { return fail(VoxError::ReadFailed, "XYZI num_voxels"); }

                const uint64_t max_vol = static_cast<uint64_t>(size_x)
                                       * static_cast<uint64_t>(size_y)
                                       * static_cast<uint64_t>(size_z);
                if (static_cast<uint64_t>(num_voxels) > max_vol) {
                    return fail(VoxError::Malformed, "XYZI num_voxels exceeds SIZE volume");
                }
                const uint64_t needed = 4ull + static_cast<uint64_t>(num_voxels) * 4ull;
                if (static_cast<uint64_t>(csz) < needed) {
                    return fail(VoxError::Malformed, "XYZI content too small for stated voxel count");
                }

                raw_voxels.reserve(num_voxels);
                for (uint32_t i = 0; i < num_voxels; ++i) {
                    uint8_t vx = 0, vy = 0, vz = 0, ci = 0;
                    if (!read_u8(file, vx) || !read_u8(file, vy) || !read_u8(file, vz) || !read_u8(file, ci)) {
                        return fail(VoxError::ReadFailed, "XYZI voxel read");
                    }
                    if (ci == 0) { continue; } // spec says 0 is never written, but skip safely
                    if (vx >= size_x || vy >= size_y || vz >= size_z) {
                        spdlog::debug("[vox_import] voxel ({},{},{}) out of bounds ({},{},{}) — skipped",
                                      vx, vy, vz, size_x, size_y, size_z);
                        continue;
                    }
                    raw_voxels.push_back({vx, vy, vz, ci});
                }
                has_xyzi = true;
            }
            // else: second XYZI = second model, silently skip (chunk_end seek handles it)
        }
        else if (id[0]=='R' && id[1]=='G' && id[2]=='B' && id[3]=='A') {
            if (!has_rgba) {
                if (csz < 256u * 4u) { return fail(VoxError::Malformed, "RGBA chunk too small"); }
                for (int i = 0; i < 256; ++i) {
                    uint8_t r = 0, g = 0, b = 0, a = 0;
                    if (!read_u8(file, r) || !read_u8(file, g) || !read_u8(file, b) || !read_u8(file, a)) {
                        return fail(VoxError::ReadFailed, "RGBA read");
                    }
                    raw_rgba[static_cast<size_t>(i)] =
                        static_cast<uint32_t>(r)
                      | (static_cast<uint32_t>(g) << 8)
                      | (static_cast<uint32_t>(b) << 16)
                      | (static_cast<uint32_t>(a) << 24);
                }
                has_rgba = true;
            }
        }
        else {
            skip_counts[std::string{id, 4}]++;
        }

        // Always seek to the end of this chunk (handles partial reads + children)
        file.seekg(chunk_end);
    }

    if (!has_size || !has_xyzi) {
        return fail(VoxError::Malformed, "missing SIZE or XYZI chunk");
    }

    if (!skip_counts.empty()) {
        std::ostringstream oss;
        oss << "[vox_import] skipped chunks:";
        for (const auto& [k, v] : skip_counts) {
            oss << ' ' << k << '=' << v;
        }
        spdlog::debug("{}", oss.str());
    }

    // ---- Build palette ----
    // RGBA chunk: raw_rgba[i] = color for color_index (i+1), for i in [0,254].
    // Default palette: kDefaultVoxPalette[ci] = color for color_index ci (1-based).
    Model model;
    for (int ci = 1; ci <= 255; ++ci) {
        const uint32_t packed = has_rgba
            ? raw_rgba[static_cast<size_t>(ci - 1)]
            : kDefaultVoxPalette[ci];
        model.palette.materials[static_cast<size_t>(ci)] = packed_to_material(packed);
    }

    // ---- Build chunks (Y-up coordinate swap) ----
    // MagicaVoxel Z-up → engine Y-up:
    //   engine_x = vox_x
    //   engine_y = vox_z   (vox Z becomes our Y)
    //   engine_z = vox_y   (vox Y becomes our Z)
    // Vox coords are in [0, size-1] so all world coords are non-negative;
    // integer division equals floor-division here.
    for (const auto& rv : raw_voxels) {
        const int wx = static_cast<int>(rv.x);
        const int wy = static_cast<int>(rv.z); // vox Z → engine Y
        const int wz = static_cast<int>(rv.y); // vox Y → engine Z

        const int cx = wx / CHUNK_SIZE;
        const int cy = wy / CHUNK_SIZE;
        const int cz = wz / CHUNK_SIZE;
        const glm::ivec3 cc{ cx, cy, cz };

        const int lx = wx - cx * CHUNK_SIZE;
        const int ly = wy - cy * CHUNK_SIZE;
        const int lz = wz - cz * CHUNK_SIZE;

        Chunk& chunk   = model.chunks[cc];
        chunk.position = cc;
        chunk.voxels[Chunk::index(lx, ly, lz)] = rv.ci;
    }

    model.recompute_bounds();

    const std::string stem = path.stem().string();
    model.name         = stem;
    model.palette.name = stem + "_palette";
    // model.id is auto-generated by UUID::generate() in the default constructor

    // ---- Stats ----
    // palette_colors_used: distinct color indices across all placed voxels
    std::array<bool, 256> seen{};
    for (const auto& kv : model.chunks) {
        for (auto v : kv.second.voxels) {
            if (v != 0) { seen[v] = true; }
        }
    }
    int palette_colors_used = 0;
    for (int i = 1; i < 256; ++i) {
        if (seen[static_cast<size_t>(i)]) { ++palette_colors_used; }
    }

    const auto t1 = std::chrono::steady_clock::now();
    const auto parse_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    // dims are post-Y-up-swap: engine (x, y, z) = vox (x, z, y)
    spdlog::info("[vox_import] file=\"{}\" dims={}x{}x{} voxels={} chunks={}"
                 " palette_colors_used={} parse_ms={} bounds=[{},{},{}]-[{},{},{}]",
                 stem,
                 size_x, size_z, size_y,
                 raw_voxels.size(),
                 model.chunks.size(),
                 palette_colors_used,
                 parse_ms,
                 model.bounds_min.x, model.bounds_min.y, model.bounds_min.z,
                 model.bounds_max.x, model.bounds_max.y, model.bounds_max.z);

    if (out_err) { *out_err = VoxError::Ok; }
    return model;
}

} // namespace vox
