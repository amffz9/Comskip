#pragma once
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>

namespace comskip::detection {
// Capacity records the usable entries; spare entries support legacy lookahead.
// Commit it only after successful allocation so an exception preserves state.
template<class T, class Allocator>
bool grow_buffer(std::vector<T, Allocator>& buffer, long& capacity, long index,
                 long increment, long spare = 1)
{
    if (index < 0 || capacity < 0 || increment <= 0 || spare < 0)
        throw std::out_of_range("Invalid detection buffer index or capacity");
    using Size = typename std::vector<T, Allocator>::size_type;
    const Size requested = static_cast<Size>(index) + 1;
    const Size old_capacity = static_cast<Size>(capacity);
    const Size step = static_cast<Size>(increment);
    Size next_capacity = old_capacity;
    if (requested > next_capacity) {
        const Size difference = requested - next_capacity;
        const Size steps = difference / step + (difference % step != 0);
        const Size limit = std::min(buffer.max_size(),
            static_cast<Size>(std::numeric_limits<long>::max()));
        if (next_capacity > limit || steps > (limit - next_capacity) / step)
            throw std::length_error("Detection buffer capacity exceeds supported size");
        next_capacity += steps * step;
    }
    if (static_cast<Size>(spare) > buffer.max_size()
        || next_capacity > buffer.max_size() - static_cast<Size>(spare))
        throw std::length_error("Detection buffer capacity exceeds supported size");
    const Size next_size = next_capacity + static_cast<Size>(spare);
    const bool changed = buffer.size() < next_size;
    if (changed)
        buffer.resize(next_size);
    capacity = static_cast<long>(next_capacity);
    return changed;
}
}
