add_library(editor STATIC
    editor_state.cpp
    editor_state.h
    menu_bar.cpp
    menu_bar.h
    palette_panel.cpp
    palette_panel.h
)

target_compile_features(editor PUBLIC cxx_std_20)

target_include_directories(editor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(editor
    PUBLIC  voxel glm::glm
    PRIVATE spdlog::spdlog imgui::imgui
)
