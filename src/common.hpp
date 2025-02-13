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
#include <source_location>

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
  GNU_FORMAT_ATTRIB(2, 3)
  auto snprintf(std::span<char> buf, MSVC_FMT_STR char const *fmt, ...) -> snprintf_result;
  GNU_FORMAT_ATTRIB(1, 2)
  auto errorf(MSVC_FMT_STR char const *fmt, ...) -> void;
  auto read_all(char const *filepath, char const *mode = "r") -> std::string;
  auto static inline assertf(auto &&value, char const *fmt, auto... args) -> decltype(value)
    requires(sizeof...(args) > 0)
  {
    if (value)
      return std::forward<decltype(value)>(value);
    else
    {
      char buf[0x100];
      auto [msg, alloc] = snprintf(buf, fmt, args...);
      throw failed_assert{msg.data()};
    }
  }
}

#endif // COMMON_HPP