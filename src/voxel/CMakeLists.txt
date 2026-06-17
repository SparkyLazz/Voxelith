add_library(voxel STATIC
    voxel_types.cpp
    vxm_io.cpp
)

# Expose src/ so consumers write: #include "voxel/model.h"
target_include_directories(voxel PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(voxel
    PUBLIC  glm::glm
    PRIVATE spdlog::spdlog
)

target_compile_features(voxel PUBLIC
    cxx_std_20
)
