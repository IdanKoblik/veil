#include "app/theme.hpp"
#include "app/widgets.hpp"
#include "documents/ImageDocument.hpp"
#include "raylib.h"
#include <algorithm>
#include <cfloat>
#include <imgui.h>
#include <rlImGui.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <veil/log.h>
#include <portable-file-dialogs.h>
#include "window.hpp"
#include "welcome.hpp"

static void load_fonts(void) {
    ImGuiIO &io = ImGui::GetIO();

#if defined(__linux__)
    const char *fonts[] = {
        "/usr/share/fonts/TTF/JetBrainsMonoNerdFont-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/usr/share/fonts/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/noto/NotoSansMono-Regular.ttf",
    };

    for (const char *path : fonts)
        if (FileExists(path) && io.Fonts->AddFontFromFileTTF(path, 20.0f))
            return;
#endif

    io.Fonts->AddFontDefault();
}

static void run(ImageDocument &document) {
    while (!WindowShouldClose() && !document.exit) {
        begin_frame();

        document.draw_navbar();

        fill_viewport();
        ImGui::Begin("##workspace", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                     ImGuiWindowFlags_NoBackground
        );

        document.render();

        ImGui::End();

        end_frame();
    }
}

int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    rlImGuiSetLoadFontsCallback(load_fonts);
    rlImGuiBeginInitImGui();

    ImGui::GetIO().IniFilename = nullptr;
    theme::apply();

    ImGui::GetStyle().FontSizeBase = 20.0f;

    rlImGuiEndInitImGui();

    std::string problem;

    while (!WindowShouldClose()) {
        const std::string target = welcome(problem.empty() ? nullptr : problem.c_str());
        if (target.empty())
            break;

        // Scoped so the document's texture is released while the GL context still exists.
        ImageDocument document;

        try {
            document.open(target);
            problem.clear();
        } catch (const std::exception &e) {
            ERROR("%s", e.what());
            problem = e.what();
            continue;
        }

        run(document);
    }

    rlImGuiShutdown();
    CloseWindow();

    return 0;
}
