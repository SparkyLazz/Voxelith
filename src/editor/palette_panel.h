#pragma once
#include "editor/editor_state.h"

namespace ed {

struct PalettePanelResult {
    bool albedo_changed = false;
};

PalettePanelResult draw_palette_panel(EditorState& state);

} // namespace ed
