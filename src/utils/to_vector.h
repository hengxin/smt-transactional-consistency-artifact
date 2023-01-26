#ifndef CHECKER_UTILS_TO_VECTOR_H
#define CHECKER_UTILS_TO_VECTOR_H

#include <ranges>
#include <vector>

namespace checker::utils {

struct ToVector {};

inline constexpr auto operator|(std::ranges::range auto r, ToVector)
    -> std::vector<std::ranges::range_value_t<decltype(r)>> {
  return std::vector(std::ranges::begin(r), std::ranges::end(r));
}

inline constexpr auto to_vector = ToVector{};

}  // namespace checker::utils

#endif  // CHECKER_UTILS_TO_VECTOR_H
