#include "common.hpp"

namespace game::utils
{
  GNU_FORMAT_ATTRIB(2, 3)
  auto snprintf(std::span<char> buf, MSVC_FMT_STR char const *fmt, ...) -> snprintf_result
  {
    auto msg = buf.data();
    auto cap = buf.size(), len = (size_t)0;
    auto str = std::unique_ptr<char[]>();
    while (true)
    {
      va_list args;
      va_start(args, fmt);
      len = std::vsnprintf(msg, cap, fmt, args);
      va_end(args);
      if (len < cap)
        break;
      cap = len + 1;
      str = std::make_unique<char[]>(cap);
      msg = str.get();
      continue;
    }
    return {{msg, len}, std::move(str)};
  }
  GNU_FORMAT_ATTRIB(1, 2)
  auto errorf(MSVC_FMT_STR char const *fmt, ...) -> void
  {
#if defined(__EMSCRIPTEN__)
    auto msg = fmt;
#else  // defined(__EMSCRIPTEN__)
    char buf[0x100];
    auto [msg_sv, alloc] = snprintf(buf, "\033[31m%s\033[0m\n", fmt);
    auto msg = msg_sv.data();
#endif // defined(__EMSCRIPTEN__)
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, msg, args);
    va_end(args);
    error_breakpoint();
  }
  auto read_all(char const *filepath, char const *mode) -> std::string
  {
    auto res = std::string{};
    if (auto file = std::fopen(filepath, mode))
    {
      std::fseek(file, 0, SEEK_END), res.resize(std::ftell(file));
      std::fseek(file, 0, SEEK_SET), res.resize(std::fread(res.data(), sizeof(res[0]), res.size(), file));
      std::fclose(file);
    }
    return res;
  }
  auto to_utf32(std::u8string_view utf8) noexcept -> std::pair<char32_t, size_t>
  {
    for (auto mask : {0b1'0000000u,
                      0b111'00000u,
                      0b1111'0000u,
                      0b11111'000u})
    {
      auto byte_count = (uint32_t)std::popcount(mask) - (std::popcount(mask) > 1);
      if (utf8.size() < byte_count)
        return {-1u, 0u};
      auto codepoint = 0u;
      for (auto i = 0u; i < byte_count; i++, mask = 0b11'000000u)
        if (((uint32_t)utf8.at(i) bitand mask) == ((mask << 1u) bitand 0b1111'1111u))
          codepoint = (codepoint << (8u - std::popcount(mask))) bitor ((uint32_t)utf8.at(i) bitand ~mask);
        else if (i == 0u)
          goto next_mask;
        else
          return {-1u, i};
      return {codepoint, byte_count};
    next_mask:
      continue;
    }
    return {-1u, 0u};
  }
  auto to_utf8(char32_t utf32) noexcept -> std::array<char8_t, 5>
  {
    /**/ if (utf32 < (1u << 07u))
      return {char8_t(0b0'0000000u bitor 0b0'1111111u bitand (utf32 >> 00u)), u8'\0'};
    else if (utf32 < (1u << 11u))
      return {char8_t(0b110'00000u bitor 0b000'11111u bitand (utf32 >> 06u)),
              char8_t(0b10'000000u bitor 0b00'111111u bitand (utf32 >> 00u)), u8'\0'};
    else if (utf32 < (1u << 16u))
      return {char8_t(0b1110'0000u bitor 0b0000'1111u bitand (utf32 >> 12u)),
              char8_t(0b10'000000u bitor 0b00'111111u bitand (utf32 >> 06u)),
              char8_t(0b10'000000u bitor 0b00'111111u bitand (utf32 >> 00u)), u8'\0'};
    else if (utf32 < (1u << 21u))
      return {char8_t(0b11110'000u bitor 0b00000'111u bitand (utf32 >> 18u)),
              char8_t(0b10'000000u bitor 0b00'111111u bitand (utf32 >> 12u)),
              char8_t(0b10'000000u bitor 0b00'111111u bitand (utf32 >> 06u)),
              char8_t(0b10'000000u bitor 0b00'111111u bitand (utf32 >> 00u)), u8'\0'};
    else
      return {u8'\0'};
  }
}
