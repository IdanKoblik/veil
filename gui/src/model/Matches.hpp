#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

struct Matches {
    std::vector<size_t> offsets;

    size_t len = 0;     
    size_t current = 0; 
};

inline void matches_row(const Matches &matches, size_t offset, size_t count, bool *hit, bool *current) {
    std::fill(hit, hit + count, false);
    std::fill(current, current + count, false);

    if (matches.offsets.empty() || matches.len == 0)
        return;

    const size_t from = offset > matches.len ? offset - matches.len + 1 : 0;
    auto it = std::lower_bound(matches.offsets.begin(), matches.offsets.end(), from);

    for (; it != matches.offsets.end() && *it < offset + count; ++it) {
        const bool is_current = static_cast<size_t>(it - matches.offsets.begin()) == matches.current;

        for (size_t i = 0; i < count; i++) {
            if (offset + i < *it || offset + i - *it >= matches.len)
                continue;

            hit[i] = true;
            current[i] = current[i] || is_current;
        }
    }
}
