#ifndef COMMON_HPP
#define COMMON_HPP

#include <cstdio>
#include <cstdarg>
#include <cstring>

#include <stdexcept>
#include <string_view>
#include <memory>
#include <span>
#include <array>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>

#include <ranges>
#include <algorithm>
#include <source_location>
#include <utility>

#include <stb_image.h>
#include <stb_image_write.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// printf checking
#if defined(__GNUC__) or defined(__clang__)
#define GNU_FORMAT_ATTRIB(fmt_pos, args_pos) [[gnu::format(printf, fmt_pos, args_pos)]]
#define MSVC_FMT_STR
#elif defined(_MSC_VER)
#define GNU_FORMAT_ATTRIB(fmt_pos, args_pos)
#define MSVC_FMT_STR _Printf_format_string_
#else
#define GNU_FORMAT_ATTRIB(fmt_pos, args_pos)
#define MSVC_FMT_STR
#endif

#define DEFAULT_ASSIGNMENT(value) (this == &value ? *this : (std::destroy_at(this), *std::construct_at(this, std::forward<decltype(value)>(value))))
#define COMPONENT_WISE(op) ([](auto &&vec) -> decltype(auto) { return ::game::utils::component_wise([](auto &&l, auto &&r) -> decltype(auto) { return l op r; }, vec); })

namespace game
{
  using namespace std::literals;
  auto constexpr error_breakpoint() {}
}
namespace game::utils
{
  template <typename... T>
  struct overloaded : T...
  {
    using T::operator()...;
  };

  template <typename T>
  concept arithmetic = std::is_arithmetic_v<T>;
  template <typename T>
  concept arithmetic_vec = arithmetic<typename T::value_type> and 1 <= T::length() and T::length() <= 4;
  static_assert(arithmetic_vec<glm::vec2>);
  template <typename T>
  concept arithmetic_vec_or_scalar = arithmetic<T> or arithmetic_vec<T>;

  struct snprintf_result
  {
    std::string_view msg;
    std::unique_ptr<char[]> alloc = {};
  };
  GNU_FORMAT_ATTRIB(2, 3)
  auto snprintf(std::span<char> buf, MSVC_FMT_STR char const *fmt, ...) -> snprintf_result;
  GNU_FORMAT_ATTRIB(1, 2)
  auto errorf(MSVC_FMT_STR char const *fmt, ...) -> void;

  auto read_all(char const *filepath, char const *mode = "r") -> std::string;

  struct failed_assert : std::runtime_error
  {
    using std::runtime_error::runtime_error;
  };
  auto static inline assertf(auto &&value, char const *fmt, auto... args) -> decltype(value)
  {
    if (value)
      return std::forward<decltype(value)>(value);
    char buf[0x100];
    auto [msg, alloc] = snprintf(buf, fmt, args...);
    throw failed_assert{msg.data()};
  }
  auto static inline assertf(auto &&value, char const *fmt) -> decltype(value) = delete; // C++26 : = delete("assertf(value, msg) is deleted. Use assertf(value, "%s", msg) instead.");

  template <glm::length_t L, typename T>
  auto inline component_wise(std::invocable<T const, T const> auto &&pair_wise_operation, glm::vec<L, T> const &vec) noexcept -> T
  {
    auto span = std::span{&vec[0], L};
    auto res = span[0];
    for (auto const &v : span.subspan(1))
      res = pair_wise_operation(res, v);
    return res;
  }

  auto to_utf32(std::u8string_view utf8) noexcept -> std::pair<char32_t, size_t>;
  auto to_utf8(char32_t utf32) noexcept -> std::array<char8_t, 5>;
}

#endif // COMMON_HPP