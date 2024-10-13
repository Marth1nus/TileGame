#include <print>

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
    auto offset = std::string_view{glsl}.find(buf);
    if (offset == std::string_view::npos)
      break;
    std::snprintf(buf, std::size(buf), "%*d", (int)std::size(buf) - 1, texture_slot_count);
    std::ranges::copy(std::string_view{buf}, glsl.data() + offset);
    glsl = glsl.subspan(offset + std::size(buf) - 1);
  }
}
[[nodiscard("Returns the file contents with formatted variables")]]
static auto load_glsl_from_file(char const *filepath)
{
  auto res = utils::file_read_all(filepath);
  apply_glsl_format_variables(res);
  return res;
}

static struct glfw
{
  glfw() { ASSERT(glfwInit()); }
  ~glfw() { glfwTerminate(); }
} const glfw{};

static auto window = shared<GLFWwindow>{};

static inline auto window_init(int width = 720, int height = -1, char const *title = "TileGame")
{
  height = height < 0 ? width : height;
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  window = {glfwCreateWindow(width, height, title, 0, 0), glfwDestroyWindow};
  glfwMakeContextCurrent(ASSERT(window));
#ifdef __EMSCRIPTEN__
#else
  ASSERT(gladLoadGLES2(glfwGetProcAddress));
#endif
  glViewport(0, 0, width, height);
  glCheckError();

  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &texture_slot_count);
}

static GLuint tile_sets_ubo;
struct tile_set
{
  uint first, last, columns, tex;
};
static auto tile_sets = std::vector<tile_set>{};
static inline auto tile_sets_init()
{
  glGenBuffers(1, &tile_sets_ubo);
  glBindBuffer(GL_UNIFORM_BUFFER, tile_sets_ubo);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, tile_sets_ubo);
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

static GLuint vao, vertices_vbo, instances_vbo, tiles_vbo;
struct vertex
{
  vec2 pos, uv;
};
static auto constexpr vertices = std::array{
    vertex{{0, 0}, {0, 0}},
    vertex{{0, 1}, {0, 1}},
    vertex{{1, 0}, {1, 0}},
    vertex{{1, 1}, {1, 1}},
};
struct instance
{
  vec2 pos, size, uv_pos, uv_size;
  glm::uint tex;
};
static auto instances = std::vector<instance>{};
static auto tiles = std::vector<uint32_t>{};

static inline auto vao_init()
{
  auto buffers = std::array<GLuint, 3>{};
  glGenBuffers((GLsizei)buffers.size(), buffers.data());
  vertices_vbo /*  */ = buffers.at(0);
  instances_vbo /* */ = buffers.at(1);
  tiles_vbo /*     */ = buffers.at(2);

  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  auto bytes = std::span<std::byte const>{};

  bytes = std::as_bytes(std::span(vertices));
  glBindBuffer(GL_ARRAY_BUFFER, vertices_vbo);
  glBufferData(GL_ARRAY_BUFFER, bytes.size(), bytes.data(), GL_STATIC_DRAW);
  glCheckError();

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, pos));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, uv));
  glEnableVertexAttribArray(1);
  glCheckError();

  bytes = std::as_bytes(std::span(instances));
  glBindBuffer(GL_ARRAY_BUFFER, instances_vbo);
  glBufferData(GL_ARRAY_BUFFER, bytes.size(), bytes.data(), GL_DYNAMIC_DRAW);
  glCheckError();

  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(instance), (void *)offsetof(instance, pos));
  glVertexAttribDivisor(2, 1);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(instance), (void *)offsetof(instance, size));
  glVertexAttribDivisor(3, 1);
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(instance), (void *)offsetof(instance, uv_pos));
  glVertexAttribDivisor(4, 1);
  glEnableVertexAttribArray(4);
  glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, sizeof(instance), (void *)offsetof(instance, uv_size));
  glVertexAttribDivisor(5, 1);
  glEnableVertexAttribArray(5);
  glVertexAttribPointer(6, 1, GL_UNSIGNED_INT, GL_FALSE, sizeof(instance), (void *)offsetof(instance, tex));
  glVertexAttribDivisor(6, 1);
  glEnableVertexAttribArray(6);
  glCheckError();

  bytes = std::as_bytes(std::span(tiles));
  glBindBuffer(GL_ARRAY_BUFFER, tiles_vbo);
  glBufferData(GL_ARRAY_BUFFER, bytes.size(), bytes.data(), GL_DYNAMIC_DRAW);
  glCheckError();

  glVertexAttribPointer(7, 1, GL_UNSIGNED_INT, GL_FALSE, sizeof(uint32_t), 0);
  glVertexAttribDivisor(7, 1);
  glEnableVertexAttribArray(7);
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

static GLuint vid, fid, pid;
static struct
{
  GLint projection,
      use_tiles,
      TILE_SETS,
      tile_chunk_columns,
      textures;
} uniform{};

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
  pid and (glDeleteProgram(pid), 1);
  vid and (glDeleteShader(vid), 1);
  fid and (glDeleteShader(fid), 1);
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
  uniform.projection /*         */ = glGetUniformLocation(pid, "projection" /*         */);
  uniform.use_tiles /*          */ = glGetUniformLocation(pid, "use_tiles" /*          */);
  uniform.TILE_SETS /*          */ = glGetUniformBlockIndex(pid, "TILE_SETS" /*        */);
  uniform.tile_chunk_columns /* */ = glGetUniformLocation(pid, "tile_chunk_columns" /* */);
  uniform.textures /*           */ = glGetUniformLocation(pid, "textures" /*           */);
  glCheckError();

  auto projection = glm::mat4(1);
  auto textures = std::views::iota(0, texture_slot_count) | std::ranges::to<std::vector>();

  glUniformMatrix4fv(uniform.projection, 1, 0, &projection[0][0]);
  glUniform1ui(uniform.use_tiles, true);
  glUniformBlockBinding(pid, uniform.TILE_SETS, 0);
  glUniform1ui(uniform.tile_chunk_columns, 1);
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
  window_init();
  tile_sets_init();
  vao_init();
  pid_init(load_glsl_from_file("shaders/vert.glsl"),
           load_glsl_from_file("shaders/frag.glsl"));
  glClearColor(0.1, 0.1, 0.1, 0.1);
  glfwSwapInterval(1);

  glUseProgram(pid);
  auto projection = glm::ortho<float>(0, 2, 0, 2);
  // projection = glm::ortho<float>(0, 42, 0, 38);
  glUniformMatrix4fv(uniform.projection, 1, GL_FALSE, &projection[0][0]);

  textures_load(std::array{
      "images/gfx/cave.png",      // 0
      "images/gfx/character.png", // 1
      "images/gfx/font.png",      // 2
      "images/gfx/Inner.png",     // 3
      "images/gfx/log.png",       // 4
      "images/gfx/NPC_test.png",  // 5
      "images/gfx/objects.png",   // 6
      "images/gfx/Overworld.png", // 7
  });

  auto sprite = [tex = 3](vec2 p, vec2 up, uint tex = 0) mutable
  { return instance{
        .pos = p,
        .size = vec2(1),
        .uv_pos = up / vec2(40.f, 36.f) * 0.0f + 0.0f,
        .uv_size = vec2(1) / vec2(40.f, 36.f) * 0.0f + 1.0f,
        .tex = tex++,
    }; };
  instances = {
      sprite({0, 0}, {0, 6}),
      sprite({0, 1}, {0, 7}),
      sprite({1, 0}, {1, 6}),
      sprite({1, 1}, {1, 7}),
  };
  instances_upload();

  tile_sets = {
      tile_set{.first = 0, .last = 40 * 36, .columns = 40, .tex = 7},
  };
  tile_sets_upload();

  tiles = std::views::iota(0ui32, 40 * 36ui32) | std::ranges::to<std::vector>();
  tiles_upload();

  glCheckError();
}

static inline void loop()
{
  pid_init(load_glsl_from_file("shaders/vert.glsl"),
           load_glsl_from_file("shaders/frag.glsl"));

  glClear(GL_COLOR_BUFFER_BIT);

  glUseProgram(pid);
  glBindVertexArray(vao);

  for (auto [i, tex] : textures | std::views::enumerate | std::views::reverse)
    glActiveTexture(GL_TEXTURE0 + i), glBindTexture(GL_TEXTURE_2D, tex);

  glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, (GLsizei)vertices.size(), (GLsizei)instances.size());
  glCheckError();

  glUniform1ui(uniform.use_tiles, true);
  glUniform1ui(uniform.tile_chunk_columns, 40);
  glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, (GLsizei)vertices.size(), (GLsizei)tiles.size());
  glUniform1ui(uniform.use_tiles, false);
  glCheckError();

  glfwSwapBuffers(window);
  glfwPollEvents();
}

int main()
{
#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop(loop, 0, true);
  setup();
#else
  setup();
  while (not glfwWindowShouldClose(window))
    loop();
#endif
}