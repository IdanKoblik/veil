#pragma once

#include "app/ViewState.hpp"
#include "components/Find.hpp"
#include "components/HexView.hpp"
#include <string>
#include <veil/analysis/stream.h>

class StreamPane {
  public:
    void draw(const struct StreamSet &set, ViewState &state);

    bool has_selection(void) const {
        return this->hex.has_selection();
    };

    std::string selection_label(void) const {
        return this->hex.selection_label();
    };

    std::string selection_hex(const struct Stream &stream) const {
        return this->hex.selection_hex(stream);
    };

  private:
    HexView hex;
    Find find;
};
