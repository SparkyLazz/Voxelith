#include "editor/menu_bar.h"
#include "editor/editor_state.h"

#pragma warning(push)
#pragma warning(disable : 4100 4127 4201 4244 4245 4267 4305)
#include <imgui.h>
#pragma warning(pop)

#include <filesystem>

namespace ed {

MenuResult draw_menu_bar(EditorState& state) {
    MenuResult result;

    // Shared state for all modals — only one open at a time.
    static char s_path[512] = {};
    static bool s_error     = false;

    // Deferred open flags: set from inside the menu, consumed outside it.
    // This avoids associating the popup with the menu-bar window's ID stack.
    static bool s_want_open_vox = false;
    static bool s_want_open_vxm = false;
    static bool s_want_save_vxm = false;

    // ---- Main menu bar ----
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {

            if (ImGui::MenuItem("Open .vox...")) {
                s_want_open_vox = true;
                s_path[0]       = '\0';
                s_error         = false;
            }
            if (ImGui::MenuItem("Open .vxm...")) {
                s_want_open_vxm = true;
                s_path[0]       = '\0';
                s_error         = false;
            }

            ImGui::Separator();

            {
                const bool can_save = state.has_user_loaded_model();
                if (!can_save) ImGui::BeginDisabled();
                if (ImGui::MenuItem("Save As .vxm...")) {
                    s_want_save_vxm = true;
                    s_path[0]       = '\0';
                    s_error         = false;
                }
                if (!can_save) ImGui::EndDisabled();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Quit")) {
                result.quit_requested = true;
            }

            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // ---- Trigger popups outside menu-bar scope ----
    if (s_want_open_vox) { ImGui::OpenPopup("Open .vox");    s_want_open_vox = false; }
    if (s_want_open_vxm) { ImGui::OpenPopup("Open .vxm");    s_want_open_vxm = false; }
    if (s_want_save_vxm) { ImGui::OpenPopup("Save As .vxm"); s_want_save_vxm = false; }

    // ---- Open .vox modal ----
    if (ImGui::BeginPopupModal("Open .vox", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Path to .vox file:");
        ImGui::SetNextItemWidth(420.0f);
        ImGui::InputText("##vox_path", s_path, sizeof(s_path));
        if (s_error) {
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Load failed \xe2\x80\x94 see log");
        }
        ImGui::Spacing();
        if (ImGui::Button("Open", ImVec2(100.0f, 0.0f))) {
            if (state.load_vox(std::filesystem::path{s_path})) {
                result.model_changed = true;
                s_error              = false;
                ImGui::CloseCurrentPopup();
            } else {
                s_error = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) {
            s_error = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ---- Open .vxm modal ----
    if (ImGui::BeginPopupModal("Open .vxm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Path to .vxm file:");
        ImGui::SetNextItemWidth(420.0f);
        ImGui::InputText("##vxm_path", s_path, sizeof(s_path));
        if (s_error) {
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Load failed \xe2\x80\x94 see log");
        }
        ImGui::Spacing();
        if (ImGui::Button("Open", ImVec2(100.0f, 0.0f))) {
            if (state.load_vxm(std::filesystem::path{s_path})) {
                result.model_changed = true;
                s_error              = false;
                ImGui::CloseCurrentPopup();
            } else {
                s_error = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) {
            s_error = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ---- Save As .vxm modal ----
    if (ImGui::BeginPopupModal("Save As .vxm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Save path (.vxm):");
        ImGui::SetNextItemWidth(420.0f);
        ImGui::InputText("##save_path", s_path, sizeof(s_path));
        if (s_error) {
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Save failed \xe2\x80\x94 see log");
        }
        ImGui::Spacing();
        if (ImGui::Button("Save", ImVec2(100.0f, 0.0f))) {
            if (state.save_vxm(std::filesystem::path{s_path})) {
                s_error = false;
                ImGui::CloseCurrentPopup();
            } else {
                s_error = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) {
            s_error = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    return result;
}

} // namespace ed
