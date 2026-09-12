#pragma once

#include "components/Preview.hpp"
#include "components/StreamPane.hpp"
#include "components/TextPopout.hpp"
#include "documents/FileDocument.hpp"
#include "model/Pixels.hpp"
#include "model/Streams.hpp"
#include <string>

class ImageDocument : public FileDocument {
  public:
    void render(void) override;
    void open(const std::string &target) override;
    std::string summary(void) override;

  protected:
    void handle_shortcuts(void) override;
    void edit_menu(void) override;
    void extra_menus(void) override;

  private:
    bool show_preview = true;

    Pixels pixels;
    Streams streams;

    Preview preview;
    StreamPane streams_pane;
    TextPopout text;

    size_t file_size(void) const;
    const struct Stream *active_stream(void) const;
};
