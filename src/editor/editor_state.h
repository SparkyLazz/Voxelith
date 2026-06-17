#pragma once

#include <filesystem>
#include <memory>

#include "voxel/model.h"

namespace ed {

class EditorState {
public:
    EditorState();
    ~EditorState();

    EditorState(const EditorState&)            = delete;
    EditorState& operator=(const EditorState&) = delete;
    EditorState(EditorState&&)                 = delete;
    EditorState& operator=(EditorState&&)      = delete;

    // Loaders. On failure: logs, leaves any previously-loaded model intact, returns false.
    bool load_vox(const std::filesystem::path& path);
    bool load_vxm(const std::filesystem::path& path);

    // Saver. Fails if no active model.
    bool save_vxm(const std::filesystem::path& path) const;

    // Replace active model with a fresh empty one (name="untitled", new UUID, default palette).
    void new_empty_model();

    // Log a stable [editor_state] stats line (name, id, chunks, voxels, bounds).
    void dump_stats() const;

    // Always non-null after the first new_empty_model() / load call.
    const vox::Model* active_model() const noexcept;
    vox::Model*       active_model()       noexcept;

    // True after a successful load_vox or load_vxm.
    bool has_user_loaded_model() const noexcept;

private:
    std::unique_ptr<vox::Model> active_;
    bool user_loaded_ = false;
};

} // namespace ed
