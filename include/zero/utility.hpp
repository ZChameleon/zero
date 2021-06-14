#ifndef ZERO_UTILITY_HPP
#define ZERO_UTILITY_HPP


#include <type_traits>
#include <utility>


#define FWD(x) forward<decltype(x)>(x)


namespace zero {


namespace detail {

template <typename T, template <typename...> typename Tp>
struct is_instantiation_of : std::false_type {};

template <template <typename...> typename Tp, typename... Ts>
struct is_instantiation_of<Tp<Ts...>, Tp> : std::true_type {};

} // namespace detail

template <typename T, template <typename...> typename Tp>
inline constexpr bool is_instantiation_of_v =
    detail::is_instantiation_of<std::remove_cvref_t<T>, Tp>::value;

template <typename T, template <typename...> typename Tp>
concept instantiation_of = is_instantiation_of_v<T, Tp>;


// like std::identity but return by value
inline constexpr auto id = [](auto&& val) { return std::FWD(val); };


} // namespace zero


#endif
