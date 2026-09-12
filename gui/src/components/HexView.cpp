#include "components/HexView.hpp"
#include "app/theme.hpp"

#include <algorithm>
#include <cstdio>

static const char hex_digits[] = "0123456789abcdef";

static bool within(size_t value, size_t start, size_t len) {
    return len != 0 && value >= start && value - start < len;
}

static size_t shifted(size_t from, long delta, size_t len) {
    if (delta < 0) {
        const size_t back = static_cast<size_t>(-delta);
        return from > back ? from - back : 0;
    }

    const size_t to = from + static_cast<size_t>(delta);

    return to < len ? to : len - 1;
}

void HexView::select(size_t offset, size_t len) {
    this->cursor = offset + (len ? len - 1 : 0);
    this->anchor = offset;
    this->scroll_to = offset;
}

size_t HexView::selection_start(void) const {
    if (this->cursor == nowhere)
        return 0;

    return std::min(this->anchor, this->cursor);
}

size_t HexView::selection_len(void) const {
    if (this->cursor == nowhere)
        return 0;

    return std::max(this->anchor, this->cursor) - this->selection_start() + 1;
}

std::string HexView::selection_hex(const struct Stream &stream) const {
    std::string out;

    if (this->cursor == nowhere || !stream.bytes)
        return out;

    const size_t start = this->selection_start();
    const size_t len = this->selection_len();

    out.reserve(len * 3);
    for (size_t i = 0; i < len && start + i < stream.len; i++) {
        const unsigned char byte = stream.bytes[start + i];

        if (i)
            out.push_back(' ');

        out.push_back(hex_digits[byte >> 4]);
        out.push_back(hex_digits[byte & 0x0F]);
    }

    return out;
}

std::string HexView::selection_label(void) const {
    if (this->cursor == nowhere)
        return std::string();

    const size_t start = this->selection_start();
    const size_t len = this->selection_len();

    char label[80];
    if (len == 1)
        std::snprintf(label, sizeof(label), "0x%08zx", start);
    else
        std::snprintf(label, sizeof(label), "0x%08zx .. 0x%08zx, %zu bytes", start, start + len - 1, len);

    return std::string(label);
}

void HexView::move_cursor(size_t to, bool extend, size_t len) {
    if (len == 0)
        return;

    this->cursor = to < len ? to : len - 1;
    if (!extend || this->anchor == nowhere)
        this->anchor = this->cursor;

    this->follow = true;
}

size_t HexView::byte_at(const ImVec2 &top, float glyph, float line, const ImVec2 &mouse, size_t len) const {
    const float y = mouse.y - top.y;
    if (y < 0.0f)
        return nowhere;

    const float x = mouse.x - top.x;
    size_t col = columns;

    if (x >= hex_x(glyph) && x < byte_x(glyph, columns - 1) + glyph * 3) {
        /* The space at the halves is not a column of its own. */
        const float rel = x - hex_x(glyph) - (x < byte_x(glyph, columns / 2) ? 0.0f : glyph);
        col = static_cast<size_t>(rel / (glyph * 3));
    } else if (x >= ascii_x(glyph) && x < ascii_x(glyph) + glyph * columns) {
        col = static_cast<size_t>((x - ascii_x(glyph)) / glyph);
    }

    if (col >= columns)
        return nowhere;

    const size_t offset = static_cast<size_t>(y / line) * columns + col;

    return offset < len ? offset : nowhere;
}

void HexView::handle_mouse(const ImVec2 &top, float glyph, float line, size_t len) {
    const ImGuiIO &io = ImGui::GetIO();

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const size_t byte = this->byte_at(top, glyph, line, io.MousePos, len);

        if (byte != nowhere) {
            ImGui::SetWindowFocus();
            this->move_cursor(byte, io.KeyShift, len);
            this->follow = false;
            this->dragging = true;
        }
    }

    if (!this->dragging)
        return;

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        this->dragging = false;
        return;
    }

    const size_t byte = this->byte_at(top, glyph, line, io.MousePos, len);
    if (byte != nowhere)
        this->cursor = byte;

    /* Drag past an edge and the view follows. */
    const float top_y = ImGui::GetWindowPos().y;
    const float bottom_y = top_y + ImGui::GetWindowHeight();

    if (io.MousePos.y < top_y)
        ImGui::SetScrollY(ImGui::GetScrollY() - line);
    else if (io.MousePos.y > bottom_y)
        ImGui::SetScrollY(ImGui::GetScrollY() + line);
}

void HexView::handle_keys(const struct Stream &stream) {
    const ImGuiIO &io = ImGui::GetIO();

    /* The find box is typed into, so it keeps the keys while it has them. */
    if (io.WantTextInput)
        return;

    if (!ImGui::IsWindowFocused() && !ImGui::IsWindowHovered())
        return;

    const bool extend = io.KeyShift;
    const size_t len = stream.len;

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C) && this->cursor != nowhere)
        ImGui::SetClipboardText(this->selection_hex(stream).c_str());

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
        this->anchor = 0;
        this->cursor = len - 1;
        return;
    }

    if (this->cursor == nowhere) {
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_RightArrow))
            this->move_cursor(0, false, len);

        return;
    }

    const size_t page = static_cast<size_t>(ImGui::GetWindowHeight() / ImGui::GetTextLineHeight()) * columns;

    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
        this->move_cursor(shifted(this->cursor, -1, len), extend, len);
    else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
        this->move_cursor(shifted(this->cursor, 1, len), extend, len);
    else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
        this->move_cursor(shifted(this->cursor, -static_cast<long>(columns), len), extend, len);
    else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
        this->move_cursor(shifted(this->cursor, static_cast<long>(columns), len), extend, len);
    else if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
        this->move_cursor(shifted(this->cursor, -static_cast<long>(page), len), extend, len);
    else if (ImGui::IsKeyPressed(ImGuiKey_PageDown))
        this->move_cursor(shifted(this->cursor, static_cast<long>(page), len), extend, len);
    else if (ImGui::IsKeyPressed(ImGuiKey_Home))
        this->move_cursor(io.KeyCtrl ? 0 : this->cursor - this->cursor % columns, extend, len);
    else if (ImGui::IsKeyPressed(ImGuiKey_End))
        this->move_cursor(io.KeyCtrl ? len - 1 : this->cursor - this->cursor % columns + columns - 1, extend, len);
}

void HexView::draw_row(ImDrawList *draw, const ImVec2 &pos, float glyph, float line, const struct Stream &stream, size_t offset, const Matches &matches) {
    char text[offset_digits + 1];

    std::snprintf(text, sizeof(text), "%08zx", offset);
    draw->AddText(pos, ImGui::GetColorU32(theme::text_faint), text);

    bool hit[columns];
    bool current[columns];
    matches_row(matches, offset, columns, hit, current);

    const size_t start = this->selection_start();
    const size_t selected = this->selection_len();

    for (size_t col = 0; col < columns && offset + col < stream.len; col++) {
        const unsigned char byte = stream.bytes[offset + col];
        const ImU32 color = ImGui::GetColorU32(theme::byte_color(byte));

        const ImVec2 hex_at = ImVec2(pos.x + byte_x(glyph, col), pos.y);
        const ImVec2 ascii_at = ImVec2(pos.x + ascii_x(glyph) + glyph * col, pos.y);

        if (hit[col]) {
            const ImU32 tint = ImGui::GetColorU32(current[col] ? theme::match_current : theme::match);

            draw->AddRectFilled(hex_at, ImVec2(hex_at.x + glyph * 2, hex_at.y + line), tint);
            draw->AddRectFilled(ascii_at, ImVec2(ascii_at.x + glyph, ascii_at.y + line), tint);
        }

        if (within(offset + col, start, selected)) {
            const ImU32 tint = ImGui::GetColorU32(theme::accent_soft);

            draw->AddRectFilled(hex_at, ImVec2(hex_at.x + glyph * 2, hex_at.y + line), tint);
            draw->AddRectFilled(ascii_at, ImVec2(ascii_at.x + glyph, ascii_at.y + line), tint);
        }

        if (offset + col == this->cursor)
            draw->AddRect(hex_at, ImVec2(hex_at.x + glyph * 2, hex_at.y + line), ImGui::GetColorU32(theme::accent));

        const char pair[] = {hex_digits[byte >> 4], hex_digits[byte & 0x0F], '\0'};
        draw->AddText(hex_at, color, pair);

        const char printable[] = {byte >= 0x20 && byte <= 0x7E ? static_cast<char>(byte) : '.', '\0'};
        draw->AddText(ascii_at, color, printable);
    }
}

void HexView::draw(const struct Stream &stream, const Matches &matches) {
    if (!stream.bytes || stream.len == 0) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::text_dim);
        ImGui::TextUnformatted("The stream is empty");
        ImGui::PopStyleColor();
        return;
    }

    if (this->cursor != nowhere && (this->cursor >= stream.len || this->anchor >= stream.len)) {
        this->cursor = nowhere;
        this->anchor = nowhere;
    }

    const float glyph = ImGui::CalcTextSize("0").x;
    const float line = ImGui::GetTextLineHeight();
    const int rows = static_cast<int>((stream.len + columns - 1) / columns);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##hex", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoNavInputs);

    const ImVec2 top = ImGui::GetCursorScreenPos();
    ImDrawList *draw = ImGui::GetWindowDrawList();

    ImGuiListClipper clipper;
    clipper.Begin(rows, line);

    while (clipper.Step())
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
            const ImVec2 pos = ImGui::GetCursorScreenPos();

            this->draw_row(draw, pos, glyph, line, stream, static_cast<size_t>(row) * columns, matches);
            ImGui::Dummy(ImVec2(row_w(glyph), line));
        }

    this->handle_mouse(top, glyph, line, stream.len);
    this->handle_keys(stream);

    const float height = ImGui::GetWindowHeight();
    const float scroll = ImGui::GetScrollY();

    if (this->scroll_to != nowhere) {
        ImGui::SetScrollY(static_cast<float>(this->scroll_to / columns) * line - height * 0.5f);
        this->scroll_to = nowhere;
        this->follow = false;
    } else if (this->follow && this->cursor != nowhere) {
        const float y = static_cast<float>(this->cursor / columns) * line;

        if (y < scroll)
            ImGui::SetScrollY(y);
        else if (y + line > scroll + height)
            ImGui::SetScrollY(y + line - height);

        this->follow = false;
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}
