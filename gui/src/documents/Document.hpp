#pragma once

#include "app/ViewState.hpp"
#include "app/math.hpp"
#include "app/widgets.hpp"
#include "components/AboutPopout.hpp"
#include "components/StatusBar.hpp"
#include <imgui.h>
#include <string>

class Document {
public:
    Document() {
        this->state = ViewState();
    };

    virtual ~Document() = default;
    virtual void render(void) = 0;
    virtual std::string summary(void) = 0;

    bool exit = false;

    void draw_navbar(void) {
        this->handle_shortcuts();

        if (!ImGui::BeginMainMenuBar())
            return;

        if (ImGui::BeginMenu("File")) {
            ImGui::MenuItem("Open...", "Ctrl+O");
            if (ImGui::MenuItem("Close", "Ctrl+W"))
                this->exit = true;
            this->file_menu();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            ImGui::MenuItem("Find", "Ctrl+F", &this->state.find_open);
            ImGui::Separator();

            if (ImGui::MenuItem("Zoom in", "Ctrl++"))
                this->zoom_by(ViewState::zoom_step);

            if (ImGui::MenuItem("Zoom out", "Ctrl+-"))
                this->zoom_by(1.0f / ViewState::zoom_step);

            if (ImGui::MenuItem("Reset zoom", "Ctrl+0"))
                this->state.zoom = 1.0f;

            this->edit_menu();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About Veil"))
                this->about.open();

            ImGui::EndMenu();
        }

        this->about.draw();

        this->extra_menus();
        this->navbar_center();

        ImGui::EndMainMenuBar();
    };

    void draw_status_bar(void) {
        this->status.draw(this->summary());
    };

protected:
    ViewState state;

    virtual void handle_shortcuts(void) {
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_F))
            this->state.find_open = !this->state.find_open;
    };

    virtual void file_menu(void) {};
    virtual void edit_menu(void) {};
    virtual void extra_menus(void) {};
    virtual void navbar_center(void) {};

    inline void zoom_by(float factor) {
        this->state.zoom = clampf(this->state.zoom * factor, ViewState::zoom_min, ViewState::zoom_max);
    };

private:
    AboutPopout about;
    StatusBar status;
};
