#include "app/theme.hpp"
#include "app/widgets.hpp"
#include "documents/ImageDocument.hpp"
#include "documents/VideoDocument.hpp"
#include "raylib.h"
#include "welcome.hpp"
#include "window.hpp"
#include <algorithm>
#include <cfloat>
#include <imgui.h>
#include <memory>
#include <portable-file-dialogs.h>
#include <rlImGui.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <veil/fs/file.h>
#include <veil/log.h>

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

static std::unique_ptr<FileDocument> document_for(const std::string &target) {
    if (is_video_file(get_file_type(target.c_str())))
        return std::make_unique<VideoDocument>();

    return std::make_unique<ImageDocument>();
}

static void run(FileDocument &document) {
    while (!WindowShouldClose() && !document.exit) {
        begin_frame();

        document.draw_navbar();

        fill_viewport();
        ImGui::Begin("##workspace", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground);

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
        const std::unique_ptr<FileDocument> document = document_for(target);

        try {
            document->open(target);
            problem.clear();
        } catch (const std::exception &e) {
            ERROR("%s", e.what());
            problem = e.what();
            continue;
        }

        run(*document);
    }

    rlImGuiShutdown();
    CloseWindow();

    return 0;
}
