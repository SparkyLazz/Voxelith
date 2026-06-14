# VoxelEditor — Claude Code Context

## Project Summary

VoxelEditor is a Windows-only voxel editor and world-building tool targeting rendering
quality far beyond existing tools (MagicaVoxel, Avoyd, Goxel). The visual target is
"Minecraft with high-end shaders" — PBR materials, cascaded shadow maps, atmospheric
scattering, volumetric clouds, water reflections/refraction, bloom, tonemapping, and
optional hardware ray tracing. It has two modes: **Editor** (build voxel models with
tools, undo/redo, palette-based materials) and **World** (place model instances in a
streaming scene). Materials are palette-driven: each palette slot carries a full PBR
profile (albedo, roughness, metallic, emission, transparency, IOR). Graphics are
Vulkan 1.3 via vk-bootstrap with dynamic rendering (no legacy render passes). See
VOXEL_EDITOR_DESIGN.md for the complete design — architecture, data model, phase plan,
open questions, and success criteria.

## Current Phase

**Phase 0 — Foundation.** Working toward rotating cube + ImGui + free-fly camera.

## Build

Prerequisites: Visual Studio 2022 (MSVC), Vulkan SDK, CMake 3.25+, Ninja,
vcpkg at `C:\dev\vcpkg` with `VCPKG_ROOT=C:\dev\vcpkg` in user environment.

**CLion workflow:**
1. File → Open → select `D:\Personal Projects\VoxelRenderer` (open as folder)
2. Settings → Build, Execution, Deployment → Toolchains → + → Visual Studio
   - Architecture: amd64, leave everything else auto-detected
3. Settings → Build, Execution, Deployment → CMake → select profile
   `windows-msvc-debug` (auto-detected from CMakePresets.json)
4. Click the CMake reload icon — first configure pulls vcpkg deps (15-30 min)
5. Build: Ctrl+F9 (or hammer icon)
6. Run: Ctrl+F10, select VoxelEditor target

**Manual cmake (outside CLion):**
```
cmake --preset windows-msvc-debug
cmake --build build/debug
```

## Source Tree

```
VoxelRenderer/
├── src/
│   └── main.cpp          # entry point
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json            # vcpkg manifest — all deps declared here
├── CLAUDE.md             # this file
├── VOXEL_EDITOR_DESIGN.md  # full design doc (create/maintain separately)
└── .gitignore
```

As the project grows, expect:
```
src/
├── renderer/             # Vulkan abstractions
├── world/                # voxel chunk data
├── ui/                   # ImGui panels
└── core/                 # camera, input, app loop
```

## Coding Conventions

- **Standard:** C++20. No compiler extensions (`CMAKE_CXX_EXTENSIONS OFF`).
- **Error handling:** Return codes, `std::expected`, or explicit error types.
  No exceptions for control flow.
- **Resources:** RAII over manual cleanup. Vulkan objects in RAII wrappers.
- **Math:** GLM for all vector/matrix/quaternion math.
- **Logging:** spdlog. No raw `printf` in non-trivial code.
- **Naming:**
  - Files: `snake_case.cpp` / `snake_case.h`
  - Types (classes, structs, enums): `PascalCase`
  - Functions and variables: `camelCase`
  - Constants and enum values: `UPPER_SNAKE_CASE`
- **Includes:** system/vendor headers before project headers; alphabetical within
  each group.

## Rules

- **Never commit** `build/`, `cmake-build-*/`, or `vcpkg_installed/` — all are
  in `.gitignore`.
- Warnings are errors eventually (`/WX` will be added once the codebase
  stabilizes). Fix warnings immediately; don't suppress them.
- MSVC-only. No MinGW/Clang/GCC compatibility shims needed.
