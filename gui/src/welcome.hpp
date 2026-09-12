#pragma once

#include <imgui.h>
#include <rlImGui.h>
#include <algorithm>
#include <vector>
#include <string>
#include <portable-file-dialogs.h>
#include "app/widgets.hpp"
#include "window.hpp"

void begin_frame(void) {
    BeginDrawing();
    ClearBackground(Color{26, 28, 33, 255});

    rlImGuiBegin();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 9.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(18.0f, 6.0f));
}

void end_frame(void) {
    ImGui::PopStyleVar(2);

    rlImGuiEnd();
    EndDrawing();
}

static void fill_viewport(void) {
    const ImGuiViewport *viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
}

static void dashed_rect(ImDrawList *draw, const ImVec2 &min, const ImVec2 &max, ImU32 color) {
    const float dash = 9.0f;
    const float gap = 7.0f;

    for (float x = min.x; x < max.x; x += dash + gap) {
        const float to = std::min(x + dash, max.x);

        draw->AddLine(ImVec2(x, min.y), ImVec2(to, min.y), color);
        draw->AddLine(ImVec2(x, max.y), ImVec2(to, max.y), color);
    }

    for (float y = min.y; y < max.y; y += dash + gap) {
        const float to = std::min(y + dash, max.y);

        draw->AddLine(ImVec2(min.x, y), ImVec2(min.x, to), color);
        draw->AddLine(ImVec2(max.x, y), ImVec2(max.x, to), color);
    }
}

static void drop_zone(float height) {
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 max = ImVec2(min.x + ImGui::GetContentRegionAvail().x, min.y + height);

    ImDrawList *draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(min, max, ImGui::GetColorU32(theme::sunken_bg), 4.0f);
    dashed_rect(draw, min, max, ImGui::GetColorU32(theme::border));

    ImGui::SetCursorScreenPos(ImVec2(min.x, min.y + height * 0.5f - ImGui::GetTextLineHeightWithSpacing()));
    ui::centered_label("DROP A CARRIER HERE");
    ui::centered_label("PNG OR JPEG");

    ImGui::SetCursorScreenPos(ImVec2(min.x, max.y));
}

std::string welcome(const char *problem) {
    const float card_w = 460.0f;
    const float card_h = problem ? 356.0f : 320.0f;

    std::string target;

    while (!WindowShouldClose() && target.empty()) {
        if (IsFileDropped()) {
            const FilePathList dropped = LoadDroppedFiles();

            if (dropped.count > 0)
                target = dropped.paths[0];

            UnloadDroppedFiles(dropped);
        }

        begin_frame();
        fill_viewport();

        ImGui::Begin("##welcome", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus
        );

        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - card_w) * 0.5f, (ImGui::GetWindowHeight() - card_h) * 0.5f));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28.0f, 26.0f));
        ImGui::BeginChild("##card", ImVec2(card_w, card_h), ImGuiChildFlags_Borders);
        ImGui::PopStyleVar();

        ImGui::PushFont(nullptr, 38.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, theme::accent_hover);
        ui::centered_text(WINDOW_TITLE);
        ImGui::PopStyleColor();
        ImGui::PopFont();

        ui::centered_label("A STEGANOGRAPHY TOOLKIT");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        drop_zone(112.0f);

        ImGui::Spacing();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, theme::accent_soft);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme::accent_hover);

        if (ImGui::Button("Open a carrier...", ImVec2(-FLT_MIN, 0.0f))) {
            const std::vector<std::string> picked =
                pfd::open_file("Open a carrier", ".", {"Images", "*.png *.jpg *.jpeg", "All files", "*"}).result();

            if (!picked.empty())
                target = picked[0];
        }

        ImGui::PopStyleColor(3);

        if (problem) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, theme::byte_filled);
            ui::centered_text(problem);
            ImGui::PopStyleColor();
        }

        ImGui::EndChild();
        ImGui::End();

        end_frame();
    }

    return target;
}