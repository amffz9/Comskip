#pragma once
#include "detector_records.h"
#include <limits>
#include <stdexcept>
#include <vector>

namespace comskip::detection {
inline block_info empty_block() {
    block_info block{};
    block.score = 1.0;
    return block;
}

// The count excludes the initialized terminal entry used by boundary scoring.
inline void reset_blocks(std::vector<block_info>& blocks, long& count) {
    blocks.resize(1);
    blocks.front() = empty_block();
    count = 0;
}

inline void complete_block(std::vector<block_info>& blocks, long& count) {
    if (count < 0 || count == std::numeric_limits<long>::max() ||
        static_cast<std::size_t>(count) + 1 != blocks.size())
        throw std::out_of_range("Invalid completed detection block count");
    blocks.push_back(empty_block());
    ++count;
}

inline void erase_blocks(std::vector<block_info>& blocks, long& count,
                         long first, long removed = 1) {
    if (count < 0 || first < 0 || removed < 0 || first > count || removed > count - first ||
        static_cast<std::size_t>(count) + 1 != blocks.size())
        throw std::out_of_range("Invalid detection block removal");
    blocks.erase(blocks.begin() + first, blocks.begin() + first + removed);
    count -= removed;
}
}
