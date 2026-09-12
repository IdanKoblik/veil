#pragma once

#include "model/Matches.hpp"
#include <cstddef>
#include <imgui.h>
#include <veil/analysis/stream.h>

class TextView {
  public:
    static constexpr size_t columns_max = 512;
    static constexpr size_t nowhere = static_cast<size_t>(-1);

    void draw(const struct Stream &stream, const Matches &matches);
    void reveal(size_t offset);

  private:
    size_t scroll_to = nowhere;
    size_t columns = 64;

    void draw_row(ImDrawList *draw, const ImVec2 &pos, float glyph, float line, const struct Stream &stream, size_t offset, const Matches &matches);
};
