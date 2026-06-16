cmake_minimum_required(VERSION 3.25)
project(VoxelEditor CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(
        /W4
        /utf-8
        /Zc:preprocessor
        /MP
    )
endif()

# ---- Dependencies ----
find_package(glfw3 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(vk-bootstrap CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

# ---- Shader compiler ----
# Separate cache variables avoid the collision where the second find_program
# sees the first one's cached result and GLSLC_IS_GLSLANG never gets set.
find_program(GLSLC_BIN NAMES glslc HINTS "$ENV{VULKAN_SDK}/Bin")
find_program(GLSLANGVALIDATOR_BIN
    NAMES glslangValidator
    HINTS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/glslang"
)

if(GLSLC_BIN)
    set(SHADER_COMPILER "${GLSLC_BIN}")
    set(GLSLC_IS_GLSLANG FALSE)
    message(STATUS "Shader compiler: glslc (${SHADER_COMPILER})")
elseif(GLSLANGVALIDATOR_BIN)
    set(SHADER_COMPILER "${GLSLANGVALIDATOR_BIN}")
    set(GLSLC_IS_GLSLANG TRUE)
    message(STATUS "Shader compiler: glslangValidator (${SHADER_COMPILER})")
else()
    message(FATAL_ERROR "No shader compiler found. Install Vulkan SDK (glslc) or vcpkg glslang[tools].")
endif()

# ---- Compile shaders ----
set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${SHADER_OUTPUT_DIR}")

set(SHADER_SOURCES
    "${CMAKE_SOURCE_DIR}/shaders/cube.vert"
    "${CMAKE_SOURCE_DIR}/shaders/cube.frag"
)

set(SHADER_SPV_FILES)
foreach(SHADER_SRC ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME "${SHADER_SRC}" NAME)
    set(SHADER_SPV "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")

    if(GLSLC_IS_GLSLANG)
        add_custom_command(
            OUTPUT  "${SHADER_SPV}"
            COMMAND "${SHADER_COMPILER}"
                    -V
                    --target-env vulkan1.3
                    -o "${SHADER_SPV}"
                    "${SHADER_SRC}"
            DEPENDS "${SHADER_SRC}"
            COMMENT "Compiling shader ${SHADER_NAME}"
            VERBATIM
        )
    else()
        add_custom_command(
            OUTPUT  "${SHADER_SPV}"
            COMMAND "${SHADER_COMPILER}"
                    --target-env=vulkan1.3
                    -O
                    -o "${SHADER_SPV}"
                    "${SHADER_SRC}"
            DEPENDS "${SHADER_SRC}"
            COMMENT "Compiling shader ${SHADER_NAME}"
            VERBATIM
        )
    endif()

    list(APPEND SHADER_SPV_FILES "${SHADER_SPV}")
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SHADER_SPV_FILES})

# ---- Voxel data library ----
add_subdirectory(src/voxel)

# ---- Executable ----
add_executable(VoxelEditor
    src/main.cpp
    src/renderer.cpp
    src/vma_impl.cpp
    src/camera.cpp
    src/input.cpp
    src/vk/vk_context.cpp
    src/vk/vk_swapchain.cpp
    src/vk/vk_frame_sync.cpp
    src/vk/vk_pipeline.cpp
    src/vk/vk_mesh.cpp
    src/vk/vk_imgui.cpp
)

add_dependencies(VoxelEditor Shaders)

target_include_directories(VoxelEditor PRIVATE src)

target_compile_definitions(VoxelEditor PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    SHADER_DIR="${SHADER_OUTPUT_DIR}"
    VOXEL_SMOKE_TEST
)

target_link_libraries(VoxelEditor PRIVATE
    glfw
    spdlog::spdlog
    fmt::fmt
    vk-bootstrap::vk-bootstrap
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator
    glm::glm
    imgui::imgui
    voxel
)
