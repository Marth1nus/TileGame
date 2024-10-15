#include <cstdio>

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

#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stb_image.h>

#undef assert
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <GLES3/gl3.h>
#include <GLFW/glfw3.h>
#else
#include <glad/gles2.h>
#include <GLFW/glfw3.h>
#endif

using glm::vec2, glm::uint,
    std::literals::operator""sv,
    std::literals::operator""s;

namespace utils
{
  struct failed_assert : std::runtime_error
  {
    using std::runtime_error::runtime_error;
  };

  auto inline static constexpr assert(auto &&value, auto &&msg) -> decltype(value)
  {
    if (value) [[likely]]
      return std::forward<decltype(value)>(value);
    throw failed_assert(msg);
  }

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
      std::fprintf(stderr, "OpenGL ES error 0x%x: %s\n", err, str);
    }
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

  template <typename T, typename VT>
  concept sized_range_value_convertible_to = std::ranges::sized_range<T> and std::convertible_to<std::ranges::range_value_t<T>, VT>;
}
#define ASSERT(...) utils::assert((__VA_ARGS__), #__VA_ARGS__)
using utils::shared, utils::glCheckError;

static auto texture_slot_count = 16;

static auto apply_glsl_format_variables(std::span<char> glsl)
{
  while (true)
  {
    char buf[] = "{texture_slot_count}";
    auto offset = std::string_view{glsl.data(), glsl.size()}.find(buf);
    if (offset == std::string_view::npos)
      break;
    std::snprintf(buf, std::size(buf), "%*d", (int)std::size(buf) - 1, texture_slot_count);
    std::copy(buf, buf + std::size(buf) - 1, glsl.data() + offset);
    glsl = glsl.subspan(offset + std::size(buf) - 1);
  }
}
[[nodiscard("Returns the file contents with formatted variables")]]
static auto load_glsl_from_file(char const *filepath)
{
  auto res = utils::file_read_all(filepath);
  apply_glsl_format_variables(res);
#ifdef __EMSCRIPTEN__
  auto find = "vec4 tex_color = texture(textures[tex], uv);"sv;
  if (auto offset = res.find(find); offset not_eq std::string::npos)
  {
    auto glsl = res.substr(0, offset);
    glsl += "vec4 tex_color; switch (tex) { ";
    for (auto i = 0; i < texture_slot_count; i++)
    {
      char buf[96];
      std::snprintf(buf, std::size(buf), "\tcase %4du: tex_color = texture(textures[%4du], uv); break; ", i, i);
      glsl += buf;
    }
    glsl += "}";
    glsl += std::string_view{res}.substr(offset + find.size());
    res = std::move(glsl);
  }
#else
#endif
  return res;
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
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  auto window = shared{glfwCreateWindow(width, height, title, 0, 0), glfwDestroyWindow};
  glfwMakeContextCurrent(ASSERT(window));
#ifdef __EMSCRIPTEN__
#else
  ASSERT(gladLoadGLES2(glfwGetProcAddress));
#endif
  glViewport(0, 0, width, height);
  glCheckError();

  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &texture_slot_count);
  return window;
}();

struct instance
{
  vec2 pos{0, 0}, size{1, 1}, uv_pos{0, 0}, uv_size{1, 1};
  uint tex{0};
};
struct tile_set
{
  uint first, last, columns, rows,
      tex, padding[3];
};
static auto vao = GLuint{};
static auto vbos = std::array<GLuint, 4>{};
static auto const &[vertices_vbo, instances_vbo, tiles_vbo, tile_sets_ubo] = vbos;
static auto constexpr vertices = std::array{vec2{0, 0}, vec2{0, 1}, vec2{1, 0}, vec2{1, 1}};
static auto instances = std::vector<instance>{};
static auto tiles = std::vector<uint32_t>{};
static auto tile_sets = std::vector<tile_set>{};

static inline auto vao_init()
{
  glGenVertexArrays(1, &vao);
  glGenBuffers((GLsizei)vbos.size(), vbos.data());

  glBindVertexArray(vao);
  auto bytes = std::span<std::byte const>{};
  auto i = 0;

  bytes = std::as_bytes(std::span(vertices));
  glBindBuffer(GL_ARRAY_BUFFER, vertices_vbo);
  glBufferData(GL_ARRAY_BUFFER, bytes.size(), bytes.data(), GL_STATIC_DRAW);
  glCheckError();

  glVertexAttribPointer(i, 2, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(vec2), (void *)0);
  glEnableVertexAttribArray(i++);
  glCheckError();

  bytes = std::as_bytes(std::span(instances));
  glBindBuffer(GL_ARRAY_BUFFER, instances_vbo);
  glBufferData(GL_ARRAY_BUFFER, bytes.size(), bytes.data(), GL_DYNAMIC_DRAW);
  glCheckError();

  for (auto offset : {offsetof(instance, pos),
                      offsetof(instance, size),
                      offsetof(instance, uv_pos),
                      offsetof(instance, uv_size)})
  {
    glVertexAttribPointer(i, 2, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(instance), (void *)offset);
    glVertexAttribDivisor(i, 1);
    glEnableVertexAttribArray(i++);
    glCheckError();
  }
  glVertexAttribIPointer(i, 1, GL_UNSIGNED_INT, (GLsizei)sizeof(instance), (void *)offsetof(instance, tex));
  glVertexAttribDivisor(i, 1);
  glEnableVertexAttribArray(i++);
  glCheckError();

  bytes = std::as_bytes(std::span(tiles));
  glBindBuffer(GL_ARRAY_BUFFER, tiles_vbo);
  glBufferData(GL_ARRAY_BUFFER, bytes.size(), bytes.data(), GL_DYNAMIC_DRAW);
  glCheckError();

  glVertexAttribIPointer(i, 1, GL_UNSIGNED_INT, sizeof(tiles.at(0)), (void *)0);
  glVertexAttribDivisor(i, 1);
  glEnableVertexAttribArray(i++);
  glCheckError();

  glBindBuffer(GL_UNIFORM_BUFFER, tile_sets_ubo);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, tile_sets_ubo);
  glCheckError();
}
static inline auto instances_upload()
{
  auto static constinit capacity = size_t(0);
  glBindBuffer(GL_ARRAY_BUFFER, instances_vbo);
  if (capacity not_eq instances.capacity())
    glBufferData(GL_ARRAY_BUFFER, (capacity = instances.capacity()) * sizeof(instances.at(0)), nullptr, GL_DYNAMIC_DRAW);
  if (auto bytes = std::as_bytes(std::span(instances)); not bytes.empty())
    glBufferSubData(GL_ARRAY_BUFFER, 0, bytes.size(), bytes.data());
  glCheckError();
}
static inline auto tiles_upload()
{
  auto static constinit capacity = size_t(0);
  glBindBuffer(GL_ARRAY_BUFFER, tiles_vbo);
  if (capacity not_eq tiles.capacity())
    glBufferData(GL_ARRAY_BUFFER, GLsizei((capacity = tiles.capacity()) * sizeof(tiles.at(0))), nullptr, GL_DYNAMIC_DRAW);
  if (auto bytes = std::as_bytes(std::span(tiles)); not bytes.empty())
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizei)bytes.size(), bytes.data());
  glCheckError();
}
static inline auto tile_sets_upload()
{
  auto static constinit capacity = size_t(0);
  glBindBuffer(GL_UNIFORM_BUFFER, tile_sets_ubo);
  if (capacity not_eq tile_sets.capacity())
    glBufferData(GL_UNIFORM_BUFFER, (capacity = tile_sets.capacity()) * sizeof(tile_sets.at(0)), nullptr, GL_DYNAMIC_READ);
  if (auto bytes = std::as_bytes(std::span(tile_sets)); not bytes.empty())
    glBufferSubData(GL_UNIFORM_BUFFER, 0, bytes.size(), bytes.data());
  glCheckError();
}

static GLuint vid, fid, pid;
static struct
{
  GLint projection,
      use_tiles,
      TILE_SETS,
      columns,
      textures;
} uniform{};
auto projection = glm::ortho<float>(0, 32, 0, 32);

[[nodiscard("Return is a new shader handle (Manual deletion required)")]]
static auto make_shader(GLenum type, std::string_view glsl)
{
  auto sid = glCreateShader(type);
  auto off = glsl.find("#version");
  off = off == std::string_view::npos ? 0 : off;
  auto str = glsl.data() + off;
  auto len = GLsizei(glsl.size() - off);
  glShaderSource(sid, 1, &str, &len);
  glCompileShader(sid);
  if (GLint status, len; glGetShaderiv(sid, GL_COMPILE_STATUS, &status), not status)
  {
    std::string log;
    log.resize((glGetShaderiv(sid, GL_INFO_LOG_LENGTH, &len), len));
    log.resize((glGetShaderInfoLog(sid, len, &len, log.data()), len));
    std::fprintf(stderr, "%s Shader Error: %s", type == GL_VERTEX_SHADER ? "Vertex" : "Fragment", log.c_str());
    glDeleteShader(sid), sid = 0;
  }
  glCheckError();
  return sid;
}
static auto pid_init(std::string_view vert_glsl, std::string_view frag_glsl)
{
  (void)(pid and (glDeleteProgram(pid), 1));
  (void)(vid and (glDeleteShader(vid), 1));
  (void)(fid and (glDeleteShader(fid), 1));
  pid = glCreateProgram();
  vid = make_shader(GL_VERTEX_SHADER, vert_glsl);
  fid = make_shader(GL_FRAGMENT_SHADER, frag_glsl);
  glAttachShader(pid, vid);
  glAttachShader(pid, fid);
  glLinkProgram(pid);
  if (GLint status, len; glGetProgramiv(pid, GL_LINK_STATUS, &status), not status)
  {
    std::string log;
    log.resize((glGetProgramiv(pid, GL_INFO_LOG_LENGTH, &len), len));
    log.resize((glGetProgramInfoLog(pid, len, &len, log.data()), len));
    std::fprintf(stderr, "Program Error: %s", log.c_str());
    glDeleteProgram(pid), pid = 0;
    return;
  }
  glCheckError();

  glUseProgram(pid);
  uniform.projection /* */ = glGetUniformLocation(pid, "projection" /*  */);
  uniform.use_tiles /*  */ = glGetUniformLocation(pid, "use_tiles" /*   */);
  uniform.TILE_SETS /*  */ = glGetUniformBlockIndex(pid, "TILE_SETS" /* */);
  uniform.columns /*    */ = glGetUniformLocation(pid, "columns" /*     */);
  uniform.textures /*   */ = glGetUniformLocation(pid, "textures" /*    */);
  glCheckError();

  auto textures = std::vector<int>((size_t)texture_slot_count);
  std::generate(textures.begin(), textures.end(), [i = 0]() mutable
                { return i++; });

  glUniformMatrix4fv(uniform.projection, 1, 0, &projection[0][0]);
  glUniform1ui(uniform.use_tiles, true);
  glUniformBlockBinding(pid, uniform.TILE_SETS, 0);
  glUniform1ui(uniform.columns, 1);
  glUniform1iv(uniform.textures, (GLsizei)texture_slot_count, textures.data());
  glCheckError();
}

auto textures = std::vector<GLuint>{};
static auto textures_load(utils::sized_range_value_convertible_to<char const *> auto const &files)
{
  if (not textures.empty())
    glDeleteTextures((GLsizei)textures.size(), textures.data());
  textures.resize(std::min<size_t>(std::size(files), texture_slot_count));
  glGenTextures((GLsizei)textures.size(), textures.data());
  for (auto i = 0; auto &&path : files)
  {
    glBindTexture(GL_TEXTURE_2D, textures.at(i++));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    auto width = 1, height = 1, channels = 4;
    auto pixels = ASSERT(stbi_load(path, &width, &height, &channels, channels));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    stbi_image_free(pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glCheckError();
  }
}

static inline void setup()
{
  vao_init();
  pid_init(load_glsl_from_file("res/shaders/vert.glsl"),
           load_glsl_from_file("res/shaders/frag.glsl"));
  glClearColor(0.1, 0.1, 0.1, 0.1);

  textures_load(std::array{
      "res/images/gfx/cave.png",      // 0
      "res/images/gfx/character.png", // 1
      "res/images/gfx/font.png",      // 2
      "res/images/gfx/Inner.png",     // 3
      "res/images/gfx/log.png",       // 4
      "res/images/gfx/NPC_test.png",  // 5
      "res/images/gfx/objects.png",   // 6
      "res/images/gfx/Overworld.png", // 7
  });

  instances = {
      instance{.pos{04, 04}, .size{07, 07}, .uv_size{1, 1}, .tex = 0},
      instance{.pos{12, 04}, .size{16, 07}, .uv_size{2, 1}, .tex = 1},
      instance{.pos{04, 12}, .size{07, 16}, .uv_size{1, 2}, .tex = 2},
      instance{.pos{12, 12}, .size{16, 16}, .uv_size{2, 2}, .tex = 3},
  };
  instances_upload();

  tile_sets = {
      tile_set{.first = 0, .last = 40 * 36, .columns = 40, .rows = 36, .tex = 7},
  };
  tile_sets.resize(texture_slot_count * 2, tile_sets.at(0));
  tile_sets_upload();

  tiles.resize((size_t)40 * 36);
  std::generate(tiles.begin(), tiles.end(), [i = 0]() mutable
                { return i++; });
  tiles_upload();

  glCheckError();
}

static inline void loop()
{
  pid_init(load_glsl_from_file("res/shaders/vert.glsl"),
           load_glsl_from_file("res/shaders/frag.glsl"));
  glClear(GL_COLOR_BUFFER_BIT);

  glUseProgram(pid);
  glBindVertexArray(vao);

  for (auto i = (int)textures.size() - 1; i >= 0; i--)
  {
    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(GL_TEXTURE_2D, textures.at(i));
  }

  glUniformMatrix4fv(uniform.projection, 1, GL_FALSE, &projection[0][0]);

  glUniform1ui(uniform.use_tiles, true);
  glUniform1ui(uniform.columns, 40);
  glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, (GLsizei)vertices.size(), (GLsizei)tiles.size());
  glCheckError();

  glUniform1ui(uniform.use_tiles, false);
  glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, (GLsizei)vertices.size(), (GLsizei)instances.size());
  glCheckError();

  glfwSwapBuffers(window);
  glfwSwapInterval(1);
  glfwPollEvents();
}

int main()
{
#ifdef __EMSCRIPTEN__
  setup();
  emscripten_set_main_loop(loop, 0, true);
#else
  setup();
  while (not glfwWindowShouldClose(window))
    loop();
#endif
}