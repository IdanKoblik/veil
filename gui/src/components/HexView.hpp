#pragma once

#include "model/Matches.hpp"
#include <imgui.h>
#include <string>
#include <veil/analysis/stream.h>

class HexView {
  public:
    static constexpr size_t columns = 16;
    static constexpr size_t nowhere = static_cast<size_t>(-1);

    void draw(const struct Stream &stream, const Matches &matches);

    void select(size_t offset, size_t len);

    bool has_selection(void) const {
        return this->cursor != nowhere;
    };

    size_t selection_start(void) const;
    size_t selection_len(void) const;

    std::string selection_hex(const struct Stream &stream) const;

    std::string selection_label(void) const;

  private:
    static constexpr size_t offset_digits = 8;

    size_t cursor = nowhere;
    size_t anchor = nowhere;
    size_t scroll_to = nowhere;

    bool dragging = false;
    bool follow = false;

    static float hex_x(float glyph) {
        return glyph * (offset_digits + 2);
    };

    static float ascii_x(float glyph) {
        return hex_x(glyph) + glyph * (columns * 3 + 2);
    };

    static float row_w(float glyph) {
        return ascii_x(glyph) + glyph * columns;
    };

    static float byte_x(float glyph, size_t col) {
        return hex_x(glyph) + glyph * col * 3 + (col < columns / 2 ? 0.0f : glyph);
    };

    size_t byte_at(const ImVec2 &top, float glyph, float line, const ImVec2 &mouse, size_t len) const;

    void draw_row(ImDrawList *draw, const ImVec2 &pos, float glyph, float line, const struct Stream &stream, size_t offset, const Matches &matches);

    void handle_mouse(const ImVec2 &top, float glyph, float line, size_t len);
    void handle_keys(const struct Stream &stream);

    void move_cursor(size_t to, bool extend, size_t len);
};
