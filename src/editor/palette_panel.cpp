#include "editor/palette_panel.h"
#include "editor/editor_state.h"

#include "voxel/chunk.h"
#include "voxel/material.h"
#include "voxel/model.h"

#pragma warning(push)
#pragma warning(disable : 4100 4127 4201 4244 4245 4267 4305)
#include <imgui.h>
#pragma warning(pop)

#include <spdlog/spdlog.h>

#include <array>
#include <cstdint>

namespace ed {

PalettePanelResult draw_palette_panel(EditorState& state) {
    PalettePanelResult result;

    vox::Model* model = state.active_model();
    if (!model) return result;

    // Detect model change, reset selection so we don't edit the wrong palette slot.
    static int       s_selected_index = -1;
    static vox::UUID s_last_model_id  = {};
    static bool      s_last_id_valid  = false;

    const bool id_changed = !s_last_id_valid || !(s_last_model_id == model->id);
    if (id_changed) {
        if (s_last_id_valid) {
            spdlog::debug("[palette_panel] model changed, selection reset");
        }
        s_selected_index = -1;
        s_last_model_id  = model->id;
        s_last_id_valid  = true;

    }

    ImGui::Begin("Palette");

    // Region 1 — header.
    if (model->name.empty()) {
        ImGui::TextUnformatted("(no model)");
        ImGui::End();
        return result;
    }

    ImGui::Text("Palette: %s", model->name.c_str());
    ImGui::Separator();

    // Build used-set: which palette indices are referenced by at least one voxel.
    std::array<bool, 256> used{};
    for (const auto& kvp : model->chunks) {
        const auto& ch = kvp.second;
        if (ch.is_empty()) continue;
        for (const uint8_t v : ch.voxels) {
            if (v != 0) used[v] = true;
        }
    }

    // Region 2 — 16×16 swatch grid.
    constexpr float SWATCH_SIZE = 24.0f;
    const ImVec2    swatch_sz(SWATCH_SIZE, SWATCH_SIZE);

    for (int i = 0; i < 256; ++i) {
        if (i % 16 != 0) ImGui::SameLine(0.0f, 2.0f);

        ImGui::PushID(i);

        if (i == 0) {
            // Empty slot — not selectable; rendered as a dark box with an X.
            (void)ImGui::ColorButton("##empty",
                ImVec4(0.12f, 0.12f, 0.12f, 1.0f),
                ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoTooltip,
                swatch_sz);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 p = ImGui::GetItemRectMin();
            const ImVec2 q = ImGui::GetItemRectMax();
            dl->AddLine({p.x + 3.f, p.y + 3.f}, {q.x - 3.f, q.y - 3.f}, IM_COL32(120, 120, 120, 255), 1.5f);
            dl->AddLine({q.x - 3.f, p.y + 3.f}, {p.x + 3.f, q.y - 3.f}, IM_COL32(120, 120, 120, 255), 1.5f);
        } else {
            const auto&  mat         = model->palette.materials[i];
            const bool   is_selected = (s_selected_index == i);
            const ImVec4 color(mat.albedo.r, mat.albedo.g, mat.albedo.b, 1.0f);

            if (ImGui::ColorButton("##sw", color,
                    ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoTooltip,
                    swatch_sz)) {
                s_selected_index = i;
            }

            // Yellow border on the selected swatch.
            if (is_selected) {
                ImDrawList* dl = ImGui::GetWindowDrawList();
                const ImVec2 p = ImGui::GetItemRectMin();
                const ImVec2 q = ImGui::GetItemRectMax();
                dl->AddRect(p, q, IM_COL32(255, 220, 0, 255), 0.0f, 0, 2.0f);
            }

            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Index %d \xe2\x80\x94 RGB(%d, %d, %d) [used: %s]",
                    i,
                    static_cast<int>(mat.albedo.r * 255.0f + 0.5f),
                    static_cast<int>(mat.albedo.g * 255.0f + 0.5f),
                    static_cast<int>(mat.albedo.b * 255.0f + 0.5f),
                    used[i] ? "yes" : "no");
                ImGui::EndTooltip();
            }
        }

        ImGui::PopID();
    }

    // Region 3 — material editor for the selected index.
    if (s_selected_index > 0) {
        ImGui::Separator();
        ImGui::Text("Editing index %d", s_selected_index);

        auto& mat = model->palette.materials[s_selected_index];
        constexpr const char* PHASE4_TIP = "No visual effect until Phase 4 (PBR).";

        // Albedo — write triggers a mesh rebuild so colors update live.
        float albedo[3] = {mat.albedo.r, mat.albedo.g, mat.albedo.b};
        if (ImGui::ColorEdit3("Albedo##pal", albedo)) {
            mat.albedo            = {albedo[0], albedo[1], albedo[2]};
            result.albedo_changed = true;
        }

        // Roughness.
        ImGui::SliderFloat("Roughness##pal", &mat.roughness, 0.0f, 1.0f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", PHASE4_TIP);

        // Metallic.
        ImGui::SliderFloat("Metallic##pal", &mat.metallic, 0.0f, 1.0f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", PHASE4_TIP);

        // Emission — wrapped in a group so IsItemHovered covers the whole composite widget.
        float emission[3] = {mat.emission.r, mat.emission.g, mat.emission.b};
        ImGui::BeginGroup();
        if (ImGui::ColorEdit3("Emission##pal", emission)) {
            mat.emission = {emission[0], emission[1], emission[2]};
        }
        ImGui::EndGroup();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", PHASE4_TIP);

        ImGui::SliderFloat("Emission Strength##pal", &mat.emission_strength,
            0.0f, 50.0f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", PHASE4_TIP);

        // Transparency.
        ImGui::SliderFloat("Transparency##pal", &mat.transparency, 0.0f, 1.0f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", PHASE4_TIP);

        // IOR.
        ImGui::SliderFloat("IOR##pal", &mat.ior, 1.0f, 3.0f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", PHASE4_TIP);

        // Material flags.
        {
            const uint32_t glass_bit   = static_cast<uint32_t>(vox::MaterialFlag::Glass);
            const uint32_t foliage_bit = static_cast<uint32_t>(vox::MaterialFlag::Foliage);

            bool is_glass = (mat.flags & glass_bit) != 0u;
            if (ImGui::Checkbox("Glass##pal", &is_glass)) {
                if (is_glass) mat.flags |= glass_bit;
                else          mat.flags &= ~glass_bit;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", PHASE4_TIP);

            ImGui::SameLine();

            bool is_foliage = (mat.flags & foliage_bit) != 0u;
            if (ImGui::Checkbox("Foliage##pal", &is_foliage)) {
                if (is_foliage) mat.flags |= foliage_bit;
                else            mat.flags &= ~foliage_bit;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", PHASE4_TIP);
        }
    }

    ImGui::End();
    return result;
}

} // namespace ed
