#pragma once

#include "editor/editor_state.h"

namespace ed {

struct MenuResult {
    bool model_changed  = false;
    bool quit_requested = false;
};

MenuResult draw_menu_bar(EditorState& state);

} // namespace ed
