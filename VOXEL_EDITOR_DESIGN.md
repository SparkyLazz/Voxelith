# Voxel Editor — Project Design Document

**Last updated:** June 14, 2026
**Status:** Phase 0 — Foundation
**Author:** Project owner + Claude (architecture chat)

---

## 1. Vision

A voxel editor and world-building tool with rendering quality dramatically beyond what existing tools (MagicaVoxel, Avoyd, Goxel) offer. The visual target is "Minecraft with high-end shaders" — atmospheric scattering, volumetric clouds, PBR materials, water with reflections and refraction, soft shadows, bloom, tonemapping.

The product has two modes:

- **Editor mode** — build voxel models (houses, props, characters) with full editing tools
- **World mode** — drag finished models as instances into a larger streaming world, duplicate, arrange

Materials are assigned by palette: each color in the palette has a material profile (albedo, roughness, metallic, emission, transparency, refraction). Every voxel of that color inherits the material. A user can mark "yellow = emissive" and every yellow voxel emits light.

---

## 2. Locked Technical Decisions

| Decision | Choice | Rationale |
|---|---|---|
| Language | C++20 | Every graphics tutorial, book, and reference codebase is in C++. Reduces friction while learning. |
| Graphics API | Vulkan 1.3 | Modern compute, indirect drawing, bindless, RT extensions for stretch goals. |
| GPU target | RTX 5060 (dev), GTX 1060 / RX 580 minimum | Modern features assumed. RT optional but architected-in. |
| Platform | Windows 10/11 only | Per requirements. Cross-platform deferred. |
| Build system | CMake 3.25+ | Standard, works with vcpkg and Visual Studio. |
| Package manager | vcpkg (manifest mode) | Easy dependency management for Vulkan ecosystem. |
| IDE | Visual Studio 2022 | Best Windows + C++ + CMake experience. |
| UI library | Dear ImGui (docking branch) | Industry standard for engine tooling. Dockable panels like Blender. |
| Window/input | GLFW | Battle-tested, plays well with Vulkan and ImGui. |
| Math | GLM | Standard for Vulkan/OpenGL C++ projects. |
| Image loading | stb_image | Header-only, trivial. |
| Model import | Custom .vox parser (MagicaVoxel) + native format | Compatibility + own format for advanced features. |
| Logging | spdlog | Fast, easy. |

### Dependencies summary (vcpkg manifest)

```
vulkan-headers, vulkan-loader, vulkan-validationlayers,
glfw3, glm, imgui[docking-experimental,vulkan-binding,glfw-binding],
stb, spdlog, fmt, tinygltf (for stretch goal)
```

---

## 3. Visual Reference & Rendering Target

Reference: Minecraft with Kappa / Project Luma shaders.

These are **rasterization-based shader packs**, not path tracing. All effects are achievable with modern raster + screen-space techniques. This is a realistic 6-month target.

### Feature list (Phase 4 deliverable)

- PBR shading (Cook-Torrance, GGX, energy-conserving)
- Cascaded shadow maps (3-4 cascades, PCF or PCSS for soft shadows)
- Atmospheric scattering sky (Hillaire 2020 method)
- Volumetric clouds (raymarched, Schneider/Häggström method)
- Water: planar reflections + screen-space refraction + animated normal maps
- Screen-space ambient occlusion (GTAO or SSAO)
- Bloom (Kawase or compute-based downsampling)
- HDR pipeline + ACES or AgX tonemapping
- Temporal anti-aliasing (TAA)
- Emission with bloom feedback (lit voxels glow)

### Stretch (Phase 4.5, optional)

- Hardware ray-traced shadows (Vulkan RT pipeline)
- Hardware ray-traced reflections for water/metal
- These are bolted onto the raster path — not a replacement.

---

## 4. Architecture Overview

```
┌──────────────────────────────────────────────────────────────┐
│                       Application Layer                      │
│  (main loop, mode switching, input routing, ImGui frame)     │
└─────┬────────────────────────────────────┬───────────────────┘
      │                                    │
┌─────▼────────────┐              ┌────────▼──────────────────┐
│   Editor Core    │              │   World Core              │
│  - Active voxel  │              │  - Scene graph            │
│    model         │              │  - Model instances        │
│  - Tool state    │              │  - Streaming manager      │
│  - Undo/redo     │              │  - Spatial index          │
│  - Palette       │              │                           │
└─────┬────────────┘              └────────┬──────────────────┘
      │                                    │
      └────────────┬───────────────────────┘
                   │
         ┌─────────▼──────────┐
         │   Voxel Data       │
         │  - Chunk (32³)     │
         │  - Palette         │
         │  - Material defs   │
         │  - Serialization   │
         └─────────┬──────────┘
                   │
         ┌─────────▼──────────┐
         │   Mesh Pipeline    │
         │  - Greedy mesher   │
         │  - GPU upload      │
         │  - LOD             │
         └─────────┬──────────┘
                   │
         ┌─────────▼──────────────────────────────────┐
         │   Renderer (Vulkan)                        │
         │  - Frame graph                             │
         │  - GBuffer / Forward+ (TBD in renderer chat)│
         │  - Shadow pass                             │
         │  - Sky + clouds                            │
         │  - Water                                   │
         │  - Post-process stack                      │
         └────────────────────────────────────────────┘
```

### Threading model (planned)

- Main thread: input, ImGui, render submission
- Worker pool (N-1 cores): meshing, chunk generation, streaming I/O
- Lock-free queues for chunk → GPU upload requests

---

## 5. Data Model

### Chunk

```cpp
constexpr int CHUNK_SIZE = 32;  // 32^3 voxels = 32,768 per chunk
struct Chunk {
    uint8_t voxels[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE]; // palette index, 0 = empty
    glm::ivec3 position;        // in chunk coordinates
    uint32_t dirty_flags;       // for re-meshing
    MeshHandle mesh;            // GPU mesh, nullopt if empty
};
```

### Palette + Material

```cpp
struct Material {
    glm::vec3 albedo;
    float roughness;
    float metallic;
    glm::vec3 emission;     // HDR color, 0 = non-emissive
    float emission_strength;
    float transparency;     // 0 = opaque, 1 = fully transparent
    float ior;              // index of refraction (glass, water)
    uint32_t flags;         // glass, foliage (alpha-tested), etc.
};

struct Palette {
    Material materials[256]; // index 0 reserved for "empty"
    std::string name;
};
```

### Model (the thing you build in editor)

```cpp
struct Model {
    std::unordered_map<glm::ivec3, Chunk> chunks; // sparse
    Palette palette;
    glm::ivec3 bounds_min, bounds_max;
    std::string name;
    UUID id;
};
```

### World

```cpp
struct ModelInstance {
    UUID model_id;          // reference to a Model
    glm::vec3 position;
    glm::quat rotation;
    float scale;
};

struct World {
    std::unordered_map<UUID, Model> model_library;
    std::vector<ModelInstance> instances;
    StreamingManager streaming;
};
```

---

## 6. File Formats

- **`.vox`** — MagicaVoxel format, read-only import for compatibility
- **`.vxm`** — Custom native model format (palette + chunks, binary, versioned header)
- **`.vxw`** — Custom native world format (model references + instances)

Format spec lives in the Editor sub-chat once we get to Phase 1.

---

## 7. Phase Plan

### Phase 0 — Foundation *(Weeks 1-2)*
- Visual Studio 2022 + Vulkan SDK install
- CMake project skeleton with vcpkg
- GLFW window + Vulkan device + swapchain
- ImGui hooked up with docking
- Free-fly camera
- **Deliverable:** Rotating cube in a window, ImGui overlay, camera you can fly.

### Phase 1 — Voxel Data Core *(Weeks 3-5)*
- Chunk + Model + Palette + Material structs
- Serialization (.vxm format)
- .vox importer
- Palette editor UI (assign materials to colors)
- **Deliverable:** Load a MagicaVoxel model, dump stats, save as .vxm, reload.

### Phase 2 — Mesh + Basic Renderer *(Weeks 6-8)*
- Greedy meshing algorithm
- GPU mesh upload pipeline
- Forward renderer with single directional light, flat shading
- Frustum culling
- **Deliverable:** Loaded model rendered in 3D, navigable with camera.

### Phase 3 — Editor MVP *(Weeks 9-11)*
- Place / erase / paint / picker tools
- Selection box, copy, paste, fill
- Undo / redo (command pattern)
- Save / load native format
- Palette editor with full material UI
- Object library panel
- **Deliverable:** A user can build a house from scratch in your editor and save it.

### Phase 4 — Advanced Rendering *(Weeks 12-17)* ⚠️ **biggest phase**
Six weeks. This is what makes your editor look like the references.
- Week 12: PBR shading + GBuffer or Forward+ decision
- Week 13: Cascaded shadow maps with PCF/PCSS
- Week 14: Atmospheric sky + sun
- Week 15: Volumetric clouds
- Week 16: Water (reflections, refraction, normals, foam)
- Week 17: Post stack — SSAO, bloom, tonemapping, TAA
- **Deliverable:** Editor scene looks like the reference screenshots.

### Phase 4.5 — RT (optional stretch) *(slipped into Phase 5 if behind)*
- RT shadows
- RT reflections
- **Deliverable:** Toggle in settings, side-by-side comparison.

### Phase 5 — World Mode + Streaming *(Weeks 18-21)*
- World scene separate from editor
- Drag-drop model instances
- Chunk streaming around camera (load/unload by distance)
- GPU instancing for repeated placements
- Simple LOD (lower-detail meshes at distance)
- **Deliverable:** Build several models, drop them into a world, fly around.

### Phase 6 — Polish *(Weeks 22-24)*
- Performance pass (profile, optimize hotspots)
- Mirror, rotate, group operations
- Material presets library
- Export to .obj or .gltf
- Bug fixes
- **Deliverable:** Shippable v1.

---

## 8. Sub-Chat Structure

| Chat | Scope | Phases |
|---|---|---|
| **Architecture** (this one) | High-level decisions, phase transitions, blockers, "what next" | All |
| **Setup/Build** | CMake, vcpkg, IDE, dependency issues | 0, ongoing |
| **Editor** | Data structures, tools, UI, file formats, undo/redo, world mode | 1, 3, 5 |
| **Renderer** | All graphics — Vulkan setup, shaders, lighting, post-processing | 2, 4, 4.5 |

**Workflow per sub-chat:**
1. Open with this design doc pasted as context
2. State which phase + which week we're on
3. Plan mode → Claude Code executes → review → next step
4. Significant decisions or blockers → bring back to architecture chat

---

## 9. Open Questions / Decisions Deferred

These will be answered when their phase arrives, not now:

- Forward+ vs. Deferred (Phase 4 week 12 — depends on transparency needs)
- Greedy meshing vs. binary greedy vs. surface nets (Phase 2)
- Shadow technique: PCF vs. PCSS vs. VSM (Phase 4 week 13)
- Cloud rendering: half-res reconstruction strategy (Phase 4 week 15)
- World streaming: chunk size for world vs. model chunks (Phase 5)

---

## 10. Success Criteria

By end of Phase 6:
1. User can build a detailed voxel model (~512³ scale) in the editor
2. Materials with emission, glass, metal all visibly work
3. World mode supports 50+ model instances streaming smoothly at 60 FPS on the dev GPU
4. A side-by-side screenshot vs. MagicaVoxel makes the difference obvious
5. Stable enough that you can use it for 1+ hour without crashes

---

## 11. Risks

| Risk | Mitigation |
|---|---|
| Phase 4 takes longer than 6 weeks | Cut features in order: TAA → clouds → water → SSAO. Keep PBR + shadows + sky as non-negotiable. |
| Vulkan learning curve stalls Phase 0-2 | Use vk-bootstrap to skip boilerplate. Lean on Vulkan Tutorial + Vulkan Guide. |
| Greedy meshing bugs | Start with naive per-voxel meshing as fallback. Optimize after Phase 2 works. |
| Scope creep | This document is the source of truth. New ideas go to "Backlog" section, not into the current phase. |

---

## 12. Backlog (post-v1, not in scope)

- Animation (rigging, keyframes)
- Scripting (Lua for procedural generation)
- Multi-user editing
- Mac / Linux ports
- Path-traced offline render mode
- VR mode
- Asset marketplace integration

---

*End of design doc. Paste into each sub-chat opening message.*
