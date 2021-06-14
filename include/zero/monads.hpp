#ifndef ZERO_MONADS_HPP
#define ZERO_MONADS_HPP


#include <type_traits>
#include <concepts>
#include <utility>
#include <ranges>
#include <functional>
#include <coroutine>

#include <iostream>


#define MONADS ::zero::monads::
#define CORO ::std::


namespace zero::monads {


namespace detail {

template <typename Ma>
struct unwrapped {};

template <template <typename> typename M, typename A>
struct unwrapped<M<A>> {
    using type = A;
};

template <template <typename> typename M, typename A>
struct unwrapped<M<A> const> {
    using type = A const;
};

template <template <typename> typename M, typename A>
struct unwrapped<M<A> volatile> {
    using type = A volatile;
};

template <template <typename> typename M, typename A>
struct unwrapped<M<A> const volatile> {
    using type = A const volatile;
};

} // namespace detail

// TODO: 是否应将非引用视为右值引用？
// 大部分元函数将非引用视为右值引用，因此这里大概不需要。
template <typename Ma>
using unwrapped_t = std::conditional_t<
    std::is_reference_v<Ma>,
    std::conditional_t<std::is_lvalue_reference_v<Ma>,
                       std::add_lvalue_reference_t<typename detail::unwrapped<
                           std::remove_reference_t<Ma>>::type>,
                       std::add_rvalue_reference_t<typename detail::unwrapped<
                           std::remove_reference_t<Ma>>::type>>,
    typename detail::unwrapped<std::remove_reference_t<Ma>>::type>;


namespace detail {

template <typename Ma, typename Mb>
struct is_same_wrapper : std::false_type {};

template <template <typename> typename M, typename A, typename B>
struct is_same_wrapper<M<A>, M<B>> : std::true_type {};

} // namespace detail

template <typename Ma, typename Mb>
inline constexpr bool is_same_wrapper_v =
    detail::is_same_wrapper<std::remove_cv_t<Ma>, std::remove_cv_t<Mb>>::value;

template <typename Ma, typename Mb>
concept same_wrapper_as = is_same_wrapper_v<Ma, Mb>;


template <typename Fn, typename Ma>
concept unwrapped_invocable = requires(Fn fn, unwrapped_t<Ma> a) {
    {
        std::invoke(std::FWD(fn), std::FWD(a))
        } -> same_wrapper_as<std::remove_reference_t<Ma>>;
};


template <typename Fn, typename Ma>
    requires unwrapped_invocable<Fn, Ma>
using unwrapped_invoke_result_t =
    std::remove_reference_t<std::invoke_result_t<Fn, unwrapped_t<Ma>>>;


namespace detail {

// 提供一个稍微比 std::bind(auto&&, auto&&...) 更特殊一点的重载以防止意外匹配，
// 但 bind_fn 可以通过 constraints 约束返回类型，提供此重载反而可能隐藏某些逻辑
// BUG 。
// void bind(auto&&, auto&&) = delete;

// views have been treated as lvalues, because they are non-owning ranges.
template <typename Rng, typename T>
using maybe_move_t = std::conditional_t<
    std::is_lvalue_reference_v<Rng> || std::ranges::view<Rng>,
    std::remove_reference_t<T>&, std::remove_reference_t<T>&&>;

template <std::ranges::sized_range Rng, unwrapped_invocable<Rng> Fn>
auto bind(Rng&& rng, Fn&& fn) -> unwrapped_invoke_result_t<Fn, Rng> {
    unwrapped_invoke_result_t<Fn, Rng> res;
    for (auto&& vals : std::FWD(rng)) {
        for (auto&& val : std::invoke(
                 std::FWD(fn),
                 std::forward<maybe_move_t<Rng, decltype(vals)>>(vals))) {
            res.emplace_back(std::move(val));
        }
    }
    return res;
}

template <instantiation_of<std::optional> Opt, unwrapped_invocable<Opt> Fn>
auto bind(Opt&& opt, Fn&& fn) {
    return opt ? std::invoke(std::FWD(fn), *std::FWD(opt)) : std::nullopt;
}

// 不使用 std::forward 会导致某些情况下 constraints 检查通过但实际 dispatch
// 到 std::bind，因此要保证 constraints 和 call 行为的一致性。
// same_wrapper_as forbid return by references, see bind_fn::operator()
// below.
template <typename Ma, typename Fn>
concept has_adl = requires(Ma ma, Fn fn) {
    //{
    bind(std::FWD(ma), std::FWD(fn));
    //} -> same_wrapper_as<std::remove_reference_t<Ma>>;
};

class bind_fn {
    enum class strategy { none, member, non_member };

    template <typename Ma, typename Fn>
    static consteval strategy choose() {
        if constexpr (has_adl<Ma, Fn>) {
            return strategy::non_member;
        } else {
            return strategy::none;
        }
    }

    template <typename Ma, typename Fn>
    static constexpr strategy choice = choose<Ma, Fn>();

public:
    // 引用和 monad 的性质有不可调和的矛盾，所以不能是 decltype(auto) 。
    template <typename Ma, typename Fn>
        requires(choice<Ma, Fn> != strategy::none)
    constexpr auto operator()(Ma&& ma, Fn&& fn) const {
        return bind(std::FWD(ma), std::FWD(fn));
    }
};

} // namespace detail

inline constexpr detail::bind_fn bind;


template <template <typename...> typename M, typename A>
auto return_(A&& a) -> M<A> {
    return {std::FWD(a)};
}


// coroutine supports
namespace detail {


// std::vector<std::tuple<int, int>> foo() {
//     auto const x = co_await std::vector{1, 2, 3};
//     auto const y = co_await std::vector{4, 5, 6};
//     co_return std::tuple{x, y};
// }


template <template <typename> typename M, typename B>
struct promise {
    using handle_t = CORO coroutine_handle<promise>;

    struct cowrapped {
        handle_t coro_;
        operator M<B>() /*const*/ /*noexcept*/ {
            auto const mb = std::move(coro_.promise().result);
            coro_.destroy();
            return mb;
        }
    };

    M<B> result;

    cowrapped get_return_object() /*noexcept*/ {
        return {handle_t::from_promise(*this)};
    }

    template <typename R> /*noexcept*/
        requires std::same_as<std::remove_reference_t<R>, B>
    void return_value(R&& r) { result = MONADS return_<M>(std::FWD(r)); }

    void unhandled_exception() /*noexcept*/ {}

    CORO suspend_never initial_suspend() noexcept { return {}; }
    CORO suspend_always final_suspend() noexcept { return {}; }
};


template <typename Ma>
operator co_await(Ma&& ma) {
    struct awaiter {
        bool await_ready() noexcept { return true; }
        auto await_resume() /*noexcept*/ { return MONADS bind(std::FWD(ma), ); }
    };
}


} // namespace detail


} // namespace zero::monads


template <template <typename> typename M, typename B>
struct std::coroutine_traits<M<B>> {
    using promise_type = MONADS detail::promise<M, B>;
};


#endif
