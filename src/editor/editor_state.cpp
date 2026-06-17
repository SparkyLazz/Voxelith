#include "editor/editor_state.h"

#include <spdlog/spdlog.h>

#include "voxel/vox_import.h"
#include "voxel/vxm_io.h"

namespace ed {

EditorState::EditorState()  = default;
EditorState::~EditorState() = default;

void EditorState::new_empty_model() {
    active_ = std::make_unique<vox::Model>();
    active_->name = "untitled";
    user_loaded_ = false;
    spdlog::info("[editor_state] new_empty_model id={}", active_->id.to_string());
}

bool EditorState::load_vox(const std::filesystem::path& path) {
    vox::VoxError err{};
    auto m = vox::import_vox(path, &err);
    if (!m) {
        spdlog::error("[editor_state] load_vox failed: path=\"{}\" err={}",
                      path.string(), vox::to_string(err));
        return false;
    }
    active_ = std::make_unique<vox::Model>(std::move(*m));
    user_loaded_ = true;
    dump_stats();
    return true;
}

bool EditorState::load_vxm(const std::filesystem::path& path) {
    vox::VxmError err{};
    auto m = vox::read_vxm(path, &err);
    if (!m) {
        spdlog::error("[editor_state] load_vxm failed: path=\"{}\" err={}",
                      path.string(), vox::to_string(err));
        return false;
    }
    active_ = std::make_unique<vox::Model>(std::move(*m));
    user_loaded_ = true;
    dump_stats();
    return true;
}

bool EditorState::save_vxm(const std::filesystem::path& path) const {
    if (!active_) {
        spdlog::error("[editor_state] save_vxm called with no active model");
        return false;
    }
    vox::VxmError err{};
    if (!vox::write_vxm(*active_, path, &err)) {
        spdlog::error("[editor_state] save_vxm failed: path=\"{}\" err={}",
                      path.string(), vox::to_string(err));
        return false;
    }
    spdlog::info("[editor_state] save_vxm OK: path=\"{}\"", path.string());
    return true;
}

void EditorState::dump_stats() const {
    if (!active_) {
        spdlog::warn("[editor_state] dump_stats: no active model");
        return;
    }
    const auto& m = *active_;
    spdlog::info("[editor_state] name=\"{}\" id={} chunks={} voxels={} "
                 "bounds=[{},{},{}]-[{},{},{}]",
                 m.name, m.id.to_string(),
                 m.chunk_count(), m.non_empty_voxel_count(),
                 m.bounds_min.x, m.bounds_min.y, m.bounds_min.z,
                 m.bounds_max.x, m.bounds_max.y, m.bounds_max.z);
}

const vox::Model* EditorState::active_model() const noexcept {
    return active_.get();
}

vox::Model* EditorState::active_model() noexcept {
    return active_.get();
}

bool EditorState::has_user_loaded_model() const noexcept {
    return user_loaded_;
}

} // namespace ed
