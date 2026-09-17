#pragma once

#include "documents/StreamDocument.hpp"
#include "model/Video.hpp"
#include <string>

class VideoDocument : public StreamDocument {
  public:
    void open(const std::string &target) override;

  protected:
    const char *media_menu(void) const override {
        return "Video";
    };

    void draw_properties(void) override;
    void draw_preview_controls(void) override;

  private:
    Video video;
    int frame = 0;

    void show_frame(int index);
};
