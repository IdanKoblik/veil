#pragma once

#include "documents/Document.hpp"
#include <string>
#include <veil/fs/file.h>

class FileDocument : public Document {
public:
    std::string file_name_of(const std::string &path) {
        const size_t slash = path.find_last_of('/');
        return slash == std::string::npos ? path : path.substr(slash + 1);
    }

    virtual void open(const std::string &path) = 0;
protected: 
    std::string path;
    std::string name;
    enum FileType file_type;

    void file_menu(void) override {
        ImGui::Separator();
        if (ImGui::MenuItem("Copy path", nullptr, false, !this->path.empty()))
            ImGui::SetClipboardText(this->path.c_str());
    };

    void navbar_center(void) override {
        if (!this->name.empty())
            ui::centered_label(this->name.c_str());
    };
};
