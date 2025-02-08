#include "common.hpp"

namespace game::utils
{
  auto snprintf(std::span<char> buf, MSVC_FMT_STR char const *fmt, ...) -> snprintf_result GNU_FORMAT_ATTRIB(2, 3)
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
      str = std::unique_ptr<char[]>(new char[cap]);
      msg = str.get();
      continue;
    }
    return {{msg, len}, std::move(str)};
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
}
