#include "voxel/naive_mesher.h"

#include <spdlog/spdlog.h>

namespace vox {

namespace {

// Floor-division that works correctly for negative dividends.
// C++ truncates toward zero; we need floor toward -inf.
static int floor_div(int a, int n) {
    return a / n - (a % n != 0 && (a ^ n) < 0 ? 1 : 0);
}
static int floor_mod(int a, int n) {
    const int r = a % n;
    return (r != 0 && (r ^ n) < 0) ? r + n : r;
}

// Sample the voxel at world coord (wx,wy,wz). Returns 0 (empty) if the
// chunk doesn't exist or the voxel slot is empty.
static uint8_t sample(const Model& model, int wx, int wy, int wz) {
    const int cx = floor_div(wx, CHUNK_SIZE);
    const int cy = floor_div(wy, CHUNK_SIZE);
    const int cz = floor_div(wz, CHUNK_SIZE);
    const auto it = model.chunks.find(glm::ivec3{cx, cy, cz});
    if (it == model.chunks.end()) { return 0; }
    const int lx = floor_mod(wx, CHUNK_SIZE);
    const int ly = floor_mod(wy, CHUNK_SIZE);
    const int lz = floor_mod(wz, CHUNK_SIZE);
    return it->second.at(lx, ly, lz);
}

// Per-face description: neighbor direction + 4 vertex offsets from the
// voxel's minimum corner (wx, wy, wz). Vertices are in CCW order when
// viewed from outside the voxel (matches VK_FRONT_FACE_COUNTER_CLOCKWISE).
// Two triangles per face use index pattern: 0,1,2  0,2,3.
//
// Winding verified by cross-product of edge01 x edge02 for each face:
//   +X: (0,1,0)x(0,1,1) = (1,0,0)  → outward normal +X ✓
//   -X: (0,1,0)x(0,1,-1)= (-1,0,0) → -X ✓
//   +Y: (0,0,1)x(1,0,1) = (0,1,0)  → +Y ✓
//   -Y: (0,0,-1)x(1,0,-1)= (0,-1,0)→ -Y ✓
//   +Z: (1,0,0)x(1,1,0) = (0,0,1)  → +Z ✓
//   -Z: (-1,0,0)x(-1,1,0)=(0,0,-1) → -Z ✓
struct FaceVert { float dx, dy, dz; };
struct FaceDesc { int nx, ny, nz; FaceVert v[4]; };

static const FaceDesc kFaces[6] = {
    // +X (neighbor at wx+1, viewed from +X)
    { 1, 0, 0, { {1,0,0},{1,1,0},{1,1,1},{1,0,1} } },
    // -X (neighbor at wx-1, viewed from -X)
    {-1, 0, 0, { {0,0,1},{0,1,1},{0,1,0},{0,0,0} } },
    // +Y (neighbor at wy+1, viewed from +Y)
    { 0, 1, 0, { {0,1,0},{0,1,1},{1,1,1},{1,1,0} } },
    // -Y (neighbor at wy-1, viewed from -Y)
    { 0,-1, 0, { {0,0,1},{0,0,0},{1,0,0},{1,0,1} } },
    // +Z (neighbor at wz+1, viewed from +Z)
    { 0, 0, 1, { {0,0,1},{1,0,1},{1,1,1},{0,1,1} } },
    // -Z (neighbor at wz-1, viewed from -Z)
    { 0, 0,-1, { {1,0,0},{0,0,0},{0,1,0},{1,1,0} } },
};

} // anonymous namespace

MesherOutput build_naive_mesh(const Model& model) {
    MesherOutput out;
    bool logged[6] = {};

    for (const auto& [cc, chunk] : model.chunks) {
        if (chunk.is_empty()) { continue; }

        const glm::ivec3 origin = cc * CHUNK_SIZE;

        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int y = 0; y < CHUNK_SIZE; ++y) {
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    const uint8_t ci = chunk.at(x, y, z);
                    if (ci == 0) { continue; }

                    const glm::vec3 color =
                        model.palette.materials[static_cast<size_t>(ci)].albedo;

                    const int wx = origin.x + x;
                    const int wy = origin.y + y;
                    const int wz = origin.z + z;

                    for (int fi = 0; fi < 6; ++fi) {
                        const auto& face = kFaces[fi];
                        if (sample(model, wx + face.nx, wy + face.ny, wz + face.nz) != 0) {
                            continue;
                        }

                        const auto base_idx = static_cast<uint32_t>(out.vertices.size());
                        const float fx = static_cast<float>(wx);
                        const float fy = static_cast<float>(wy);
                        const float fz = static_cast<float>(wz);
                        for (const auto& fv : face.v) {
                            out.vertices.push_back({
                                { fx + fv.dx, fy + fv.dy, fz + fv.dz },
                                color
                            });
                        }
                        out.indices.push_back(base_idx + 0);
                        out.indices.push_back(base_idx + 1);
                        out.indices.push_back(base_idx + 2);
                        out.indices.push_back(base_idx + 0);
                        out.indices.push_back(base_idx + 2);
                        out.indices.push_back(base_idx + 3);

                        if (!logged[fi]) {
                            logged[fi] = true;
                            const auto& v = out.vertices;
                            spdlog::debug(
                                "[naive_mesher] first face dir_idx={} dir=({},{},{}) "
                                "v0=({:.0f},{:.0f},{:.0f}) v1=({:.0f},{:.0f},{:.0f}) "
                                "v2=({:.0f},{:.0f},{:.0f}) v3=({:.0f},{:.0f},{:.0f})",
                                fi, face.nx, face.ny, face.nz,
                                v[base_idx+0].position.x, v[base_idx+0].position.y, v[base_idx+0].position.z,
                                v[base_idx+1].position.x, v[base_idx+1].position.y, v[base_idx+1].position.z,
                                v[base_idx+2].position.x, v[base_idx+2].position.y, v[base_idx+2].position.z,
                                v[base_idx+3].position.x, v[base_idx+3].position.y, v[base_idx+3].position.z
                            );
                        }
                    }
                }
            }
        }
    }

    return out;
}

} // namespace vox
