#include <zero/utility.hpp>
#include <zero/monads.hpp>
#include <type_traits>
#include <utility>
#include <ranges>
#include <functional>


namespace rng = std::ranges;
namespace monads = zero::monads;


#include <vector>
#include <string>
#include <optional>
#include <algorithm>
#include <iostream>
#include <future>


std::vector<std::tuple<int, int>> foo() {
    auto const x = co_await std::vector{1, 2, 3};
    co_return std::tuple{x, 4};
}


int main() {
    rng::copy(foo(), std::ostream_iterator<int>{std::cout, " "});
    std::cout << "\n";

    std::vector v1{1, 2, 3};

    auto v2 = monads::bind(std::move(v1), [](int&& x) {
        return monads::bind(std::vector{4, 5, 6}, [x](int y) {
            return monads::return_<std::vector>(std::tuple{x, y});
        });
    });
    for (auto&& tp : v2) {
        std::printf("(%d,%d), ", std::get<0>(tp), std::get<1>(tp));
    }
    std::cout << "\n";

    std::optional const opt{42};
    auto opt2 = monads::bind(opt, [](int const& i) {
        return monads::return_<std::optional>(std::to_string(i));
    });
    std::cout << *opt2 << "\n";

    auto opt3 = monads::bind(
        std::optional{10086}, [](int&& i) -> std::optional<std::string> {
            return monads::return_<std::optional>(std::to_string(i));
        });
    std::cout << *opt3 << "\n";

    std::vector<std::vector<int>> const v{{1, 2, 3, 4}, {5, 6, 7}, {8, 9}};
    rng::copy(monads::bind(v, zero::id),
              std::ostream_iterator<int>{std::cout, " "});
}
