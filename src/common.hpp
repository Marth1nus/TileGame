#ifndef COMMON_HPP
#define COMMON_HPP

#include <cstdio>
#include <cstdarg>

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

#define DEFAULT_MOVE_CONSTRUCTOR(value) this == &value ? *this : (std::destroy_at(this), *std::construct_at(this, std::forward<decltype(value)>(value)))

namespace game
{
  using namespace std::literals;
  auto constexpr error_breakpoint() {}
}
namespace game::utils
{
  struct failed_assert : std::runtime_error
  {
    using std::runtime_error::runtime_error;
  };
  struct snprintf_result
  {
    std::string_view msg;
    std::unique_ptr<char[]> alloc = {};
  };
  auto snprintf(std::span<char> buf, MSVC_FMT_STR char const *fmt, ...) -> snprintf_result GNU_FORMAT_ATTRIB(2, 3);
  auto read_all(char const *filepath, char const *mode = "r") -> std::string;
  auto static inline assertf(auto &&value, MSVC_FMT_STR char const *fmt, auto... args) -> decltype(value) GNU_FORMAT_ATTRIB(2, 3)
  {
    if (value)
      return std::forward<decltype(value)>(value);
    if constexpr (not sizeof...(args))
      throw failed_assert{fmt};
    else
    {
      char buf[0x100];
      auto [msg, alloc] = snprintf(buf, fmt, args...);
      throw failed_assert{msg.data()};
    }
  }
  auto static inline errorf(MSVC_FMT_STR char const *fmt, auto... args) -> void GNU_FORMAT_ATTRIB(1, 2)
  {
#if defined(__EMSCRIPTEN__)
    auto msg = fmt;
#else  // defined(__EMSCRIPTEN__)
    char buf[0x100];
    auto [msg_sv, alloc] = snprintf(buf, "\033[31m%s\033[0m\n", fmt);
    auto msg = msg_sv.data();
#endif // defined(__EMSCRIPTEN__)
    if constexpr (sizeof...(args))
      std::fprintf(stderr, msg, args...);
    else
      std::fprintf(stderr, "%s", msg);
    error_breakpoint();
  }
}

#endif // COMMON_HPP