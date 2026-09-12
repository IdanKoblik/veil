#pragma once

#include "model/Matches.hpp"
#include <cstddef>
#include <vector>
#include <veil/analysis/stream.h>

class Find {
public:
    static constexpr size_t limit = 4096;

    bool draw(const struct Stream &stream, bool *open);
    void close(void);

    const Matches &matches(void) const {
        return this->found;
    };

    size_t offset(void) const;

    size_t length(void) const {
        return this->found.len;
    };

private:
    enum Mode { MODE_TEXT, MODE_HEX };

    char query[128] = "";
    int mode = MODE_TEXT;

    bool visible = false;
    bool focus = false;
    bool stale = false;
    bool capped = false;

    const unsigned char *searched = nullptr;
    size_t searched_len = 0;

    Matches found;

    std::vector<unsigned char> needle(void) const;

    void run(const struct Stream &stream);
    bool step(long delta);
};
