#ifndef CHECKER_UTILS_TO_VECTOR_H
#define CHECKER_UTILS_TO_VECTOR_H

#include <ranges>
#include <vector>

namespace checker::utils {

struct ToVector {};

inline constexpr auto operator|(std::ranges::range auto r, ToVector) {
  return std::vector(std::ranges::begin(r), std::ranges::end(r));
}

inline constexpr ToVector to_vector;

}  // namespace checker::utils

#endif  // CHECKER_UTILS_TO_VECTOR_H
