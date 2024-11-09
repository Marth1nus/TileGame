#include <cstdio>

#include <expected>
#include <stdexcept>
#include <type_traits>
#include <concepts>

#include <ranges>
#include <algorithm>

#include <array>
#include <span>
#include <vector>
#include <string>
#include <string_view>

#include <tuple>
#include <memory>
#include <optional>

#include <chrono>

#include <lua.hpp>
#include <stb_image.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <entt/entt.hpp>
#include <box2d/box2d.h>

#undef assert
#ifdef __EMSCRIPTEN__
#define WEBGL 1
#include <emscripten.h>
#include <GLES3/gl3.h>
#include <GLFW/glfw3.h>
#else
#define WEBGL 0
#include <glad/gles2.h>
#include <GLFW/glfw3.h>
#endif

// define printf format checking atributes
#if defined(__GNUC__) or defined(__clang__)
#define printf_fmt_check_attribute [[gnu::format(printf, 2, 3)]]
#define printf_fmt_check
#elif defined(_MSC_VER)
#include <sal.h>
#define printf_fmt_check_attribute
#define printf_fmt_check _In_z_ _Printf_format_string_
#else
#define printf_fmt_check_attribute
#define printf_fmt_check
#endif

using glm::vec2, glm::uvec2, glm::ivec2, glm::uint,
    std::literals::operator""sv,
    std::literals::operator""s;

namespace utils
{
  struct immutable_string
  {
  public:
    template <std::convertible_to<std::string_view> T>
    inline constexpr immutable_string(T &&str) noexcept
        : immutable_string(static_cast<std::string_view>(str),
                           std::convertible_to<T, char const *> or std::same_as<T, std::string>,                  // char const[] and std::string are expected to null terminated
                           std::is_rvalue_reference_v<decltype(str)> and not std::is_trivially_destructible_v<T>) // clone if `std::string&&`-like
    {
    }
    inline constexpr immutable_string(std::string_view str, bool null_terminated = 0, bool clone = 0) noexcept
    {
      (m_string /*             */ = str.data() /* */);
      (m_size /*               */ = str.size() /* */);
      (m_has_allocation /*     */ = false /*      */);
      (m_is_null_terminated /* */ = null_terminated and m_string[m_size] == '\0');
      if (clone)
        *this = this->clone();
    }
    inline constexpr immutable_string() noexcept = default;
    inline constexpr immutable_string(immutable_string &&o) noexcept
    {
      (m_string /*             */ = o.m_string /*             */), (o.m_string /*             */ = 0);
      (m_size /*               */ = o.m_size /*               */), (o.m_size /*               */ = 0);
      (m_has_allocation /*     */ = o.m_has_allocation /*     */), (o.m_has_allocation /*     */ = 0);
      (m_is_null_terminated /* */ = o.m_is_null_terminated /* */), (o.m_is_null_terminated /* */ = 0);
    }
    inline constexpr immutable_string(immutable_string const &o) noexcept
    {
      (m_string /*             */ = o.m_string /*             */);
      (m_size /*               */ = o.m_size /*               */);
      (m_has_allocation /*     */ = o.m_has_allocation /*     */);
      (m_is_null_terminated /* */ = o.m_is_null_terminated /* */);
      if (m_has_allocation)
        ++*m_ref_count();
    }
    inline constexpr ~immutable_string() noexcept
    {
      if (m_has_allocation and --*m_ref_count() == 0)
        ::operator delete(m_ref_count(), std::align_val_t(alignof(size_t)));
      (m_string /*             */ = 0);
      (m_size /*               */ = 0);
      (m_has_allocation /*     */ = 0);
      (m_is_null_terminated /* */ = 0);
    }

    inline constexpr auto operator=(immutable_string &&o) noexcept -> immutable_string &
    {
      if (this == &o)
        return *this;
      this->~immutable_string();
      return *new (this) immutable_string(std::forward<decltype(o)>(o));
    }
    inline constexpr auto operator=(immutable_string const &o) noexcept -> immutable_string &
    {
      if (this == &o)
        return *this;
      this->~immutable_string();
      return *new (this) immutable_string(std::forward<decltype(o)>(o));
    }

    inline constexpr auto has_allocation() const noexcept -> bool { return m_has_allocation; }
    inline constexpr auto null_terminated() const noexcept -> bool { return m_is_null_terminated; }
    inline constexpr auto view() const noexcept -> std::string_view { return {m_string, m_size}; }
    inline constexpr auto c_str() const noexcept -> char const * { return m_is_null_terminated ? m_string : nullptr; }

    inline constexpr auto operator==(immutable_string const &o) const noexcept { return view() == o.view(); }
    inline constexpr auto operator<=>(immutable_string const &o) const noexcept { return view() <=> o.view(); }
    inline constexpr operator std::string_view() const { return view(); }

    [[nodiscard("immutable_string::clone returns an allocating string")]]
    auto clone(bool deep = false) const noexcept -> immutable_string
    {
      auto res = *this;
      if (res.has_allocation() and not deep)
        return res;

      auto const view = res.view();
      if (view.empty())
        return res;

      auto const buf = ::operator new(sizeof(*m_ref_count()) + sizeof(*m_string) * (view.size() + 1), // m_ref_count + m_string
                                      std::align_val_t(alignof(size_t)));
      auto const ref = new (buf) size_t{1};
      auto const str = reinterpret_cast<char *>(ref + 1);
      std::copy(view.begin(), view.end(), str)[0] = '\0';

      res.m_string /*             */ = str;
      res.m_size /*               */ = view.size();
      res.m_has_allocation /*     */ = true;
      res.m_is_null_terminated /* */ = true;
      *res.m_ref_count() /*       */ = 1;
      return res;
    }
    inline constexpr auto c_str(bool force) noexcept -> char const *
    {
      if (force and not null_terminated())
        *this = clone();
      return c_str();
    }

  private:
    char const *m_string = nullptr;
    size_t m_size = 0;
    bool m_has_allocation : 1 = false;
    bool m_is_null_terminated : 1 = false;
    // member `m_ref_count` only exists if `m_has_allocation` and is located before `m_string` in that case
    inline constexpr auto m_ref_count() const noexcept -> size_t *
    {
      return not m_has_allocation ? nullptr : const_cast<size_t *>(reinterpret_cast<size_t const *>(m_string) - 1);
    }
  };
  [[nodiscard("May return an allocated string")]]
  auto static vsnprintf(char buf[], size_t cap, char const *fmt, va_list args) -> std::pair<std::string_view, std::unique_ptr<char[]>>
  {
    auto len = cap;
    auto str = std::unique_ptr<char[]>{};
    auto msg = buf;
    for (;;) // snprintf until len < cap
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
  [[nodiscard("May return an allocated string")]]
  printf_fmt_check_attribute auto static snprintf(char buf[], size_t cap, printf_fmt_check char const *fmt, ...) -> decltype(vsnprintf(buf, cap, fmt, va_list{}))
  {
    va_list args;
    va_start(args, fmt);
    auto res = vsnprintf(buf, cap, fmt, args);
    va_end(args);
    return res;
  }
  template <size_t cap>
  printf_fmt_check_attribute auto static snprintf(char (&buf)[cap], printf_fmt_check char const *fmt, ...) -> decltype(vsnprintf(buf, cap, fmt, va_list{}))
  {
    va_list args;
    va_start(args, fmt);
    auto res = vsnprintf(buf, cap, fmt, args);
    va_end(args);
    return res;
  }
  struct failed_assert : std::runtime_error
  {
    using std::runtime_error::runtime_error;
  };
  auto inline static constexpr assert(auto &&value, char const *msg) -> decltype(value)
  {
    if (value) [[likely]]
      return std::forward<decltype(value)>(value);
    throw failed_assert(msg);
  }
  printf_fmt_check_attribute auto static assertf(auto &&value, printf_fmt_check char const *fmt, ...) -> decltype(value)
  {
    if (value) [[likely]]
      return std::forward<decltype(value)>(value);
    char buf[0x100];
    va_list args;
    va_start(args, fmt);
    auto [msg, alloc] = vsnprintf(buf, std::size(buf), fmt, args);
    va_end(args);
    throw failed_assert(msg.data());
  }
  auto static inline file_read_all(char const *filepath) -> std::string
  {
    std::string res;
    auto file = std::fopen(filepath, "r");
    if (not file)
      return res;
    std::fseek(file, 0, SEEK_END), res.resize(std::ftell(file));
    std::fseek(file, 0, SEEK_SET), res.resize(std::fread(res.data(), sizeof(res.front()), res.size(), file));
    std::fclose(file);
    return res;
  }
  /* clang-format off */ template <typename... T> struct overload : T... {}; /* clang-format on */
  /* clang-format off */ template <typename T, size_t N> struct compile_time_array_wrapper { T arr[N]; }; /* clang-format on */
  template <typename T>
  struct shared
  {
    using ptr_t = std::shared_ptr<T>;
    ptr_t ptr;

    inline constexpr shared() noexcept = default;
    inline constexpr shared(shared &&) noexcept = default;
    inline constexpr shared(shared const &) noexcept = default;
    inline constexpr shared &operator=(shared &&) noexcept = default;
    inline constexpr shared &operator=(shared const &) noexcept = default;

    inline constexpr shared(ptr_t ptr) noexcept : ptr(std::move(ptr)) {}
    inline constexpr operator ptr_t const &() const noexcept { return ptr; }

    inline constexpr shared(T *ptr, auto &&deleter) noexcept : ptr(ptr, std::forward<decltype(deleter)>(deleter)) {}
    inline constexpr operator T *() const noexcept { return ptr.get(); }
  };
#define ASSERT(...) utils::assert((__VA_ARGS__), #__VA_ARGS__)
#define DEBUG_ASSERT(...) utils::assert((__VA_ARGS__), #__VA_ARGS__)
}
using utils::shared, utils::overload;
namespace std
{
  template <>
  struct hash<::utils::immutable_string> : hash<string_view>
  {
  };
}

static struct glfw
{
  glfw() { ASSERT(glfwInit()); }
  ~glfw() { glfwTerminate(); }
} const glfw{};
static auto window = []()
{
  auto width = 720, height = width;
  auto title = "TileGame";
  auto window = shared{glfwCreateWindow(width, height, title, 0, 0), glfwDestroyWindow};
  glfwMakeContextCurrent(ASSERT(window));
#ifdef __EMSCRIPTEN__
#else
  ASSERT(gladLoadGLES2(glfwGetProcAddress));
#endif
  glViewport(0, 0, width, height);
  return window;
}();

namespace render
{
  auto static glCheckError()
  {
    for (GLenum err; (err = glGetError()) not_eq GL_NO_ERROR;)
    {
      auto str = /* clang-format off */ [err]{ switch (err) {
        case GL_INVALID_ENUM:                  return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE:                 return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION:             return "GL_INVALID_OPERATION";
        case GL_OUT_OF_MEMORY:                 return "GL_OUT_OF_MEMORY";
        case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
        default:                               return "UNKNOWN_ERROR"; } }(); /* clang-format on */
      std::fprintf(stderr, "\033[31mOpenGL ES error 0x%x %s\n\033[0m", err, str);
    }
  }
  [[nodiscard("Returns 0 or a shader object. glDeleteShader to delete.")]]
  static auto make_shader(GLenum type, std::string_view glsl, std::string_view defines)
  {
    auto version = std::string_view{};
    { // Split `version` from `glsl`
      auto find = glsl.find("#version");
      utils::assertf(find not_eq std::string_view::npos, "glsl `#version` %s", "required");
      glsl = glsl.substr(find);
      find = glsl.find('\n');
      utils::assertf(find not_eq std::string_view::npos, "glsl `#version` %s", "string must be on its own line.");
      version = glsl.substr(0, find + 1); // version line including \n
      glsl = glsl.substr(find);           // rest of glsl starting with \n (just in case)
    }
    auto strings = std::array{version, defines, glsl};
    auto sources = std::array<GLchar const *, strings.size()>{};
    auto lengths = std::array<GLsizei /*  */, strings.size()>{};
    for (auto i = 0u; i < strings.size(); i++)
    {
      sources.at(i) = /*    */ strings.at(i).data();
      lengths.at(i) = (GLsizei)strings.at(i).size();
    }
    auto sid = glCreateShader(type);
    glShaderSource(sid, (GLsizei)sources.size(), sources.data(), lengths.data());
    glCompileShader(sid);
    if (GLint status, len; glGetShaderiv(sid, GL_COMPILE_STATUS, &status), not status)
    {
      std::string log;
      log.resize((glGetShaderiv(sid, GL_INFO_LOG_LENGTH, &len), len));
      log.resize((glGetShaderInfoLog(sid, len, &len, log.data()), len));
      std::fprintf(stderr, "\033[31m%s Shader Error: %s\033[0m", type == GL_VERTEX_SHADER ? "Vertex" : "Fragment", log.c_str());
      glDeleteShader(sid), sid = 0;
    }
    glCheckError();
    return sid;
  }
  static auto make_program(std::string_view vert_glsl, std::string_view frag_glsl, std::string_view defines)
  {
    auto pid = glCreateProgram();
    auto vid = make_shader(GL_VERTEX_SHADER, vert_glsl, defines);
    auto fid = make_shader(GL_FRAGMENT_SHADER, frag_glsl, defines);
    glAttachShader(pid, vid);
    glAttachShader(pid, fid);
    glLinkProgram(pid);
    if (GLint status, len; glGetProgramiv(pid, GL_LINK_STATUS, &status), not status)
    {
      std::string log;
      log.resize((glGetProgramiv(pid, GL_INFO_LOG_LENGTH, &len), len));
      log.resize((glGetProgramInfoLog(pid, len, &len, log.data()), len));
      std::fprintf(stderr, "\033[31mProgram Error: %s\033[0m", log.c_str());
      glDeleteProgram(pid), pid = 0;
    }
    glCheckError();
    return pid;
  }
  static auto make_program(std::string_view vert_glsl, std::string_view frag_glsl, int texture_slots, int webgl = WEBGL)
  {
    auto static constexpr &buf_fmt =
        "#define TEXTURE_SLOTS %3du\n"
        "#define WEBGL         %3d \n";
    char buf[std::size(buf_fmt)];
    auto buf_len = std::snprintf(buf, std::size(buf), buf_fmt, texture_slots, webgl);
    ASSERT(buf_len < std::size(buf));
    auto defines = std::string_view(buf, buf_len);
    return make_program(vert_glsl, frag_glsl, defines);
  }
  static auto texture_load(GLuint tid, char const *path)
  {
    glBindTexture(GL_TEXTURE_2D, tid);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    auto width = 1, height = 1, channels = 4;
    auto pixels = utils::assertf(stbi_load(path, &width, &height, &channels, channels), "Could not load image: %s", path);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    stbi_image_free(pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glCheckError();
  }
}

namespace render::tile
{
  struct buffer
  {
    GLuint bo;
    size_t capacity;
    auto upload(std::span<std::byte const> bytes, GLenum target, GLenum usage)
    {
      glBindBuffer(target, bo);
      glBufferData(target, (GLsizei)(capacity = bytes.size()), bytes.data(), usage);
    }
    auto resize(size_t size, GLenum target, GLenum usage)
    {
      return upload(std::span((std::byte const *)0, size), target, usage);
    }
    auto update(std::span<std::byte const> bytes, size_t offset, GLenum target)
    {
      utils::assertf(offset + bytes.size() <= capacity,
                     "Buffer (%u) too small for update. Offset:%zu, Size:%zu, Capacity:%zu",
                     bo, offset, bytes.size(), capacity);
      glBindBuffer(target, bo);
      glBufferSubData(target, offset, (GLsizei)bytes.size(), bytes.data());
    }
  };
  using tile = uint32_t;
  struct chunk
  {
    glm::ivec2 pos, size;
  };
  struct tileset
  {
    uint32_t first, last, columns, rows,
        tex, padding0, padding1, padding2;
    glm::vec2 offset, size;
  };
  auto inline static constexpr vertices = std::array{
      std::array{0.0f, 0.0f},
      std::array{0.0f, 1.0f},
      std::array{1.0f, 0.0f},
      std::array{1.0f, 1.0f},
  };
  struct renderer
  {
  public:
    auto delete_program(bool expect_fail = false)
    {
      utils::assertf(m_pid or expect_fail, "can not delete a deleted %s", "program");
      glDeleteProgram(m_pid), m_pid = 0;
      glCheckError();
      m_uniform = {};
      m_uniform_locations.clear();
      return;
    }
    auto reload_program(std::string_view vert_glsl, std::string_view frag_glsl)
    {
      delete_program(1);

      auto &pid = m_pid;
      auto texture_slots = 0;
      glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &texture_slots);
      pid = make_program(vert_glsl, frag_glsl, texture_slots);
      if (not pid)
        return false;

      glUseProgram(pid);
      m_uniform = {
          .TILESETS /* */ = (int)glGetUniformBlockIndex(pid, "TILESETS" /*        */),
          .tilesets_count /*  */ = glGetUniformLocation(pid, "tilesets_count" /*  */),
          .projection /*      */ = glGetUniformLocation(pid, "projection" /*      */),
          .textures /*        */ = glGetUniformLocation(pid, "textures" /*        */),
      };
      glCheckError();

      auto const textures = std::unique_ptr<GLint[]>(new GLint[texture_slots]);
      for (auto i = 0; i < texture_slots; i++)
        textures[i] = i;

      glUniformBlockBinding(pid, m_uniform.TILESETS /*       */, 0);
      glUniform1ui /*        */ (m_uniform.tilesets_count /* */, 0);
      glUniformMatrix4fv /*  */ (m_uniform.projection /*     */, 1, 0, &projection[0][0]);
      glUniform1iv /*        */ (m_uniform.textures /*       */, texture_slots, textures.get());
      glCheckError();

      return true;
    }
    auto inline use_program() noexcept { glUseProgram(m_pid); }
    auto inline get_program() const noexcept { return m_pid; }
    auto inline get_uniforms() const noexcept -> auto & { return m_uniform; }
    auto get_uniform_location(std::string_view name) -> GLint
    {
      if (auto find = m_uniform_locations.find(name);
          find not_eq m_uniform_locations.end())
        return find->second;
      auto name_str = utils::immutable_string(name).clone();
      auto location = glGetUniformLocation(m_pid, name_str.c_str());
      m_uniform_locations.insert({std::move(name_str), location});
      glCheckError();
      return location;
    }
    auto update_uniforms()
    {
      glCheckError();
      glUniform1ui(m_uniform.tilesets_count, tileset_count);
      glCheckError();
      glVertexAttribDivisor(m_chunks_attrib, chunk_len);
      glCheckError();
      glUniformMatrix4fv(m_uniform.projection, 1, GL_FALSE, &projection[0][0]);
      glCheckError();
    }

    auto delete_vao(bool expect_fail = false)
    {
      utils::assertf(m_vao or expect_fail, "can not delete a deleted %s", "mesh");
      glDeleteVertexArrays(1, &m_vao), m_vao = 0;
      auto buffers = std::array{&m_vertices, &m_tiles, &m_chunks, &m_tilesets};
      auto bos = std::array<GLuint, buffers.size()>{};
      for (auto i = 0u; i < buffers.size(); i++)
        bos.at(i) = std::exchange(*buffers.at(i), {}).bo;
      glDeleteBuffers((GLsizei)bos.size(), bos.data());
      chunk_len = 0xffffffffu;
      glCheckError();
    }
    auto reload_vao()
    {
      using namespace glm;
      delete_vao(1);

      auto &vao = m_vao;
      glGenVertexArrays(1, &vao);
      auto const [vbo_vertices, vbo_tiles, vbo_chunks, ubo_tilesets] = [&]
      {
        auto buffers = std::array{&m_vertices, &m_tiles, &m_chunks, &m_tilesets};
        auto bos = std::array<GLuint, buffers.size()>{};
        glGenBuffers((GLsizei)bos.size(), bos.data());
        for (auto i = 0u; i < buffers.size(); i++)
          *buffers.at(i) = {bos.at(i)};
        glCheckError();
        return bos;
      }();
      auto const bytes = std::as_bytes(std::span(vertices));
      auto i = 0u;

      glBindVertexArray(vao);
      glCheckError();

      glBindBuffer(GL_ARRAY_BUFFER, vbo_vertices);
      glBufferData(GL_ARRAY_BUFFER, (GLsizei)bytes.size(), bytes.data(), GL_STATIC_DRAW);
      glCheckError();

      glVertexAttribPointer(i, 2, GL_FLOAT, GL_FALSE, sizeof(vertices.at(0)), (void *)0);
      glVertexAttribDivisor(i, 0);
      glEnableVertexAttribArray(i++);
      glCheckError();

      glBindBuffer(GL_ARRAY_BUFFER, vbo_tiles);
      glBufferData(GL_ARRAY_BUFFER, 1, 0, GL_DYNAMIC_DRAW);
      glCheckError();

      glVertexAttribIPointer(i, 1, GL_UNSIGNED_INT, sizeof(tile), (void *)0);
      glVertexAttribDivisor(i, 1);
      glEnableVertexAttribArray(i++);
      glCheckError();

      glBindBuffer(GL_ARRAY_BUFFER, vbo_chunks);
      glBufferData(GL_ARRAY_BUFFER, 1, 0, GL_DYNAMIC_DRAW);
      glCheckError();

      m_chunks_attrib = i;
      glVertexAttribIPointer(i, 4, GL_INT, sizeof(chunk), (void *)offsetof(chunk, pos));
      glVertexAttribDivisor(i, chunk_len);
      glEnableVertexAttribArray(i++);
      glCheckError();

      glBindBuffer(GL_UNIFORM_BUFFER, ubo_tilesets);
      glBufferData(GL_UNIFORM_BUFFER, 1, 0, GL_DYNAMIC_DRAW);
      glCheckError();

      return true;
    }
    auto inline bind_vao() noexcept { glBindVertexArray(m_vao); }
    auto inline get_vao() const noexcept { return m_vao; }

    auto delete_textures(bool expect_fail = false)
    {
      utils::assertf(not m_textures.empty() or expect_fail, "can not delete a deleted %s", "textures-array");
      glDeleteTextures((GLsizei)m_textures.size(), m_textures.data());
      m_textures.clear();
      m_texture_paths.clear();
      glCheckError();
    }
    auto reload_textures(std::vector<std::string> &&image_paths)
    {
      delete_textures(1);
      m_texture_paths = std::move(image_paths);
      m_textures.resize(m_texture_paths.size());
      glGenTextures((GLsizei)m_textures.size(), m_textures.data());
      for (auto i = 0u; i < m_textures.size(); i++)
        texture_load(m_textures.at(i), m_texture_paths.at(i).c_str());
      glCheckError();
    }
    auto bind_textures() const noexcept
    {
      for (auto i = size_t{0}; i < m_textures.size(); i++)
      {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, m_textures.at(i));
      }
      glCheckError();
    }

    auto inline upload(std::span<tile /*    */ const> data) { return m_tiles /*    */.upload(std::as_bytes(data), GL_ARRAY_BUFFER /*   */, GL_DYNAMIC_DRAW); }
    auto inline upload(std::span<chunk /*   */ const> data) { return m_chunks /*   */.upload(std::as_bytes(data), GL_ARRAY_BUFFER /*   */, GL_DYNAMIC_DRAW); }
    auto inline upload(std::span<tileset /* */ const> data) { return m_tilesets /* */.upload(std::as_bytes(data), GL_UNIFORM_BUFFER /* */, GL_DYNAMIC_DRAW); }

    auto inline update(std::span<tile /*    */ const> data, size_t offset = 0) { return m_tiles /*    */.update(std::as_bytes(data), offset * sizeof(data[0]), GL_ARRAY_BUFFER /*   */); }
    auto inline update(std::span<chunk /*   */ const> data, size_t offset = 0) { return m_chunks /*   */.update(std::as_bytes(data), offset * sizeof(data[0]), GL_ARRAY_BUFFER /*   */); }
    auto inline update(std::span<tileset /* */ const> data, size_t offset = 0) { return m_tilesets /* */.update(std::as_bytes(data), offset * sizeof(data[0]), GL_UNIFORM_BUFFER /* */); }

    auto inline update_chunk(int chunk_index, std::span<tile const> data, size_t offset = 0) { return update(data, chunk_len * chunk_index + offset); }

    auto prep_draw()
    {
      use_program();
      bind_vao();
      bind_textures();
      glCheckError();
    }
    auto draw()
    {
      auto const tile_count_capacity = m_tiles.capacity / sizeof(tile);
      auto const tile_count = std::min((size_t)this->tile_count, tile_count_capacity);
      glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, (GLsizei)vertices.size(), (GLsizei)tile_count);
      glCheckError();
    }

  public: // public uniform data cache
    GLuint tile_count = 0, chunk_len = 0xffffffffu, tileset_count = 0;
    glm::mat4 projection = glm::ortho<float>(-1, 1, 1, -1);

  private:
    GLuint m_pid{};
    struct internal_uniform_locations
    {
      GLint TILESETS,
          tilesets_count,
          projection,
          textures;
    } m_uniform{};
    std::unordered_map<utils::immutable_string, GLint> m_uniform_locations;

    GLuint m_vao{}, m_chunks_attrib{};
    buffer m_vertices{}, m_tiles{}, m_chunks{}, m_tilesets{};

    std::vector<GLuint> m_textures;
    std::vector<std::string> m_texture_paths;
  };
  static auto global_renderer = std::optional<renderer>{};
}

struct timer
{
  double stamp = glfwGetTime();
  auto elapsed() { return timer{}.stamp - stamp; }
  auto restart() { return std::exchange(*this, {}).elapsed(); }
};
struct ticker
{
  double dt = 1.0 / 30.0, start_time = glfwGetTime();
  size_t tik = 0, max_ticks_per_frame = 4;
  bool next() { return tik < size_t((glfwGetTime() - start_time) / dt) ? ++tik : false; }
  auto skip() { start_time = glfwGetTime() - tik * dt; }
};
static auto update_ticker = ticker{};

static auto L = shared<lua_State>{};
namespace lua
{
  namespace helper
  {
    template <typename T, size_t N>
    using CTS = ::utils::compile_time_array_wrapper<T, N>;
    template <auto event_name> // CTS
    static auto constexpr event_window_forward(GLFWwindow *window, auto... args)
    {
      static_assert(std::string_view{event_name.arr, 3} == "on_",
                    "event_name must start with \"on_\"");
      if (not L)
        return;
      auto const top = lua_gettop(L);
      auto const ev_name = event_name.arr;
      auto const call = [&](char const *name)
      {
        auto argc = 0;
        if (name)
          lua_pushstring(L, name), argc++;
        if constexpr (std::same_as<std::tuple<int, char const **>, decltype(std::tuple{args...})>)
        { // on drop event has array of strings
          auto const [paths_count, paths] = std::tuple{args...};
          lua_createtable(L, paths_count, 0), argc++;
          for (auto i = 1; auto path : std::span{paths, (size_t)paths_count})
            lua_pushstring(L, path), lua_seti(L, -2, i++);
        }
        else if constexpr (sizeof...(args))
        {
          auto const for_args = [&](auto arg)
          {
            if constexpr (std::integral<decltype(arg)>)
              lua_pushinteger(L, arg);
            else if constexpr (std::floating_point<decltype(arg)>)
              lua_pushnumber(L, arg);
            else if constexpr (std::convertible_to<decltype(arg), char const *>)
              lua_pushstring(L, arg);
            else
              static_assert(false, "unsupported arg type");
            argc++;
          };
          (for_args(args), ...);
        }
        if (lua_pcall(L, argc, 0, 0) not_eq LUA_OK)
          std::fprintf(stderr, "\033[31mLUA ERROR in `game.event.%s`: %s\n\033[0m", (name ? "on_event" : ev_name), lua_tostring(L, -1)), lua_pop(L, 1);
      };
      if (lua_getglobal(L, "game") == LUA_TTABLE and
          lua_getfield(L, -1, "event") == LUA_TTABLE)
      {
        if (lua_getfield(L, -1, ev_name) == LUA_TFUNCTION)
          call(nullptr);
        else
          lua_pop(L, 1);
        if (lua_getfield(L, -1, "on_event") == LUA_TFUNCTION)
          call(ev_name + sizeof("on"));
      }
      lua_settop(L, top);
    }
    static auto inline expect_arg_count(lua_State *L, int expected_argc) -> decltype(lua_gettop(L))
    {
      auto const argc = lua_gettop(L);
      if (expected_argc == argc)
        return argc;
      luaL_error(L, "Too %s arguments. Expected:%d Got:%d",
                 expected_argc < argc ? "many" : "few", expected_argc, argc);
      [[unreachable]] throw;
    }
  }
  namespace game::event
  {
    static auto window_init /* */ (lua_State *L) -> int // fun()
    {
      using helper::event_window_forward, helper::CTS;
      if (L not_eq ::L)
        luaL_error(L, "Must be the global lua state. global:0x%p, provided:0x%p", static_cast<lua_State *>(::L), L);
      glfwSetWindowPosCallback /*          */ (window, event_window_forward<CTS("on_window_pos" /*           */)>);
      glfwSetWindowSizeCallback /*         */ (window, event_window_forward<CTS("on_window_size" /*          */)>);
      glfwSetWindowCloseCallback /*        */ (window, event_window_forward<CTS("on_window_close" /*         */)>);
      glfwSetWindowRefreshCallback /*      */ (window, event_window_forward<CTS("on_window_refresh" /*       */)>);
      glfwSetWindowFocusCallback /*        */ (window, event_window_forward<CTS("on_window_focus" /*         */)>);
      glfwSetWindowIconifyCallback /*      */ (window, event_window_forward<CTS("on_window_iconify" /*       */)>);
      glfwSetWindowMaximizeCallback /*     */ (window, event_window_forward<CTS("on_window_maximize" /*      */)>);
      glfwSetFramebufferSizeCallback /*    */ (window, event_window_forward<CTS("on_framebuffer_size " /*    */)>);
      glfwSetWindowContentScaleCallback /* */ (window, event_window_forward<CTS("on_window_content_scale" /* */)>);
      glfwSetKeyCallback /*                */ (window, event_window_forward<CTS("on_key" /*                  */)>);
      glfwSetCharCallback /*               */ (window, event_window_forward<CTS("on_char" /*                 */)>);
      glfwSetCharModsCallback /*           */ (window, event_window_forward<CTS("on_char_mods" /*            */)>);
      glfwSetMouseButtonCallback /*        */ (window, event_window_forward<CTS("on_mouse_button" /*         */)>);
      glfwSetCursorPosCallback /*          */ (window, event_window_forward<CTS("on_cursor_pos" /*           */)>);
      glfwSetCursorEnterCallback /*        */ (window, event_window_forward<CTS("on_cursor_enter" /*         */)>);
      glfwSetScrollCallback /*             */ (window, event_window_forward<CTS("on_scroll" /*               */)>);
      glfwSetDropCallback /*               */ (window, event_window_forward<CTS("on_drop" /*                 */)>);
      return 0;
    }
    static auto open_lib /*    */ (lua_State *L) -> int // fun()
    {
      luaL_Reg static constexpr event[]{
          {"window_init", event::window_init},
          {0, 0}};
      if (lua_getglobal(L, "game") not_eq LUA_TTABLE)
        return 0;
      luaL_newlib(L, event);
      lua_setfield(L, -2, "event");
      lua_pop(L, 1);
      return 0;
    }
  }
  namespace game::input
  {
    auto static inline constexpr keys = std::array{std::pair{(int16_t)GLFW_KEY_UNKNOWN, "UNKNOWN"}, std::pair{(int16_t)GLFW_KEY_SPACE, "SPACE"}, std::pair{(int16_t)GLFW_KEY_APOSTROPHE, "APOSTROPHE"}, std::pair{(int16_t)GLFW_KEY_COMMA, "COMMA"}, std::pair{(int16_t)GLFW_KEY_MINUS, "MINUS"}, std::pair{(int16_t)GLFW_KEY_PERIOD, "PERIOD"}, std::pair{(int16_t)GLFW_KEY_SLASH, "SLASH"}, std::pair{(int16_t)GLFW_KEY_0, "0"}, std::pair{(int16_t)GLFW_KEY_1, "1"}, std::pair{(int16_t)GLFW_KEY_2, "2"}, std::pair{(int16_t)GLFW_KEY_3, "3"}, std::pair{(int16_t)GLFW_KEY_4, "4"}, std::pair{(int16_t)GLFW_KEY_5, "5"}, std::pair{(int16_t)GLFW_KEY_6, "6"}, std::pair{(int16_t)GLFW_KEY_7, "7"}, std::pair{(int16_t)GLFW_KEY_8, "8"}, std::pair{(int16_t)GLFW_KEY_9, "9"}, std::pair{(int16_t)GLFW_KEY_SEMICOLON, "SEMICOLON"}, std::pair{(int16_t)GLFW_KEY_EQUAL, "EQUAL"}, std::pair{(int16_t)GLFW_KEY_A, "A"}, std::pair{(int16_t)GLFW_KEY_B, "B"}, std::pair{(int16_t)GLFW_KEY_C, "C"}, std::pair{(int16_t)GLFW_KEY_D, "D"}, std::pair{(int16_t)GLFW_KEY_E, "E"}, std::pair{(int16_t)GLFW_KEY_F, "F"}, std::pair{(int16_t)GLFW_KEY_G, "G"}, std::pair{(int16_t)GLFW_KEY_H, "H"}, std::pair{(int16_t)GLFW_KEY_I, "I"}, std::pair{(int16_t)GLFW_KEY_J, "J"}, std::pair{(int16_t)GLFW_KEY_K, "K"}, std::pair{(int16_t)GLFW_KEY_L, "L"}, std::pair{(int16_t)GLFW_KEY_M, "M"}, std::pair{(int16_t)GLFW_KEY_N, "N"}, std::pair{(int16_t)GLFW_KEY_O, "O"}, std::pair{(int16_t)GLFW_KEY_P, "P"}, std::pair{(int16_t)GLFW_KEY_Q, "Q"}, std::pair{(int16_t)GLFW_KEY_R, "R"}, std::pair{(int16_t)GLFW_KEY_S, "S"}, std::pair{(int16_t)GLFW_KEY_T, "T"}, std::pair{(int16_t)GLFW_KEY_U, "U"}, std::pair{(int16_t)GLFW_KEY_V, "V"}, std::pair{(int16_t)GLFW_KEY_W, "W"}, std::pair{(int16_t)GLFW_KEY_X, "X"}, std::pair{(int16_t)GLFW_KEY_Y, "Y"}, std::pair{(int16_t)GLFW_KEY_Z, "Z"}, std::pair{(int16_t)GLFW_KEY_LEFT_BRACKET, "LEFT_BRACKET"}, std::pair{(int16_t)GLFW_KEY_BACKSLASH, "BACKSLASH"}, std::pair{(int16_t)GLFW_KEY_RIGHT_BRACKET, "RIGHT_BRACKET"}, std::pair{(int16_t)GLFW_KEY_GRAVE_ACCENT, "GRAVE_ACCENT"}, std::pair{(int16_t)GLFW_KEY_WORLD_1, "WORLD_1"}, std::pair{(int16_t)GLFW_KEY_WORLD_2, "WORLD_2"}, std::pair{(int16_t)GLFW_KEY_ESCAPE, "ESCAPE"}, std::pair{(int16_t)GLFW_KEY_ENTER, "ENTER"}, std::pair{(int16_t)GLFW_KEY_TAB, "TAB"}, std::pair{(int16_t)GLFW_KEY_BACKSPACE, "BACKSPACE"}, std::pair{(int16_t)GLFW_KEY_INSERT, "INSERT"}, std::pair{(int16_t)GLFW_KEY_DELETE, "DELETE"}, std::pair{(int16_t)GLFW_KEY_RIGHT, "RIGHT"}, std::pair{(int16_t)GLFW_KEY_LEFT, "LEFT"}, std::pair{(int16_t)GLFW_KEY_DOWN, "DOWN"}, std::pair{(int16_t)GLFW_KEY_UP, "UP"}, std::pair{(int16_t)GLFW_KEY_PAGE_UP, "PAGE_UP"}, std::pair{(int16_t)GLFW_KEY_PAGE_DOWN, "PAGE_DOWN"}, std::pair{(int16_t)GLFW_KEY_HOME, "HOME"}, std::pair{(int16_t)GLFW_KEY_END, "END"}, std::pair{(int16_t)GLFW_KEY_CAPS_LOCK, "CAPS_LOCK"}, std::pair{(int16_t)GLFW_KEY_SCROLL_LOCK, "SCROLL_LOCK"}, std::pair{(int16_t)GLFW_KEY_NUM_LOCK, "NUM_LOCK"}, std::pair{(int16_t)GLFW_KEY_PRINT_SCREEN, "PRINT_SCREEN"}, std::pair{(int16_t)GLFW_KEY_PAUSE, "PAUSE"}, std::pair{(int16_t)GLFW_KEY_F1, "F1"}, std::pair{(int16_t)GLFW_KEY_F2, "F2"}, std::pair{(int16_t)GLFW_KEY_F3, "F3"}, std::pair{(int16_t)GLFW_KEY_F4, "F4"}, std::pair{(int16_t)GLFW_KEY_F5, "F5"}, std::pair{(int16_t)GLFW_KEY_F6, "F6"}, std::pair{(int16_t)GLFW_KEY_F7, "F7"}, std::pair{(int16_t)GLFW_KEY_F8, "F8"}, std::pair{(int16_t)GLFW_KEY_F9, "F9"}, std::pair{(int16_t)GLFW_KEY_F10, "F10"}, std::pair{(int16_t)GLFW_KEY_F11, "F11"}, std::pair{(int16_t)GLFW_KEY_F12, "F12"}, std::pair{(int16_t)GLFW_KEY_F13, "F13"}, std::pair{(int16_t)GLFW_KEY_F14, "F14"}, std::pair{(int16_t)GLFW_KEY_F15, "F15"}, std::pair{(int16_t)GLFW_KEY_F16, "F16"}, std::pair{(int16_t)GLFW_KEY_F17, "F17"}, std::pair{(int16_t)GLFW_KEY_F18, "F18"}, std::pair{(int16_t)GLFW_KEY_F19, "F19"}, std::pair{(int16_t)GLFW_KEY_F20, "F20"}, std::pair{(int16_t)GLFW_KEY_F21, "F21"}, std::pair{(int16_t)GLFW_KEY_F22, "F22"}, std::pair{(int16_t)GLFW_KEY_F23, "F23"}, std::pair{(int16_t)GLFW_KEY_F24, "F24"}, std::pair{(int16_t)GLFW_KEY_F25, "F25"}, std::pair{(int16_t)GLFW_KEY_KP_0, "KP_0"}, std::pair{(int16_t)GLFW_KEY_KP_1, "KP_1"}, std::pair{(int16_t)GLFW_KEY_KP_2, "KP_2"}, std::pair{(int16_t)GLFW_KEY_KP_3, "KP_3"}, std::pair{(int16_t)GLFW_KEY_KP_4, "KP_4"}, std::pair{(int16_t)GLFW_KEY_KP_5, "KP_5"}, std::pair{(int16_t)GLFW_KEY_KP_6, "KP_6"}, std::pair{(int16_t)GLFW_KEY_KP_7, "KP_7"}, std::pair{(int16_t)GLFW_KEY_KP_8, "KP_8"}, std::pair{(int16_t)GLFW_KEY_KP_9, "KP_9"}, std::pair{(int16_t)GLFW_KEY_KP_DECIMAL, "KP_DECIMAL"}, std::pair{(int16_t)GLFW_KEY_KP_DIVIDE, "KP_DIVIDE"}, std::pair{(int16_t)GLFW_KEY_KP_MULTIPLY, "KP_MULTIPLY"}, std::pair{(int16_t)GLFW_KEY_KP_SUBTRACT, "KP_SUBTRACT"}, std::pair{(int16_t)GLFW_KEY_KP_ADD, "KP_ADD"}, std::pair{(int16_t)GLFW_KEY_KP_ENTER, "KP_ENTER"}, std::pair{(int16_t)GLFW_KEY_KP_EQUAL, "KP_EQUAL"}, std::pair{(int16_t)GLFW_KEY_LEFT_SHIFT, "LEFT_SHIFT"}, std::pair{(int16_t)GLFW_KEY_LEFT_CONTROL, "LEFT_CONTROL"}, std::pair{(int16_t)GLFW_KEY_LEFT_ALT, "LEFT_ALT"}, std::pair{(int16_t)GLFW_KEY_LEFT_SUPER, "LEFT_SUPER"}, std::pair{(int16_t)GLFW_KEY_RIGHT_SHIFT, "RIGHT_SHIFT"}, std::pair{(int16_t)GLFW_KEY_RIGHT_CONTROL, "RIGHT_CONTROL"}, std::pair{(int16_t)GLFW_KEY_RIGHT_ALT, "RIGHT_ALT"}, std::pair{(int16_t)GLFW_KEY_RIGHT_SUPER, "RIGHT_SUPER"}, std::pair{(int16_t)GLFW_KEY_MENU, "MENU"}, std::pair{(int16_t)GLFW_KEY_LAST, "LAST"}};
    auto static inline constexpr mouse_buttons = std::array{std::pair{(int8_t)GLFW_MOUSE_BUTTON_1, "1"}, std::pair{(int8_t)GLFW_MOUSE_BUTTON_2, "2"}, std::pair{(int8_t)GLFW_MOUSE_BUTTON_3, "3"}, std::pair{(int8_t)GLFW_MOUSE_BUTTON_4, "4"}, std::pair{(int8_t)GLFW_MOUSE_BUTTON_5, "5"}, std::pair{(int8_t)GLFW_MOUSE_BUTTON_6, "6"}, std::pair{(int8_t)GLFW_MOUSE_BUTTON_7, "7"}, std::pair{(int8_t)GLFW_MOUSE_BUTTON_8, "8"}, std::pair{(int8_t)GLFW_MOUSE_BUTTON_LAST, "LAST"}, std::pair{(int8_t)GLFW_MOUSE_BUTTON_LEFT, "LEFT"}, std::pair{(int8_t)GLFW_MOUSE_BUTTON_RIGHT, "RIGHT"}, std::pair{(int8_t)GLFW_MOUSE_BUTTON_MIDDLE, "MIDDLE"}};
    auto static inline constexpr actions = std::array{std::pair{(int8_t)GLFW_RELEASE, "RELEASE"}, std::pair{(int8_t)GLFW_PRESS, "PRESS"}, std::pair{(int8_t)GLFW_REPEAT, "REPEAT"}};
    static auto open_lib(lua_State *L) -> int // fun()
    {
      if (lua_getglobal(L, "game") not_eq LUA_TTABLE)
        return 0;
      lua_createtable(L, 0, 3);
      { // game.input.key
        lua_createtable(L, 0, (int)keys.size());
        for (auto [key, name] : keys)
          lua_pushinteger(L, key), lua_setfield(L, -2, name);
        lua_setfield(L, -2, "key");
      }
      { // game.input.mouse_button
        lua_createtable(L, 0, (int)mouse_buttons.size());
        for (auto [mouse_button, name] : mouse_buttons)
          lua_pushinteger(L, mouse_button), lua_setfield(L, -2, name);
        lua_setfield(L, -2, "mouse_button");
      }
      { // game.input.mb = game.input.mouse_buttons
        lua_getfield(L, -1, "mouse_button");
        lua_setfield(L, -2, "mb");
      }
      { // game.input.action
        lua_createtable(L, 0, (int)actions.size());
        for (auto [action, name] : actions)
          lua_pushinteger(L, action), lua_setfield(L, -2, name);
        lua_setfield(L, -2, "action");
      }
      lua_setfield(L, -2, "input");
      lua_pop(L, 1);
      return 0;
    }
  }
  namespace game
  {
    static auto viewport /*     */ (lua_State *L) -> int // fun(x: integer, y: integer, width: integer, height: integer)
    {
      auto const argc = helper::expect_arg_count(L, 4);
      glViewport(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2), luaL_checkinteger(L, 3), luaL_checkinteger(L, 4));
      return 0;
    }
    static auto camera /*       */ (lua_State *L) -> int // fun(left: number, right: number, bottom: number, top: number)
    {
      auto const argc = helper::expect_arg_count(L, 4);
      render::tile::global_renderer->projection = glm::ortho<float>(
          luaL_checknumber(L, 1), luaL_checknumber(L, 2),
          luaL_checknumber(L, 3), luaL_checknumber(L, 4));
      return 0;
    }
    static auto tick_rate /*    */ (lua_State *L) -> int // fun(dt?: number): number
    {
      auto const argc = lua_gettop(L);
      if (argc)
      {
        helper::expect_arg_count(L, 1);
        update_ticker.dt = luaL_checknumber(L, 1);
      }
      lua_pushnumber(L, update_ticker.dt);
      return 1;
    }
    static auto prep_tilemap /* */ (lua_State *L) -> int // fun(map: tiled.map)
    {
      using namespace render;
      auto static constexpr func_name = "function game.prep_tilemap(map: tiled.map)";
      auto static constexpr arg0_name = "map";
      auto const argc = helper::expect_arg_count(L, 1);
      luaL_checktype(L, 1, LUA_TTABLE);
      try // switch to using exceptions and objects with raii
      {
        enum class type_t /* clang-format off */ {}; /* clang-format on */
        auto &renderer /*   */ = tile::global_renderer;
        renderer->bind_vao();
        renderer->use_program();
        if (not renderer)
          throw std::runtime_error("Global tile renderer required");
        auto static constexpr error = [](lua_State *L, char const *message)
        {
          char buf[256];
          auto const mid = buf + std::size(buf) / 2;
          auto const end = buf + std::size(buf);
          auto const argc = lua_gettop(L);
          auto it = mid;
          for (auto i = 2; i <= argc; i += 2)
          {
            auto isnum = 0;
            auto num = lua_tointegerx(L, i, &isnum);
            if (isnum)
              it += std::snprintf(it, end - it, "[%d]", (int)num);
            else
              it += std::snprintf(it, end - it, ".%s", lua_tolstring(L, i, 0));
          }
          it = buf;
          it += std::snprintf(it, end - it, "\n..in %s:\n....%s%s: %s = %s\n......%s",
                              func_name, arg0_name, mid, luaL_typename(L, -1), lua_tolstring(L, -1, 0), message);
          throw std::runtime_error{buf};
        };
        auto const field = [&]<typename FT, typename T>(FT &&field, [[maybe_unused]] T v, bool pop)
        {
          if constexpr (std::convertible_to<FT, std::string_view>)
          {
            auto const view = std::string_view{field};
            lua_pushlstring(L, view.data(), view.size());
          }
          else if (std::convertible_to<FT, lua_Integer>)
          {
            lua_pushinteger(L, (lua_Integer)field);
          }
          lua_pushvalue(L, -1);
          auto const type = lua_gettable(L, -3);
          if constexpr (std::same_as<T, type_t>)
          {
            auto const expected_type = (int)v;
            if (type not_eq expected_type)
            {
              char buf[32];
              std::snprintf(buf, std::size(buf), "was not a %s", lua_typename(L, expected_type));
              error(L, buf);
            }
            if (pop)
              lua_pop(L, 2);
            return type;
          }
          else if constexpr (std::integral<T> or std::floating_point<T>)
          {
            auto isnum = 0;
            auto const [lua_to, message] = []
            {
              if constexpr (std::integral<T>)
                return std::pair{&lua_tointegerx, "was not an integer"};
              else
                return std::pair{&lua_tonumberx, "was not a number"};
            }();
            auto num = lua_to(L, -1, &isnum);
            if (not isnum)
              error(L, message);
            if (pop)
              lua_pop(L, 2);
            return (T)num;
          }
          else if constexpr (std::convertible_to<T, std::string_view>)
          {
            auto len = size_t{0};
            auto str = lua_tolstring(L, -1, &len);
            if (not str)
              error(L, "was not a string");
            if (pop)
              lua_pop(L, 2);
            return std::string_view{str, len};
          }
          else
            static_assert(false, "Unsupported field type");
        };
        auto const map_tilewidth /*  */ = field("tilewidth"sv /*  */, 0u, true);
        auto const map_tileheight /* */ = field("tileheight"sv /* */, 0u, true);
        { // Tilesets
          auto image_paths /* */ = std::vector<std::string>{};
          auto tilesets /*    */ = std::vector<render::tile::tileset>{};
          field("tilesets"sv, (type_t)LUA_TTABLE, false);
          { // pre-allocate vectors
            auto const tilesets_len = luaL_len(L, -1);
            tilesets.reserve(tilesets_len);
            image_paths.reserve(tilesets_len);
          }
          for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) // for (tileset in map.tilesets)
          {
            auto const firstgid /*   */ = field("firstgid"sv /*   */, 0u, true);
            auto const tilecount /*  */ = field("tilecount"sv /*  */, 0u, true);
            auto const columns /*    */ = field("columns"sv /*    */, 0u, true);
            auto const tilewidth /*  */ = field("tilewidth"sv /*  */, 0u, true);
            auto const tileheight /* */ = field("tileheight"sv /* */, 0u, true);
            auto const image /*      */ = field("image"sv /*      */, "", true);
            auto const size /*       */ = vec2{tilewidth, tileheight} / vec2{map_tilewidth, map_tileheight};
            auto const offset /*     */ = vec2{0.f, 1.f - size.y};
            auto const tex = [&]
            {
              auto tex = 0u;
              for (; tex < image_paths.size(); tex++)
                if (image_paths.at(tex) == image)
                  return tex;
              image_paths.emplace_back(image);
              return tex;
            }();
            tilesets.push_back(render::tile::tileset{
                .first /*   */ = firstgid,
                .last /*    */ = firstgid + tilecount - 1,
                .columns /* */ = columns,
                .rows /*    */ = tilecount / columns,
                .tex /*     */ = tex,
                .offset /*  */ = offset,
                .size /*    */ = size,
            });
          }
          lua_pop(L, 2);
          renderer->reload_textures(std::move(image_paths));
          renderer->upload(tilesets);
        }
        { // Layers
          auto chunks /* */ = std::vector<render::tile::chunk>{};
          auto tiles /*  */ = std::vector<render::tile::tile>{};
          field("layers", (type_t)LUA_TTABLE, false);
          { // pre-allocate vectors *and* verify `len(layer[i].chunks[j].data)` uniformity
            auto expected_data_len = 0u;
            auto tiles_allocate_reserve = size_t{0};
            auto chunks_allocate_reserve = size_t{0};
            for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) // for (layer in map.layers)
            {
              auto const layer_type = field("type", "", true);
              if (layer_type not_eq "tilelayer"sv)
                continue;
              field("chunks", (type_t)LUA_TTABLE, false);
              auto const chunks_len = luaL_len(L, -1);
              chunks_allocate_reserve += chunks_len;
              for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) // for (chunk in map.layers[i].chunks)
              {
                auto const chunk_width /*  */ = field("width" /*  */, 0u, true);
                auto const chunk_height /* */ = field("height" /* */, 0u, true);
                auto const chunk_wh /*     */ = chunk_width * chunk_height;
                if (expected_data_len == 0u)
                  expected_data_len = chunk_wh;
                if (expected_data_len not_eq chunk_wh)
                  error(L, "had an irregular width * height");
                field("data", (type_t)LUA_TTABLE, false);
                auto const data_len = luaL_len(L, -1);
                if (expected_data_len not_eq data_len)
                  error(L, "had an irregular length");
                tiles_allocate_reserve += data_len;
                lua_pop(L, 2);
              }
              lua_pop(L, 2);
            }
            tiles.reserve(tiles_allocate_reserve);
            chunks.reserve(chunks_allocate_reserve);
            renderer->chunk_len = expected_data_len;
          }
          for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) // for (layer in map.layers)
          {
            auto const layer_type = field("type", "", true);
            if (layer_type not_eq "tilelayer"sv)
              continue;
            field("chunks", (type_t)LUA_TTABLE, false);
            for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) // for (chunk in map.layers[i].chunks)
            {
              auto const chunk_x /*      */ = field("x" /*      */, 0u, true);
              auto const chunk_y /*      */ = field("y" /*      */, 0u, true);
              auto const chunk_width /*  */ = field("width" /*  */, 0u, true);
              auto const chunk_height /* */ = field("height" /* */, 0u, true);

              chunks.push_back(render::tile::chunk{
                  .pos /*  */ {chunk_x, chunk_y},
                  .size /* */ {chunk_width, chunk_height},
              });

              field("data", (type_t)LUA_TTABLE, false);
              for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) // for (tile in map.layers[i].chunks[j].data)
              {
                auto isnum = 0;
                auto tile = lua_tointegerx(L, -1, &isnum);
                if (not isnum)
                  error(L, "was not an integer");
                tiles.push_back((uint)tile);
              }
              lua_pop(L, 2);
            }
            lua_pop(L, 2);
          }
          lua_pop(L, 2);
          renderer->upload(chunks);
          renderer->upload(tiles);
          renderer->tile_count = (GLsizei)tiles.size();
        }

        lua_settop(L, argc);
        return 0;
      }
      catch (std::exception const &e)
      {
        lua_settop(L, argc);
        lua_pushstring(L, e.what());
      }
      lua_error(L);
      [[unreachable]] throw;
    }
    static auto draw_tiles /*   */ (lua_State *L) -> int // fun()
    {
      helper::expect_arg_count(L, 0);
      try
      {
        render::tile::global_renderer->draw();
        return 0;
      }
      catch (std::exception const &e)
      {
        lua_pushstring(L, e.what());
      }
      lua_error(L);
      [[unreachable]] throw;
    }
    static auto open_lib /*     */ (lua_State *L) -> int // fun()
    {
      luaL_Reg static constexpr game[]{
          {"viewport" /*          */, viewport /*          */},
          {"camera" /*            */, camera /*            */},
          {"tick_rate" /*         */, tick_rate /*         */},
          {"prep_tilemap" /*      */, prep_tilemap /*      */},
          {"draw_tiles" /*        */, draw_tiles /*        */},
          {"event", 0},
          {"input", 0},
          {0, 0}};
      luaL_newlib(L, game);
      lua_setglobal(L, "game");
      event::open_lib(L);
      input::open_lib(L);
      return 0;
    }
  }
  static auto init()
  {
    L = {ASSERT(luaL_newstate()), lua_close};
    luaL_openlibs(L);
    game::open_lib(L);
    game::event::window_init(L);
  }
}

static inline void setup()
{
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glClearColor(0.1, 0.1, 0.1, 0.1);

  auto &renderer = render::tile::global_renderer.emplace();
  renderer.reload_vao();
  renderer.reload_program(utils::file_read_all("res/shaders/vert.glsl"),
                          utils::file_read_all("res/shaders/frag.glsl"));

  lua::init();
  if (auto code = "package.path = './res/scripts/?.lua;' .. package.path"; luaL_dostring(L, code) not_eq LUA_OK)
    std::fprintf(stderr, "\033[31mLua Error: %s\n\033[0m", lua_tolstring(L, -1, 0));
  if (auto path = "res/scripts/main.lua"; luaL_dofile(L, path) not_eq LUA_OK)
    std::fprintf(stderr, "\033[31mLua Error: %s\n\033[0m", lua_tolstring(L, -1, 0));
  lua_settop(L, 0);

  if (auto static constexpr global = "game", name = "setup", param = "";
      lua_getglobal(L, global) == LUA_TTABLE and lua_getfield(L, -1, name) == LUA_TFUNCTION and
      (lua_pcall(L, *param ? 1 : 0, 0, 0) not_eq LUA_OK))
    std::fprintf(stderr, "\033[31mLua Error in %s.%s(%s): %s\n\033[0m", global, name, param, lua_tolstring(L, -1, 0)), lua_pop(L, 1);
  lua_settop(L, 0);
}
static inline void update(double dt)
{
  if (auto constexpr reload_files = 0)
  {
    if (auto &renderer = render::tile::global_renderer)
      renderer->reload_program(utils::file_read_all("res/shaders/vert.glsl"),
                               utils::file_read_all("res/shaders/frag.glsl"));
    if (auto path = "res/scripts/main.lua"; luaL_dofile(L, path) not_eq LUA_OK)
      std::fprintf(stderr, "\033[31mLua Error: %s\n\033[0m", lua_tolstring(L, -1, 0));
    lua_settop(L, 0);
  }

  if (auto static constexpr global = "game", name = "update", param = "dt";
      lua_getglobal(L, global) == LUA_TTABLE and lua_getfield(L, -1, name) == LUA_TFUNCTION and
      (lua_pushnumber(L, dt), lua_pcall(L, *param ? 1 : 0, 0, 0) not_eq LUA_OK))
    std::fprintf(stderr, "\033[31mLua Error in %s.%s(%s): %s\n\033[0m", global, name, param, lua_tolstring(L, -1, 0)), lua_pop(L, 1);
  lua_settop(L, 0);
}
static inline void draw()
{
  glClear(GL_COLOR_BUFFER_BIT);

  auto &renderer = render::tile::global_renderer;

  renderer->prep_draw();
  renderer->update_uniforms();

  if (auto static constexpr global = "game", name = "draw", param = "";
      lua_getglobal(L, global) == LUA_TTABLE and lua_getfield(L, -1, name) == LUA_TFUNCTION and
      (lua_pcall(L, *param ? 1 : 0, 0, 0) not_eq LUA_OK))
    std::fprintf(stderr, "\033[31mLua Error in %s.%s(%s): %s\n\033[0m", global, name, param, lua_tolstring(L, -1, 0)), lua_pop(L, 1);
  lua_settop(L, 0);
}
static inline void loop()
{
  auto static constexpr now = std::chrono::high_resolution_clock::now;
  using duration = std::chrono::duration<double>;

  auto const before_ttl = now();
  auto const before_evt = now();
  glfwSwapInterval(1);
  glfwPollEvents();
  auto const after_evt = now();

  auto const before_upd = now();
  for (auto i = size_t{0}; update_ticker.next(); i++)
  {
    if (i < update_ticker.max_ticks_per_frame)
      update(update_ticker.dt);
    else
      update_ticker.skip();
  }
  auto const after_upd = now();

  auto const before_frm = now();
  draw();
  auto const after_frm = now();
  auto const after_ttl = now();

  if (auto constexpr print_frame_timings = 1)
  {
    auto const evt_duration = duration(after_evt - before_evt).count();
    auto const upd_duration = duration(after_upd - before_upd).count();
    auto const frm_duration = duration(after_frm - before_frm).count();
    auto const ttl_duration = duration(after_ttl - before_ttl).count();
    auto static constinit evt_time = 0.0, upd_time = 0.0, frm_time = 0.0, ttl_time = 0.0;
    auto static constexpr count = 120;
    (evt_time *= count - 1), (evt_time += evt_duration), (evt_time /= count);
    (upd_time *= count - 1), (upd_time += upd_duration), (upd_time /= count);
    (frm_time *= count - 1), (frm_time += frm_duration), (frm_time /= count);
    (ttl_time *= count - 1), (ttl_time += ttl_duration), (ttl_time /= count);
    std::printf("\033[s\033[47m\033[30m"
                "\033[1;80H  event time:%5.1lfms "
                "\033[2;80H update time:%5.1lfms "
                "\033[3;80H  frame time:%5.1lfms "
                "\033[4;80H  total time:%5.1lfms "
                "\033[u\033[0m",
                evt_time * 1'000.0,
                frm_time * 1'000.0,
                upd_time * 1'000.0,
                ttl_time * 1'000.0);
  }

  glfwSwapBuffers(window);
}
static inline void shutdown()
{
  if (auto static constexpr global = "game", name = "shutdown", param = "";
      lua_getglobal(L, global) == LUA_TTABLE and lua_getfield(L, -1, name) == LUA_TFUNCTION and
      (lua_pcall(L, *param ? 1 : 0, 0, 0) not_eq LUA_OK))
    std::fprintf(stderr, "\033[31mLua Error in %s.%s(%s): %s\n\033[0m", global, name, param, lua_tolstring(L, -1, 0)), lua_pop(L, 1);
  lua_settop(L, 0);
  L = {};
  render::tile::global_renderer = {};
}

int main()
{
  setup();
#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop(loop, 0, true);
#else
  while (not glfwWindowShouldClose(window))
    loop();
#endif
  shutdown();
}