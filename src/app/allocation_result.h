#pragma once
#include <expected>
#include <functional>
#include <new>
#include <utility>

namespace comskip {
enum class AllocationError { insufficient_memory };
template <class Operation>
[[nodiscard]] std::expected<void,AllocationError> attempt_allocation(Operation&& operation) {
    try {
        std::invoke(std::forward<Operation>(operation));
        return {};
    } catch (const std::bad_alloc&) {
        return std::unexpected(AllocationError::insufficient_memory);
    }
}
}
