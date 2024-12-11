#ifndef COMMON_HPP
#define COMMON_HPP

#include <cstdio>
#include <cstdarg>

#include <memory>
#include <optional>
#include <stdexcept>
#include <expected>

#include <span>
#include <array>
#include <vector>

#include <string>
#include <string_view>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#undef assert

#if defined(_MSC_VER)
#include <sal.h>
#define printf_fmt_check(fmt_index, varargs_index)
#define printf_fmt_arg_check _Printf_format_string_
#else
#define printf_fmt_check(fmt_index, varargs_index) [[gnu::format(printf, fmt_index, varargs_index)]]
#define printf_fmt_arg_check
#endif

namespace game
{
  auto inline constexpr print_error_breakpoint() noexcept {}
  using std::literals::operator""sv,
      std::literals::operator""s;
}
namespace game::utils
{
  struct snprintf_result
  {
    std::string_view msg;
    std::unique_ptr<char[]> alloc;
  };
  struct failed_assert : std::runtime_error
  {
    using std::runtime_error::runtime_error;
  };
  auto file_read_all(char const *path) -> std::string;
  auto vsnprintf(std::span<char> buf, char const *fmt, va_list args) -> snprintf_result;
  auto inline constexpr assert(auto &&value, char const *msg = "failed assert") -> decltype(value)
  {
    if (value) [[likely]]
      return std::forward<decltype(value)>(value);
    throw failed_assert{msg};
  }
  printf_fmt_check(2, 3) auto inline constexpr assertf(auto &&value, printf_fmt_arg_check char const *fmt, ...) -> decltype(value)
  {
    if (value) [[likely]]
      return std::forward<decltype(value)>(value);
    char buf[0x100];
    va_list args;
    va_start(args, fmt);
    auto [msg, alloc] = vsnprintf(buf, fmt, args);
    va_end(args);
    throw failed_assert{msg.data()};
  }
  printf_fmt_check(2, 3) auto snprintf(std::span<char> buf, printf_fmt_arg_check char const *fmt, ...) -> snprintf_result;
  printf_fmt_check(1, 2) auto print_errorf(printf_fmt_arg_check char const *fmt, ...) -> int;
}

#endif // COMMON_HPP