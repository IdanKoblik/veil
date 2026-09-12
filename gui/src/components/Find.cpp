#include "components/Find.hpp"
#include "app/theme.hpp"

#include <cstdio>
#include <cstring>
#include <imgui.h>

static int hex_value(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';

    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;

    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return -1;
}

std::vector<unsigned char> Find::needle(void) const {
    std::vector<unsigned char> bytes;

    if (this->mode == MODE_TEXT) {
        for (const char *c = this->query; *c; c++)
            bytes.push_back(static_cast<unsigned char>(*c));

        return bytes;
    }

    unsigned char value = 0;
    int digits = 0;

    for (const char *c = this->query; *c; c++) {
        if (*c == ' ')
            continue;

        const int digit = hex_value(*c);
        if (digit < 0)
            return {};

        value = static_cast<unsigned char>(value << 4 | digit);
        if (++digits < 2)
            continue;

        bytes.push_back(value);
        value = 0;
        digits = 0;
    }

    return digits ? std::vector<unsigned char>() : bytes;
}

void Find::run(const struct Stream &stream) {
    this->found.offsets.clear();
    this->found.current = 0;
    this->capped = false;
    this->stale = false;

    this->searched = stream.bytes;
    this->searched_len = stream.len;

    const std::vector<unsigned char> needle = this->needle();

    this->found.len = needle.size();
    if (needle.empty() || !stream.bytes || stream.len < needle.size())
        return;

    const size_t last = stream.len - needle.size();

    for (size_t at = 0; at <= last; at++) {
        const unsigned char *hit = static_cast<const unsigned char *>(std::memchr(stream.bytes + at, needle[0], last - at + 1));
        if (!hit)
            break;

        at = static_cast<size_t>(hit - stream.bytes);
        if (std::memcmp(hit, needle.data(), needle.size()) != 0)
            continue;

        this->found.offsets.push_back(at);
        if (this->found.offsets.size() == limit) {
            this->capped = true;
            break;
        }
    }
}

bool Find::step(long delta) {
    const size_t count = this->found.offsets.size();
    if (count == 0)
        return false;

    const size_t forward = static_cast<size_t>(delta < 0 ? count - 1 : 1);
    this->found.current = (this->found.current + forward) % count;

    return true;
}

size_t Find::offset(void) const {
    if (this->found.current >= this->found.offsets.size())
        return 0;

    return this->found.offsets[this->found.current];
}

void Find::close(void) {
    this->found.offsets.clear();
    this->found.current = 0;
    this->found.len = 0;

    this->searched = nullptr;
    this->searched_len = 0;

    this->visible = false;
}

bool Find::draw(const struct Stream &stream, bool *open) {
    if (!this->visible) {
        this->visible = true;
        this->focus = true;
        this->stale = true;
    }

    bool jump = false;

    if (this->focus) {
        ImGui::SetKeyboardFocusHere();
        this->focus = false;
    }

    ImGui::SetNextItemWidth(240.0f);
    if (ImGui::InputText("##query", this->query, sizeof(this->query), ImGuiInputTextFlags_EnterReturnsTrue))
        jump = this->step(ImGui::GetIO().KeyShift ? -1 : 1);

    if (jump)
        ImGui::SetKeyboardFocusHere(-1);

    if (ImGui::IsItemEdited())
        this->stale = true;

    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f);
    if (ImGui::Combo("##mode", &this->mode, "text\0hex\0"))
        this->stale = true;

    if (this->stale || stream.bytes != this->searched || stream.len != this->searched_len) {
        this->run(stream);
        jump = !this->found.offsets.empty();
    }

    ImGui::SameLine();
    if (ImGui::ArrowButton("##prev", ImGuiDir_Up))
        jump = this->step(-1);

    ImGui::SameLine();
    if (ImGui::ArrowButton("##next", ImGuiDir_Down))
        jump = this->step(1);

    ImGui::SameLine();

    char label[64] = "";
    if (!this->found.offsets.empty())
        std::snprintf(label, sizeof(label), "%zu / %zu%s", this->found.current + 1, this->found.offsets.size(), this->capped ? "+" : "");
    else if (this->query[0])
        std::snprintf(label, sizeof(label), "no match");

    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_dim);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();

    ImGui::SameLine();
    if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        this->close();
        *open = false;
        return false;
    }

    ImGui::Spacing();

    return jump && !this->found.offsets.empty();
}
