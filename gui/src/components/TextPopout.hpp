#pragma once

#include "components/Find.hpp"
#include "components/TextView.hpp"
#include <cstddef>
#include <veil/analysis/stream.h>

class TextPopout {
  public:
    void open(void);
    void draw(const struct StreamSet &set);

    bool is_open(void) const {
        return this->shown;
    };

  private:
    bool pending = false;
    bool shown = false;
    bool find_open = false;
    size_t stream = 0;

    TextView view;
    Find find;
};
