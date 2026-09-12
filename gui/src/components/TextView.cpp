#include "components/TextView.hpp"
#include "app/theme.hpp"

#include <algorithm>

static char printable(unsigned char byte) {
    return byte >= 0x20 && byte <= 0x7E ? static_cast<char>(byte) : '.';
}

void TextView::reveal(size_t offset) {
    this->scroll_to = offset;
}

void TextView::draw_row(ImDrawList *draw, const ImVec2 &pos, float glyph, float line, const struct Stream &stream, size_t offset, const Matches &matches) {
    const size_t count = std::min(this->columns, stream.len - offset);

    char text[columns_max];
    bool hit[columns_max];
    bool current[columns_max];

    matches_row(matches, offset, count, hit, current);

    for (size_t i = 0; i < count; i++)
        text[i] = printable(stream.bytes[offset + i]);

    for (size_t i = 0; i < count; i++) {
        if (!hit[i])
            continue;

        const ImVec2 lo = ImVec2(pos.x + glyph * i, pos.y);
        const ImVec2 hi = ImVec2(lo.x + glyph, lo.y + line);

        draw->AddRectFilled(lo, hi, ImGui::GetColorU32(current[i] ? theme::match_current : theme::match));
    }

    draw->AddText(pos, ImGui::GetColorU32(theme::text_dim), text, text + count);

    for (size_t i = 0; i < count; i++) {
        if (!hit[i])
            continue;

        const char one[] = {text[i], '\0'};
        draw->AddText(ImVec2(pos.x + glyph * i, pos.y), ImGui::GetColorU32(theme::text), one);
    }
}

void TextView::draw(const struct Stream &stream, const Matches &matches) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::sunken_bg);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##recovered", ImVec2(0, 0), ImGuiChildFlags_Borders);

    if (!stream.bytes || stream.len == 0) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::text_dim);
        ImGui::TextUnformatted("The stream is empty");
        ImGui::PopStyleColor();

        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        return;
    }

    const float glyph = ImGui::CalcTextSize("0").x;
    const float line = ImGui::GetTextLineHeight();
    const float room = ImGui::GetContentRegionAvail().x;

    this->columns = std::min(columns_max, std::max(static_cast<size_t>(16), static_cast<size_t>(room / glyph)));

    const int rows = static_cast<int>((stream.len + this->columns - 1) / this->columns);

    if (this->scroll_to != nowhere) {
        ImGui::SetScrollY(static_cast<float>(this->scroll_to / this->columns) * line - ImGui::GetWindowHeight() * 0.5f);
        this->scroll_to = nowhere;
    }

    ImDrawList *draw = ImGui::GetWindowDrawList();

    ImGuiListClipper clipper;
    clipper.Begin(rows, line);

    while (clipper.Step())
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
            const ImVec2 pos = ImGui::GetCursorScreenPos();

            this->draw_row(draw, pos, glyph, line, stream, static_cast<size_t>(row) * this->columns, matches);
            ImGui::Dummy(ImVec2(room, line));
        }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}
