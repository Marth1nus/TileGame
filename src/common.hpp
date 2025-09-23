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

#include <optional>
#include <expected>

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
  auto inline static assertf(auto &&value, char const *fmt, auto... args) -> decltype(value)
  {
    if (value)
      return std::forward<decltype(value)>(value);
    char buf[0x100];
    auto [msg, alloc] = snprintf(buf, fmt, args...);
    throw failed_assert{msg.data()};
  }
  auto inline static assertf(auto &&value, char const *fmt) -> decltype(value) = delete; // C++26 : = delete("assertf(value, msg) is deleted. Use assertf(value, "%s", msg) instead.");

  template <glm::length_t L, typename T>
  auto inline static constexpr component_wise(std::invocable<T const, T const> auto &&pair_wise_operation, glm::vec<L, T> const &vec) noexcept -> T
  {
    auto span = std::span{&vec[0], L};
    auto res = span[0];
    for (auto const &v : span.subspan(1))
      res = pair_wise_operation(res, v);
    return res;
  }

  struct utf8_codepoint
  {
    std::array<char8_t, 4> chars = {};
    auto inline static constexpr byte_type(char8_t c) noexcept
    {
      struct result
      {
        size_t value;
        auto inline constexpr is_error /*    */ () const noexcept { return value < 1zu; }
        auto inline constexpr is_continue /* */ () const noexcept { return value > 4zu; }
        auto inline constexpr is_size /*     */ () const noexcept { return not(is_error() or is_continue()); }
        auto inline constexpr size() const noexcept { return is_size() ? value : 0zu; }
      };
      return result{/*   */ ((c & 0b1'0000000u) == 0b0'0000000u) ? 1zu
                    : /* */ ((c & 0b11'000000u) == 0b10'000000u) ? 5zu
                    : /* */ ((c & 0b111'00000u) == 0b110'00000u) ? 2zu
                    : /* */ ((c & 0b1111'0000u) == 0b1110'0000u) ? 3zu
                    : /* */ ((c & 0b11111'000u) == 0b11110'000u) ? 4zu
                                                                 : 0zu};
    }
    auto inline /*  */ constexpr completion() const noexcept
    {
      struct result
      {
        size_t size, error;
        auto inline constexpr has_size /*  */ () const noexcept -> bool { return not has_error(); }
        auto inline constexpr has_error /* */ () const noexcept -> bool { return error < size; }
        auto inline constexpr size_or /*  */ (size_t d) const noexcept { return has_size /*  */ () ? size /*  */ : d; }
        auto inline constexpr error_or /* */ (size_t d) const noexcept { return has_error /* */ () ? error /* */ : d; }
      };
      auto const size = byte_type(chars.at(0)).size();
      auto error = 0zu;
      for (auto i = 1zu; i < size; i++)
        if (byte_type(chars.at(i)).is_continue())
          error++;
        else
          break;
      return result{.size = size, .error = error};
    }
    auto inline /*  */ constexpr data() const noexcept { return chars.data(); }
    auto inline /*  */ constexpr size() const noexcept { return completion().size_or(0zu); }
    auto inline /*  */ constexpr begin() const noexcept { return data(); }
    auto inline /*  */ constexpr end() const noexcept { return data() + size(); }
    auto inline static constexpr utf32(char32_t utf32) noexcept -> utf8_codepoint
    {
      /**/ if (utf32 < (1u << 07u))
        return {char8_t(0b0'0000000u | (0b0'1111111u & (utf32 >> 00u)))};
      else if (utf32 < (1u << 11u))
        return {char8_t(0b110'00000u | (0b000'11111u & (utf32 >> 06u))),
                char8_t(0b10'000000u | (0b00'111111u & (utf32 >> 00u)))};
      else if (utf32 < (1u << 16u))
        return {char8_t(0b1110'0000u | (0b0000'1111u & (utf32 >> 12u))),
                char8_t(0b10'000000u | (0b00'111111u & (utf32 >> 06u))),
                char8_t(0b10'000000u | (0b00'111111u & (utf32 >> 00u)))};
      else if (utf32 < (1u << 21u))
        return {char8_t(0b11110'000u | (0b00000'111u & (utf32 >> 18u))),
                char8_t(0b10'000000u | (0b00'111111u & (utf32 >> 12u))),
                char8_t(0b10'000000u | (0b00'111111u & (utf32 >> 06u))),
                char8_t(0b10'000000u | (0b00'111111u & (utf32 >> 00u)))};
      else
        return {u8"\uFFFD"};
    }
    auto inline /*  */ constexpr utf32() const noexcept -> char32_t
    {
      /**/ if (size() == 1zu)
        return char32_t{((chars[0] & 0b0'1111111u) << 00u)};
      else if (size() == 2zu)
        return char32_t{((chars[0] & 0b000'11111u) << 06u) +
                        ((chars[1] & 0b00'111111u) << 00u)};
      else if (size() == 3zu)
        return char32_t{((chars[0] & 0b0000'1111u) << 12u) +
                        ((chars[1] & 0b00'111111u) << 06u) +
                        ((chars[2] & 0b00'111111u) << 00u)};
      else if (size() == 4zu)
        return char32_t{((chars[0] & 0b00000'111u) << 18u) +
                        ((chars[1] & 0b00'111111u) << 12u) +
                        ((chars[2] & 0b00'111111u) << 06u) +
                        ((chars[3] & 0b00'111111u) << 00u)};
      else
        return U'\uFFFD';
    }
  };
  struct to_utf8_fn : std::ranges::range_adaptor_closure<to_utf8_fn>
  {
    template <std::ranges::input_range Range>
      requires std::same_as<std::ranges::range_value_t<Range>, char32_t>
    auto inline static constexpr operator()(Range &&range) noexcept
    {
      return range |
             std::views::transform([](char32_t c)
                                   { return utf8_codepoint::utf32(c); }) |
             std::views::join;
    }
  };
  auto inline static constexpr to_utf8 = to_utf8_fn{};
  struct to_utf32_fn : std::ranges::range_adaptor_closure<to_utf32_fn>
  {
    auto inline static constexpr operator()(std::u8string_view range) noexcept
    {
      return //
          std::array{range, u8"\uFFFD"sv} |
          std::views::join |
          std::views::adjacent_transform<4>(
              [](auto... chars)
              {
                auto const utf8 = utf8_codepoint{chars...};
                auto const first_byte = utf8.byte_type(utf8.chars.at(0));
                auto const [take, codepoint] = first_byte.is_continue() ? std::pair{0, U'\0'}
                                               : first_byte.is_error()  ? std::pair{1, U'\uFFFD'}
                                               : utf8.size()            ? std::pair{1, utf8.utf32()}
                                                                        : std::pair{0, U'\0'};
                return std::array{codepoint} | std::views::take(take);
              }) |
          std::views::join;
    }
  };
  auto inline static constexpr to_utf32 = to_utf32_fn{};
}

#endif // COMMON_HPP