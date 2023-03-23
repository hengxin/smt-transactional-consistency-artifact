#ifndef CHECKER_UTILS_TO_VECTOR_H
#define CHECKER_UTILS_TO_VECTOR_H

#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <utility>

namespace checker::utils {

struct ToVector {};
struct ToUnorderedMap {};
struct ToUnorderedSet {};

template <typename Container>
struct To {};

inline constexpr auto operator|(std::ranges::range auto r, ToVector)
    -> std::vector<std::ranges::range_value_t<decltype(r)>> {
  return std::vector(std::ranges::begin(r), std::ranges::end(r));
}

inline constexpr auto operator|(std::ranges::range auto r, ToUnorderedMap)
    -> std::unordered_map<
        typename std::ranges::range_value_t<decltype(r)>::first_type,
        typename std::ranges::range_value_t<decltype(r)>::second_type> {
  auto m = std::unordered_map<
      typename std::ranges::range_value_t<decltype(r)>::first_type,
      typename std::ranges::range_value_t<decltype(r)>::second_type>{};

  for (auto &&[first, second] : r) {
    m.emplace(first, second);
  }
  return m;
}

inline constexpr auto operator|(std::ranges::range auto r, ToUnorderedSet)
    -> std::unordered_set<std::ranges::range_value_t<decltype(r)>> {
  return std::unordered_set(std::ranges::begin(r), std::ranges::end(r));
}

template <typename C>
inline constexpr auto operator|(std::ranges::range auto r, To<C>) -> C {
  auto c = C{};

  for (auto &&e : r) {
    c.emplace(e);
  }

  return c;
}

inline constexpr auto to_vector = ToVector{};
inline constexpr auto to_unordered_map = ToUnorderedMap{};
inline constexpr auto to_unordered_set = ToUnorderedSet{};

template <typename C>
inline constexpr auto to = To<C>{};

}  // namespace checker::utils

#endif  // CHECKER_UTILS_TO_VECTOR_H
