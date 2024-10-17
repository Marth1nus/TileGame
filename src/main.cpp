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

#include <tuple>
#include <memory>
#include <optional>

#include <lua.hpp>
#include <stb_image.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <entt/entt.hpp>

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
  template <typename... T>
  struct overload : T...
  {
    using T::operator()...;
  };

  template <size_t N>
  struct CTS
  {
    char str[N];
  };

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
using utils::shared, utils::glCheckError, utils::overload;

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

static inline auto tiles_draw(uint columns)
{
  glUniform1ui(uniform.use_tiles, true);
  glUniform1ui(uniform.columns, columns);
  glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, (GLsizei)vertices.size(), (GLsizei)tiles.size());
  glCheckError();
}
static inline auto instances_draw()
{
  glUniform1ui(uniform.use_tiles, false);
  glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, (GLsizei)vertices.size(), (GLsizei)instances.size());
  glCheckError();
}

auto textures = std::vector<GLuint>{};
auto textures_paths = std::vector<std::string>{};
static inline auto textures_load(std::vector<std::string> files)
{
  if (not textures.empty())
    glDeleteTextures((GLsizei)textures.size(), textures.data());
  textures.resize(std::min<size_t>(std::size(files), texture_slot_count));
  if (not textures.empty())
    glGenTextures((GLsizei)textures.size(), textures.data());
  textures_paths = std::move(files);
  for (auto i = 0u; i < textures.size(); i++)
  {
    auto path = textures_paths.at(i).c_str();
    glBindTexture(GL_TEXTURE_2D, textures.at(i));
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
static inline auto textures_clear()
{
  textures_load({});
}

struct timer
{
  double stamp = glfwGetTime();
  auto elapsed() { return timer{}.stamp - stamp; }
  auto restart() { return std::exchange(*this, {}).elapsed(); }
};
struct ticker
{
  double dt = 1.0 / 30.0;
  double start_time = glfwGetTime();
  size_t tik = 0, max_ticks_per_frame = 4;
  bool next() { return tik < size_t((glfwGetTime() - start_time) / dt) ? ++tik : false; }
};
static auto update_ticker = ticker{};

static auto L = shared<lua_State>{};
namespace lua
{
  template <auto event_name>
  static auto constexpr event_window_forward(GLFWwindow *window, auto... args)
  {
    static_assert(std::string_view{event_name.str}.substr(0, 3) == "on_",
                  "event_name must start with \"on_\" prefix");
    if (not L)
      return;
    auto const top = lua_gettop(L);
    auto const name = event_name.str;
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
        std::fprintf(stderr, "LUA ERROR in event `%s`: %s\n", name, lua_tostring(L, -1)), lua_pop(L, 1);
    };
    if (lua_getglobal(L, "game") == LUA_TTABLE and
        lua_getfield(L, -1, "event") == LUA_TTABLE)
    {
      if (lua_getfield(L, -1, name) == LUA_TFUNCTION)
        call(nullptr);
      else
        lua_pop(L, 1);
      if (lua_getfield(L, -1, "on_event") == LUA_TFUNCTION)
        call(name + sizeof("on"));
    }
    lua_settop(L, top);
  }
  static auto tile_sets_clear /*   */ (lua_State *L) -> int // fun()
  {
    tile_sets.clear();
    return 0;
  }
  static auto tile_sets_add /*     */ (lua_State *L) -> int // fun(tile_set: tile_set)
  {
    if (auto argc = lua_gettop(L); argc not_eq 1)
      return luaL_error(L, "Expected 1 arg. Got %d", argc);
    auto getfield = [L](auto name, auto fn, auto... args)
    {
      lua_getfield(L, -1, name);
      auto res = fn(L, -1, args...);
      return lua_pop(L, 1), res;
    };
    auto path /*    */ = getfield("image" /*   */, luaL_checklstring /* */, nullptr);
    auto columns /* */ = getfield("columns" /* */, luaL_checkinteger /* */);
    auto rows /*    */ = getfield("rows" /*    */, luaL_optinteger /*   */, columns);
    auto count /*   */ = getfield("count" /*   */, luaL_optinteger /*   */, columns * rows);
    auto tex /*     */ = std::distance(textures_paths.begin(), std::ranges::find(textures_paths, path));
    auto first /*   */ = tile_sets.empty() ? 0 : tile_sets.back().last + 1;
    if (tex == textures_paths.size())
      return luaL_error(L, "Texture not loaded: %s", path);
    tile_sets.push_back(tile_set{
        .first /*   */ = uint(first),
        .last /*    */ = uint(first + count),
        .columns /* */ = uint(columns),
        .rows /*    */ = uint(rows),
        .tex /*     */ = uint(tex),
    });
    return 0;
  }
  static auto tile_sets_set /*     */ (lua_State *L) -> int // fun(tile_sets: tile_set[])
  {
    if (auto argc = lua_gettop(L); argc == 0)
      return tile_sets_upload(), 0;
    else if (argc not_eq 1)
      return luaL_error(L, "Expected 1 arg. Got %d", argc);
    tile_sets.clear();
    tile_sets.reserve(luaL_len(L, -1));
    for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1))
      lua_pushcclosure(L, tile_sets_add, 0), lua_pushvalue(L, -2), lua_call(L, 1, 0);
    tile_sets_upload();
    return 0;
  }
  static auto tiles_set /*         */ (lua_State *L) -> int // fun(tiles: integer[])
  {
    if (lua_gettop(L) < 1)
      luaL_error(L, "too few arguments");
    luaL_checktype(L, -1, LUA_TTABLE);
    tiles.clear();
    tiles.reserve(luaL_len(L, -1));
    for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1))
      tiles.push_back(luaL_checkinteger(L, -1));
    tiles_upload();
    return 0;
  }
  static auto instances_clear /*   */ (lua_State *L) -> int // fun()
  {
    instances.clear();
    return 0;
  }
  static auto instances_add /*     */ (lua_State *L) -> int // fun(instance: instance)
  {
    if (auto argc = lua_gettop(L); argc not_eq 1)
      return luaL_error(L, "Expected 1 arg. Got %d", argc);
    luaL_checktype(L, 1, LUA_TTABLE);
    auto inst = instance{};
    if (lua_getfield(L, -1, "pos" /*     */) == LUA_TTABLE)
    {
      lua_geti(L, -1, 1), (inst.pos /*     */.x = luaL_checknumber(L, -1)), lua_pop(L, 1);
      lua_geti(L, -1, 2), (inst.pos /*     */.y = luaL_checknumber(L, -1)), lua_pop(L, 1);
    }
    lua_pop(L, 1);
    if (lua_getfield(L, -1, "size" /*    */) == LUA_TTABLE)
    {
      lua_geti(L, -1, 1), (inst.size /*    */.x = luaL_checknumber(L, -1)), lua_pop(L, 1);
      lua_geti(L, -1, 2), (inst.size /*    */.y = luaL_checknumber(L, -1)), lua_pop(L, 1);
    }
    lua_pop(L, 1);
    if (lua_getfield(L, -1, "uv_pos" /*  */) == LUA_TTABLE)
    {
      lua_geti(L, -1, 1), (inst.uv_pos /*  */.x = luaL_checknumber(L, -1)), lua_pop(L, 1);
      lua_geti(L, -1, 2), (inst.uv_pos /*  */.y = luaL_checknumber(L, -1)), lua_pop(L, 1);
    }
    lua_pop(L, 1);
    if (lua_getfield(L, -1, "uv_size" /* */) == LUA_TTABLE)
    {
      lua_geti(L, -1, 1), (inst.uv_size /* */.x = luaL_checknumber(L, -1)), lua_pop(L, 1);
      lua_geti(L, -1, 2), (inst.uv_size /* */.y = luaL_checknumber(L, -1)), lua_pop(L, 1);
    }
    lua_pop(L, 1);
    if (lua_getfield(L, -1, "tex" /*     */) == LUA_TNUMBER)
    {
      inst.tex = luaL_checkinteger(L, -1);
    }
    lua_pop(L, 1);
    instances.push_back(inst);
    return 0;
  }
  static auto instances_set /*     */ (lua_State *L) -> int // fun(instances: instance[])
  {
    if (auto argc = lua_gettop(L); argc == 0)
      return instances_upload(), 0;
    else if (argc not_eq 1)
      return luaL_error(L, "Expected 1 arg. Got %d", argc);
    luaL_checktype(L, 1, LUA_TTABLE);
    instances.clear();
    instances.reserve(luaL_len(L, -1));
    for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1))
      lua_pushcclosure(L, instances_add, 0), lua_pushvalue(L, -2), lua_call(L, 1, 0);
    instances_upload();
    return 0;
  }
  static auto tick_rate /*         */ (lua_State *L) -> int // fun(dt: number?): number
  {
    if (auto argc = lua_gettop(L); argc == 1)
      update_ticker = {.dt = luaL_checknumber(L, 1)};
    else if (argc not_eq 0)
      return luaL_error(L, "Expected 0 or 1 args. Got %d", argc);
    lua_pushnumber(L, update_ticker.dt);
    return 1;
  }
  static auto textures_set /*      */ (lua_State *L) -> int // fun(filepaths: string[])
  {
    if (auto argc = lua_gettop(L); argc not_eq 1)
      return luaL_error(L, "Expected 1 arg. Got %d", argc);
    luaL_checktype(L, 1, LUA_TTABLE);
    try
    {
      auto paths = std::vector<std::string>{};
      paths.reserve(luaL_len(L, -1));
      for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1))
        if (auto path = luaL_tolstring(L, -1, 0); not path)
          throw std::runtime_error{lua_pushfstring(L, "paths[%i] was not a string", lua_tointegerx(L, -2, 0))};
        else if (auto file = std::fopen(path, "r"); not file)
          throw std::runtime_error{lua_pushfstring(L, "File not found: %s", path)};
        else
          std::fclose(file), paths.push_back(path);
      textures_load(std::move(paths));
      return 0;
    }
    catch (std::exception const &e)
    {
      lua_pushstring(L, e.what());
    }
    return lua_error(L);
  }
  static auto camera /*            */ (lua_State *L) -> int // fun(left: number, right: number, bottom: number, top: number)
  {
    if (auto argc = lua_gettop(L); argc not_eq 4)
      return luaL_error(L, "Expected 4 args. Got %d", argc);
    projection = glm::ortho<float>(luaL_checknumber(L, 1),
                                   luaL_checknumber(L, 2),
                                   luaL_checknumber(L, 3),
                                   luaL_checknumber(L, 4));
    return 0;
  }
  static auto viewport /*          */ (lua_State *L) -> int // fun(x: integer, y: integer, width: integer, height: integer)
  {
    if (auto argc = lua_gettop(L); argc not_eq 4)
      return luaL_error(L, "Expected 4 args. Got %d", argc);
    glViewport(luaL_checkinteger(L, 1),
               luaL_checkinteger(L, 2),
               luaL_checkinteger(L, 3),
               luaL_checkinteger(L, 4));
    return 0;
  }
  static auto time /*              */ (lua_State *L) -> int // fun():number
  {
    lua_pushnumber(L, glfwGetTime());
    return 1;
  }
  static auto event_window_init /* */ (lua_State *L) -> int // fun()
  {
    using utils::CTS;
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
  static auto open_lib /*          */ (lua_State *L) -> int // fun()
  {
    { // game
      luaL_Reg static constexpr game[]{
          {"tile_sets_clear" /*  */, tile_sets_clear /*  */},
          {"tile_sets_add" /*    */, tile_sets_add /*    */},
          {"tile_sets_set" /*    */, tile_sets_set /*    */},
          {"tiles_set" /*        */, tiles_set /*        */},
          {"instances_clear" /*  */, instances_clear /*  */},
          {"instances_add" /*    */, instances_add /*    */},
          {"instances_set" /*    */, instances_set /*    */},
          {"tick_rate" /*        */, tick_rate /*        */},
          {"reopen_lib" /*       */, open_lib /*         */},
          {"textures_set" /*     */, textures_set /*     */},
          {"camera" /*           */, camera /*           */},
          {"viewport" /*         */, viewport /*         */},
          {"time" /*             */, time /*             */},
          {0, 0}};
      luaL_newlib(L, game);
      lua_pushvalue(L, -1);
      lua_setglobal(L, "game");
    }
    { // game.event
      luaL_Reg static constexpr event[]{
          {"window_init", event_window_init}, // function()
          {0, 0}};
      luaL_newlib(L, event), lua_setfield(L, -2, "event");
    }
    { // game.input
      lua_createtable(L, 0, 3);
      { // game.input.keys
        auto static constexpr keys = std::array{std::tuple{(uint16_t)GLFW_KEY_UNKNOWN, "UNKNOWN"}, std::tuple{(uint16_t)GLFW_KEY_SPACE, "SPACE"}, std::tuple{(uint16_t)GLFW_KEY_APOSTROPHE, "APOSTROPHE"}, std::tuple{(uint16_t)GLFW_KEY_COMMA, "COMMA"}, std::tuple{(uint16_t)GLFW_KEY_MINUS, "MINUS"}, std::tuple{(uint16_t)GLFW_KEY_PERIOD, "PERIOD"}, std::tuple{(uint16_t)GLFW_KEY_SLASH, "SLASH"}, std::tuple{(uint16_t)GLFW_KEY_0, "0"}, std::tuple{(uint16_t)GLFW_KEY_1, "1"}, std::tuple{(uint16_t)GLFW_KEY_2, "2"}, std::tuple{(uint16_t)GLFW_KEY_3, "3"}, std::tuple{(uint16_t)GLFW_KEY_4, "4"}, std::tuple{(uint16_t)GLFW_KEY_5, "5"}, std::tuple{(uint16_t)GLFW_KEY_6, "6"}, std::tuple{(uint16_t)GLFW_KEY_7, "7"}, std::tuple{(uint16_t)GLFW_KEY_8, "8"}, std::tuple{(uint16_t)GLFW_KEY_9, "9"}, std::tuple{(uint16_t)GLFW_KEY_SEMICOLON, "SEMICOLON"}, std::tuple{(uint16_t)GLFW_KEY_EQUAL, "EQUAL"}, std::tuple{(uint16_t)GLFW_KEY_A, "A"}, std::tuple{(uint16_t)GLFW_KEY_B, "B"}, std::tuple{(uint16_t)GLFW_KEY_C, "C"}, std::tuple{(uint16_t)GLFW_KEY_D, "D"}, std::tuple{(uint16_t)GLFW_KEY_E, "E"}, std::tuple{(uint16_t)GLFW_KEY_F, "F"}, std::tuple{(uint16_t)GLFW_KEY_G, "G"}, std::tuple{(uint16_t)GLFW_KEY_H, "H"}, std::tuple{(uint16_t)GLFW_KEY_I, "I"}, std::tuple{(uint16_t)GLFW_KEY_J, "J"}, std::tuple{(uint16_t)GLFW_KEY_K, "K"}, std::tuple{(uint16_t)GLFW_KEY_L, "L"}, std::tuple{(uint16_t)GLFW_KEY_M, "M"}, std::tuple{(uint16_t)GLFW_KEY_N, "N"}, std::tuple{(uint16_t)GLFW_KEY_O, "O"}, std::tuple{(uint16_t)GLFW_KEY_P, "P"}, std::tuple{(uint16_t)GLFW_KEY_Q, "Q"}, std::tuple{(uint16_t)GLFW_KEY_R, "R"}, std::tuple{(uint16_t)GLFW_KEY_S, "S"}, std::tuple{(uint16_t)GLFW_KEY_T, "T"}, std::tuple{(uint16_t)GLFW_KEY_U, "U"}, std::tuple{(uint16_t)GLFW_KEY_V, "V"}, std::tuple{(uint16_t)GLFW_KEY_W, "W"}, std::tuple{(uint16_t)GLFW_KEY_X, "X"}, std::tuple{(uint16_t)GLFW_KEY_Y, "Y"}, std::tuple{(uint16_t)GLFW_KEY_Z, "Z"}, std::tuple{(uint16_t)GLFW_KEY_LEFT_BRACKET, "LEFT_BRACKET"}, std::tuple{(uint16_t)GLFW_KEY_BACKSLASH, "BACKSLASH"}, std::tuple{(uint16_t)GLFW_KEY_RIGHT_BRACKET, "RIGHT_BRACKET"}, std::tuple{(uint16_t)GLFW_KEY_GRAVE_ACCENT, "GRAVE_ACCENT"}, std::tuple{(uint16_t)GLFW_KEY_WORLD_1, "WORLD_1"}, std::tuple{(uint16_t)GLFW_KEY_WORLD_2, "WORLD_2"}, std::tuple{(uint16_t)GLFW_KEY_ESCAPE, "ESCAPE"}, std::tuple{(uint16_t)GLFW_KEY_ENTER, "ENTER"}, std::tuple{(uint16_t)GLFW_KEY_TAB, "TAB"}, std::tuple{(uint16_t)GLFW_KEY_BACKSPACE, "BACKSPACE"}, std::tuple{(uint16_t)GLFW_KEY_INSERT, "INSERT"}, std::tuple{(uint16_t)GLFW_KEY_DELETE, "DELETE"}, std::tuple{(uint16_t)GLFW_KEY_RIGHT, "RIGHT"}, std::tuple{(uint16_t)GLFW_KEY_LEFT, "LEFT"}, std::tuple{(uint16_t)GLFW_KEY_DOWN, "DOWN"}, std::tuple{(uint16_t)GLFW_KEY_UP, "UP"}, std::tuple{(uint16_t)GLFW_KEY_PAGE_UP, "PAGE_UP"}, std::tuple{(uint16_t)GLFW_KEY_PAGE_DOWN, "PAGE_DOWN"}, std::tuple{(uint16_t)GLFW_KEY_HOME, "HOME"}, std::tuple{(uint16_t)GLFW_KEY_END, "END"}, std::tuple{(uint16_t)GLFW_KEY_CAPS_LOCK, "CAPS_LOCK"}, std::tuple{(uint16_t)GLFW_KEY_SCROLL_LOCK, "SCROLL_LOCK"}, std::tuple{(uint16_t)GLFW_KEY_NUM_LOCK, "NUM_LOCK"}, std::tuple{(uint16_t)GLFW_KEY_PRINT_SCREEN, "PRINT_SCREEN"}, std::tuple{(uint16_t)GLFW_KEY_PAUSE, "PAUSE"}, std::tuple{(uint16_t)GLFW_KEY_F1, "F1"}, std::tuple{(uint16_t)GLFW_KEY_F2, "F2"}, std::tuple{(uint16_t)GLFW_KEY_F3, "F3"}, std::tuple{(uint16_t)GLFW_KEY_F4, "F4"}, std::tuple{(uint16_t)GLFW_KEY_F5, "F5"}, std::tuple{(uint16_t)GLFW_KEY_F6, "F6"}, std::tuple{(uint16_t)GLFW_KEY_F7, "F7"}, std::tuple{(uint16_t)GLFW_KEY_F8, "F8"}, std::tuple{(uint16_t)GLFW_KEY_F9, "F9"}, std::tuple{(uint16_t)GLFW_KEY_F10, "F10"}, std::tuple{(uint16_t)GLFW_KEY_F11, "F11"}, std::tuple{(uint16_t)GLFW_KEY_F12, "F12"}, std::tuple{(uint16_t)GLFW_KEY_F13, "F13"}, std::tuple{(uint16_t)GLFW_KEY_F14, "F14"}, std::tuple{(uint16_t)GLFW_KEY_F15, "F15"}, std::tuple{(uint16_t)GLFW_KEY_F16, "F16"}, std::tuple{(uint16_t)GLFW_KEY_F17, "F17"}, std::tuple{(uint16_t)GLFW_KEY_F18, "F18"}, std::tuple{(uint16_t)GLFW_KEY_F19, "F19"}, std::tuple{(uint16_t)GLFW_KEY_F20, "F20"}, std::tuple{(uint16_t)GLFW_KEY_F21, "F21"}, std::tuple{(uint16_t)GLFW_KEY_F22, "F22"}, std::tuple{(uint16_t)GLFW_KEY_F23, "F23"}, std::tuple{(uint16_t)GLFW_KEY_F24, "F24"}, std::tuple{(uint16_t)GLFW_KEY_F25, "F25"}, std::tuple{(uint16_t)GLFW_KEY_KP_0, "KP_0"}, std::tuple{(uint16_t)GLFW_KEY_KP_1, "KP_1"}, std::tuple{(uint16_t)GLFW_KEY_KP_2, "KP_2"}, std::tuple{(uint16_t)GLFW_KEY_KP_3, "KP_3"}, std::tuple{(uint16_t)GLFW_KEY_KP_4, "KP_4"}, std::tuple{(uint16_t)GLFW_KEY_KP_5, "KP_5"}, std::tuple{(uint16_t)GLFW_KEY_KP_6, "KP_6"}, std::tuple{(uint16_t)GLFW_KEY_KP_7, "KP_7"}, std::tuple{(uint16_t)GLFW_KEY_KP_8, "KP_8"}, std::tuple{(uint16_t)GLFW_KEY_KP_9, "KP_9"}, std::tuple{(uint16_t)GLFW_KEY_KP_DECIMAL, "KP_DECIMAL"}, std::tuple{(uint16_t)GLFW_KEY_KP_DIVIDE, "KP_DIVIDE"}, std::tuple{(uint16_t)GLFW_KEY_KP_MULTIPLY, "KP_MULTIPLY"}, std::tuple{(uint16_t)GLFW_KEY_KP_SUBTRACT, "KP_SUBTRACT"}, std::tuple{(uint16_t)GLFW_KEY_KP_ADD, "KP_ADD"}, std::tuple{(uint16_t)GLFW_KEY_KP_ENTER, "KP_ENTER"}, std::tuple{(uint16_t)GLFW_KEY_KP_EQUAL, "KP_EQUAL"}, std::tuple{(uint16_t)GLFW_KEY_LEFT_SHIFT, "LEFT_SHIFT"}, std::tuple{(uint16_t)GLFW_KEY_LEFT_CONTROL, "LEFT_CONTROL"}, std::tuple{(uint16_t)GLFW_KEY_LEFT_ALT, "LEFT_ALT"}, std::tuple{(uint16_t)GLFW_KEY_LEFT_SUPER, "LEFT_SUPER"}, std::tuple{(uint16_t)GLFW_KEY_RIGHT_SHIFT, "RIGHT_SHIFT"}, std::tuple{(uint16_t)GLFW_KEY_RIGHT_CONTROL, "RIGHT_CONTROL"}, std::tuple{(uint16_t)GLFW_KEY_RIGHT_ALT, "RIGHT_ALT"}, std::tuple{(uint16_t)GLFW_KEY_RIGHT_SUPER, "RIGHT_SUPER"}, std::tuple{(uint16_t)GLFW_KEY_MENU, "MENU"}, std::tuple{(uint16_t)GLFW_KEY_LAST, "LAST"}};
        lua_createtable(L, 0, (int)keys.size());
        for (auto [key, name] : keys)
          lua_pushinteger(L, key), lua_setfield(L, -2, name);
        lua_setfield(L, -2, "key");
      }
      { // game.input.mouse_buttons
        auto static constexpr mouse_buttons = std::array{std::tuple{(uint8_t)GLFW_MOUSE_BUTTON_1, "1"}, std::tuple{(uint8_t)GLFW_MOUSE_BUTTON_2, "2"}, std::tuple{(uint8_t)GLFW_MOUSE_BUTTON_3, "3"}, std::tuple{(uint8_t)GLFW_MOUSE_BUTTON_4, "4"}, std::tuple{(uint8_t)GLFW_MOUSE_BUTTON_5, "5"}, std::tuple{(uint8_t)GLFW_MOUSE_BUTTON_6, "6"}, std::tuple{(uint8_t)GLFW_MOUSE_BUTTON_7, "7"}, std::tuple{(uint8_t)GLFW_MOUSE_BUTTON_8, "8"}, std::tuple{(uint8_t)GLFW_MOUSE_BUTTON_LAST, "LAST"}, std::tuple{(uint8_t)GLFW_MOUSE_BUTTON_LEFT, "LEFT"}, std::tuple{(uint8_t)GLFW_MOUSE_BUTTON_RIGHT, "RIGHT"}, std::tuple{(uint8_t)GLFW_MOUSE_BUTTON_MIDDLE, "MIDDLE"}};
        lua_createtable(L, 0, (int)mouse_buttons.size());
        for (auto [mouse_button, name] : mouse_buttons)
          lua_pushinteger(L, mouse_button), lua_setfield(L, -2, name);
        lua_setfield(L, -2, "mouse_button");
      }
      { // game.input.mb = game.input.mouse_buttons
        lua_getfield(L, -1, "mouse_button");
        lua_setfield(L, -2, "mb");
      }
      lua_setfield(L, -2, "input");
    }
    lua_pop(L, 1); // pop game table
    return 0;
  }
  static auto init()
  {
    L = {ASSERT(luaL_newstate()), lua_close};
    luaL_openlibs(L);
    lua::open_lib(L);
    event_window_init(L);
  }
}

static inline void setup()
{
  glClearColor(0.1, 0.1, 0.1, 0.1);
  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &texture_slot_count);

  vao_init();
  pid_init(load_glsl_from_file("res/shaders/vert.glsl"),
           load_glsl_from_file("res/shaders/frag.glsl"));

  textures_load(std::vector<std::string>{
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
  tile_sets.resize(texture_slot_count, tile_sets.at(0));
  tile_sets_upload();

  tiles.resize((size_t)40 * 36);
  std::generate(tiles.begin(), tiles.end(), [i = 0]() mutable
                { return i++; });
  tiles_upload();

  lua::init();
  if (auto path = "res/scripts/main.lua"; luaL_dofile(L, path) not_eq LUA_OK)
    std::fprintf(stderr, "Lua Error: %s\n", lua_tolstring(L, -1, 0));
  lua_settop(L, 0);
}

static inline void update()
{
  pid_init(load_glsl_from_file("res/shaders/vert.glsl"),
           load_glsl_from_file("res/shaders/frag.glsl"));

  if (auto path = "res/scripts/main.lua"; luaL_dofile(L, path) not_eq LUA_OK)
    std::fprintf(stderr, "Lua Error: %s\n", lua_tolstring(L, -1, 0));
  lua_settop(L, 0);

  if (lua_getglobal(L, "update"); lua_iscfunction(L, -1))
    if (lua_pcall(L, 0, 0, 0) not_eq LUA_OK)
      std::fprintf(stderr, "Lua Error in update(): %s\n", lua_tolstring(L, -1, 0));
  lua_settop(L, 0);
}

static inline void loop()
{
  for (auto i = size_t{0}; i < update_ticker.max_ticks_per_frame && update_ticker.next(); i++)
    update();

  glClear(GL_COLOR_BUFFER_BIT);

  glUseProgram(pid);
  glBindVertexArray(vao);

  for (auto i = 0; i < textures.size(); i++)
  {
    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(GL_TEXTURE_2D, textures.at(i));
  }

  glUniformMatrix4fv(uniform.projection, 1, GL_FALSE, &projection[0][0]);

  tiles_draw(40);
  instances_draw();

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