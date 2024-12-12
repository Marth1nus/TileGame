#include "common.hpp"

auto game::utils::file_read_all(char const *path) -> std::string
{
  auto res = std::string{};
  auto file = std::fopen(path, "r");
  if (not file)
    return res;
  res.resize((size_t)(std::fseek(file, 0, SEEK_END), std::ftell(file)));
  res.resize((size_t)(std::fseek(file, 0, SEEK_SET), std::fread(res.data(), sizeof(res[0]), res.size(), file)));
  std::fclose(file);
  return res;
}
auto game::utils::vsnprintf(std::span<char> buf, char const *fmt, va_list args) -> snprintf_result
{
  auto msg = buf.data();
  auto cap = buf.size(), len = cap;
  auto str = std::unique_ptr<char[]>{};
  while (1)
  {
    va_list args_copy;
    va_copy(args_copy, args);
    len = std::vsnprintf(msg, cap, fmt, args_copy);
    va_end(args_copy);
    if (len < cap) [[likely]]
      break;
    cap = len + 1;
    str = std::unique_ptr<char[]>(new char[cap]);
    msg = str.get();
  }
  return {{msg, len}, std::move(str)};
}
printf_fmt_check(2, 3) auto game::utils::snprintf(std::span<char> buf, printf_fmt_arg_check char const *fmt, ...) -> snprintf_result
{
  va_list args;
  va_start(args, fmt);
  auto res = vsnprintf(buf, fmt, args);
  va_end(args);
  return res;
}
printf_fmt_check(1, 2) auto game::utils::print_errorf(printf_fmt_arg_check char const *fmt, ...) -> int
{
#if defined(__EMSCRIPTEN__)
  auto fmt_ex = std::string_view{fmt};
#else  // defined(__EMSCRIPTEN__)
  char buf[0x100];
  auto [fmt_ex, alloc] = snprintf(buf, "\033[31m%s\033[0m\n", fmt);
#endif // defined(__EMSCRIPTEN__)
  va_list args;
  va_start(args, fmt);
  auto res = std::vfprintf(stderr, fmt_ex.data(), args);
  va_end(args);
  print_error_breakpoint();
  return res;
}
