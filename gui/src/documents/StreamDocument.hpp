#pragma once

#include "components/Preview.hpp"
#include "components/StreamPane.hpp"
#include "components/TextPopout.hpp"
#include "documents/FileDocument.hpp"
#include "model/Streams.hpp"
#include <string>

class StreamDocument : public FileDocument {
  public:
    void render(void) override;
    std::string summary(void) override;

  protected:
    Streams streams;
    Preview preview;

    void handle_shortcuts(void) override;
    void edit_menu(void) override;
    void extra_menus(void) override;

    virtual const char *media_menu(void) const = 0;
    virtual void draw_properties(void) = 0;
    virtual void draw_preview_controls(void) {};

    size_t file_size(void) const;

  private:
    bool show_preview = true;

    StreamPane streams_pane;
    TextPopout text;

    const struct Stream *active_stream(void) const;
};
