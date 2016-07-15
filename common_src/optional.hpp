#ifndef OPTIONAL_HPP
#define OPTIONAL_HPP

// std::optional is planned for C++17.
// In the meantime: use either boost::optional or std::experimental::optional.

#include <experimental/optional>

template<typename... Ts>
using optional = std::experimental::optional<Ts...>;
using nullopt_t = std::experimental::nullopt_t;
using in_place_t = std::experimental::in_place_t;

constexpr nullopt_t nullopt = std::experimental::nullopt;
constexpr in_place_t in_place = std::experimental::in_place;

#endif
