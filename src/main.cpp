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

#undef assert
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <GLES3/gl3.h>
#include <GLFW/glfw3.h>
#else
#include <glad/gles2.h>
#include <GLFW/glfw3.h>
#endif

auto static texture_slot_count = 16;

static char vert_glsl[]{R"glsl(
  #version 300 es
  layout(location = 0) in vec2 mesh_pos;
  layout(location = 1) in vec2 mesh_uv_pos;
  layout(location = 2) in vec2 instance_pos;
  layout(location = 3) in vec2 instance_size;
  layout(location = 4) in vec2 instance_uv_pos;
  layout(location = 5) in vec2 instance_uv_size;
  layout(location = 6) in uint instance_tex;

  uniform mat4 projection;

  out vec2 pos;
  out vec2 uv;
  flat out uint tex;

  void main()
  {
    pos = instance_pos + instance_size * mesh_pos;
    uv = instance_uv_pos + instance_uv_size * mesh_uv_pos;
    tex = instance_tex;
    gl_Position = projection * vec4(pos, 0, 1);
    gl_Position
  }
)glsl"};

static char frag_glsl[]{R"glsl(
  #version 300 es
  precision mediump float;
  precision mediump sampler2D;

  in vec2 pos;
  in vec2 uv;
  flat in uint tex;

  uniform sampler2D textures[####];

  out vec4 color;

  void main()
  {
    color = texture(textures[tex], uv);
  }
)glsl"};

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

  template <typename T>
  struct shared
  {
    std::shared_ptr<T> ptr;
    inline constexpr shared() noexcept = default;
    inline constexpr shared(shared &&) noexcept = default;
    inline constexpr shared(shared const &) noexcept = default;
    inline constexpr shared &operator=(shared &&) noexcept = default;
    inline constexpr shared &operator=(shared const &) noexcept = default;

    inline constexpr shared(std::shared_ptr<T> ptr) noexcept : ptr(std::move(ptr)) {}
    inline constexpr operator std::shared_ptr<T> const &() const noexcept { return ptr; }

    inline constexpr shared(T *ptr, auto &&deleter) noexcept : ptr(ptr, std::forward<decltype(deleter)>(deleter)) {}
    inline constexpr operator T *() const noexcept { return ptr.get(); }
  };
}
#define ASSERT(...) utils::assert((__VA_ARGS__), #__VA_ARGS__)
using utils::shared, utils::glCheckError;

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
  [&] { // replace textures array size in frag_glsl
    auto target = std::span<char>(frag_glsl);
    auto find = std::string_view{};
    auto offset = size_t{};
    // find definition
    offset = std::string_view{target}.find(find = "uniform sampler2D textures[####];");
    if (offset == std::string_view::npos or target.size() - offset < find.size())
      return;
    target = target.subspan(offset, find.size());
    // find size
    offset = std::string_view{target}.find(find = "[####]");
    if (offset == std::string_view::npos or target.size() - offset < find.size())
      return;
    target = target.subspan(offset, find.size());
    // replace size
    std::snprintf(target.data(), target.size(), "[%4d\0", texture_slot_count);
    target[find.size() - 1] = find.back();
  }();
}

static GLuint vao, vertices_vbo, instances_vbo;
struct vertex
{
  glm::vec2 pos, uv;
};
static auto constexpr vertices = std::array{
    vertex{{0, 0}, {0, 0}},
    vertex{{0, 1}, {0, 1}},
    vertex{{1, 0}, {1, 0}},
    vertex{{1, 1}, {1, 1}},
};
struct instance
{
  glm::vec2 pos, size, uv_pos, uv_size;
  glm::uint tex;
};
static auto instances = std::vector<instance>{};

static inline auto vao_init()
{
  glGenVertexArrays(1, &vao);
  GLuint buffers[]{vertices_vbo, instances_vbo};
  glGenBuffers((GLsizei)std::size(buffers), buffers);
  (vertices_vbo = buffers[0]), (instances_vbo = buffers[1]);

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

static GLuint vid, fid, pid;
static struct
{
  GLint projection, textures;
} uniform{};

[[nodiscard]]
static auto make_shader(GLenum type, std::string_view glsl)
{
  auto sid = glCreateShader(type);
  auto str = glsl.data();
  auto len = (GLsizei)glsl.size();
  glShaderSource(sid, 1, &str, &len);
  glCompileShader(sid);
  if (GLint status, len;
      glGetShaderiv(sid, GL_COMPILE_STATUS, &status), not status)
  {
    std::string log;
    log.resize((glGetShaderiv(sid, GL_INFO_LOG_LENGTH, &len), len));
    log.resize((glGetShaderInfoLog(sid, len, &len, log.data()), len));
    std::fprintf(stderr, "Shader Error: %s", log.c_str());
    glDeleteShader(sid), sid = 0;
  }
  glCheckError();
  return sid;
}
static auto pid_init(std::string_view vert_glsl = ::vert_glsl, std::string_view frag_glsl = ::frag_glsl)
{
  pid = glCreateProgram();
  vid = make_shader(GL_VERTEX_SHADER, vert_glsl);
  fid = make_shader(GL_FRAGMENT_SHADER, frag_glsl);
  glAttachShader(pid, vid);
  glAttachShader(pid, fid);
  glLinkProgram(pid);
  if (GLint status, len;
      glGetProgramiv(pid, GL_LINK_STATUS, &status), not status)
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
  uniform.projection = glGetUniformLocation(pid, "projection");
  uniform.textures = glGetUniformLocation(pid, "textures");
  glCheckError();

  if (uniform.textures >= 0)
  {
    auto textures = std::vector<int>(texture_slot_count);
    for (auto i = 0; auto &t : textures)
      t = i++;
    glUniform1iv(uniform.textures, (GLsizei)textures.size(), textures.data());
  }
  glCheckError();
}

static inline void setup()
{
  window_init();
  vao_init();
  pid_init();

  glClearColor(0.1, 0.1, 0.1, 0.1);
  glfwSwapInterval(1);

  glUseProgram(pid);
  auto projection = glm::ortho<float>(0, 8, 0, 8);
  glUniformMatrix4fv(uniform.projection, 1, GL_FALSE, &projection[0][0]);

  for (auto i = 1; i < 7; i++)
    for (auto j = 1; j < 7; j++)
      instances.push_back(instance{.pos{i, j}, .size{1, 1}, .uv_pos{0, 0}, .uv_size{1, 1}, .tex = uint32_t(i + j)});
  instances_upload();

  glCheckError();
}

static inline void loop()
{
  glClear(GL_COLOR_BUFFER_BIT);

  glUseProgram(pid);
  glBindVertexArray(vao);
  glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, (GLsizei)vertices.size(), (GLsizei)instances.size());
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