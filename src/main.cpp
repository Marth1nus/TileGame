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

#include <stb_image.h>
#include <stb_image_write.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>

#if defined(USE_GLAD)
#include <glad/glad.h>
#else // defined(USE_GLAD)
#include <GLES3/gl3.h>
#endif // defined(USE_GLAD)

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#else  // defined(__EMSCRIPTEN__)
#endif // defined(__EMSCRIPTEN__)

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
  auto static snprintf(std::span<char> buf, char const *fmt, ...) -> snprintf_result
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
  auto static inline assertf(auto &&value, char const *fmt, auto... args) -> decltype(value)
  {
    if (value)
      return std::forward<decltype(value)>(value);
    if constexpr (not sizeof...(args))
      throw failed_assert{fmt};
    else
    {
      char buf[0x100];
      auto [msg, alloc] = snprintf(buf, fmt, args...);
      throw failed_assert{msg.data()};
    }
  }
  auto static inline errorf(char const *fmt, auto... args) -> void
  {
#if defined(__EMSCRIPTEN__)
    auto msg = fmt;
#else  // defined(__EMSCRIPTEN__)
    char buf[0x100];
    auto [msg_sv, alloc] = snprintf(buf, "\033[31m%s\033[0m\n", fmt);
    auto msg = msg_sv.data();
#endif // defined(__EMSCRIPTEN__)
    if constexpr (sizeof...(args))
      std::fprintf(stderr, msg, args...);
    else
      std::fprintf(stderr, "%s", msg);
    error_breakpoint();
  }
  auto static glErrorName(GLenum err) -> char const *
  {
    switch (err)
    {
    /* clang-format off */ case GL_INVALID_ENUM:                  return "INVALID_ENUM";                  /* clang-format on */
    /* clang-format off */ case GL_INVALID_VALUE:                 return "INVALID_VALUE";                 /* clang-format on */
    /* clang-format off */ case GL_INVALID_OPERATION:             return "INVALID_OPERATION";             /* clang-format on */
    /* clang-format off */ case GL_OUT_OF_MEMORY:                 return "OUT_OF_MEMORY";                 /* clang-format on */
    /* clang-format off */ case GL_INVALID_FRAMEBUFFER_OPERATION: return "INVALID_FRAMEBUFFER_OPERATION"; /* clang-format on */
    /* clang-format off */ default:                               return "UNKNOWN_ERROR";                 /* clang-format on */
    }
  }
  auto inline constexpr glCheckError(char const *func, int line) -> void
  {
    for (GLenum err; (err = glGetError()) not_eq GL_NO_ERROR;)
      utils::errorf("GLES Error 0x%3x %32s --- on %s:%d", err, glErrorName(err), func, line);
  }
#define glCheckError() ::game::utils::glCheckError(__FUNCTION__, __LINE__)
  auto static read_all(char const *filepath, char const *mode = "r")
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
  auto static constexpr to_bool = [](std::convertible_to<bool> auto const &value) -> bool
  { return value; };
}
namespace game::render::gl
{
  auto make_shader(int32_t shader_type, std::string_view glsl) -> uint32_t
  {
    auto sources = std::array{/*    */ glsl.data()};
    auto lengths = std::array{(GLsizei)glsl.size()};
    auto sid = glCreateShader(shader_type);
    glShaderSource(sid, (GLsizei)sources.size(), sources.data(), lengths.data());
    glCompileShader(sid);
    if (int status, len; glGetShaderiv(sid, GL_COMPILE_STATUS, &status), not status)
    {
      glGetShaderiv(sid, GL_INFO_LOG_LENGTH, &len);
      auto log = std::unique_ptr<char[]>(new char[len]);
      glGetShaderInfoLog(sid, len, &len, log.get());
      auto shader_type_string = shader_type == GL_FRAGMENT_SHADER ? "Fragment" //
                                : shader_type == GL_VERTEX_SHADER ? "Vertex"
                                                                  : "Unknown";
      utils::errorf("%s Shader Error: %s", shader_type_string, log.get());
      glDeleteShader(sid), sid = 0;
    }
    glCheckError();
    return sid;
  }
  auto make_program(std::string_view vert_glsl, std::string_view frag_glsl) -> uint32_t
  {
    auto pid = glCreateProgram();
    auto vid = make_shader(GL_VERTEX_SHADER, vert_glsl),
         fid = make_shader(GL_FRAGMENT_SHADER, frag_glsl);
    glAttachShader(pid, vid);
    glAttachShader(pid, fid);
    glLinkProgram(pid);
    glDeleteShader(vid);
    glDeleteShader(fid);
    if (int status, len; glGetProgramiv(pid, GL_LINK_STATUS, &status), not status)
    {
      glGetProgramiv(pid, GL_INFO_LOG_LENGTH, &len);
      auto log = std::unique_ptr<char[]>(new char[len]);
      glGetProgramInfoLog(pid, len, &len, log.get());
      utils::errorf("Program Error: %s", log.get());
      glDeleteProgram(pid), pid = 0;
    }
    glCheckError();
    return pid;
  }
}
namespace game::render
{
  enum class attrib
  {
    instance_tile,
    instance_pos,
  };
  struct tile_mesh
  {
    tile_mesh() = default;
    tile_mesh(tile_mesh const &) = delete;
    tile_mesh &operator=(tile_mesh const &) = delete;
    tile_mesh &operator=(tile_mesh &&value) noexcept { return this == &value ? *this : (std::destroy_at(this), *std::construct_at(this, std::forward<decltype(value)>(value))); }
    tile_mesh(tile_mesh &&value) noexcept
    {
      m_vao /*               */ = std::exchange(value.m_vao /*               */, {});
      m_vbo_instance_tile /* */ = std::exchange(value.m_vbo_instance_tile /* */, {});
      m_vbo_instance_pos /*  */ = std::exchange(value.m_vbo_instance_pos /*  */, {});
      m_instance_count /*    */ = std::exchange(value.m_instance_count /*    */, {});
      m_chunk_size /*        */ = std::exchange(value.m_chunk_size /*        */, {});
    }
    tile_mesh(glm::uvec2 chunk_grid)
    {
      m_chunk_size = chunk_grid = glm::clamp(chunk_grid, {1, 1}, {0x1000, 0x1000});

      glGenVertexArrays(1, &m_vao);
      glBindVertexArray(m_vao);
      glCheckError();

      auto bos = std::array{m_vbo_instance_tile, m_vbo_instance_pos};
      glGenBuffers((GLsizei)bos.size(), bos.data());
      auto const &[vbo_instance_tile, vbo_instance_pos] = bos;
      m_vbo_instance_tile /* */ = vbo_instance_tile /* */;
      m_vbo_instance_pos /*  */ = vbo_instance_pos /*  */;
      glCheckError();

      glBindBuffer(GL_ARRAY_BUFFER, m_vbo_instance_tile);
      glBufferData(GL_ARRAY_BUFFER, (GLsizei)sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
      glVertexAttribIPointer((int)attrib::instance_tile, 1, GL_UNSIGNED_INT, (GLsizei)sizeof(uint32_t), (void *)0);
      glVertexAttribDivisor((int)attrib::instance_tile, 1);
      glEnableVertexAttribArray((int)attrib::instance_tile);
      glCheckError();

      glBindBuffer(GL_ARRAY_BUFFER, m_vbo_instance_pos);
      glBufferData(GL_ARRAY_BUFFER, (GLsizei)sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
      glVertexAttribPointer((int)attrib::instance_pos, 2, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(glm::vec3), (void *)0);
      glVertexAttribDivisor((int)attrib::instance_pos, chunk_grid.x * chunk_grid.y);
      glEnableVertexAttribArray((int)attrib::instance_pos);
      glCheckError();
    }
    ~tile_mesh()
    {
      if (m_vao)
        glDeleteVertexArrays(1, &m_vao), glCheckError();
      if (auto buffers = std::array{m_vbo_instance_tile, m_vbo_instance_pos};
          std::ranges::any_of(buffers, utils::to_bool))
        glDeleteBuffers((GLsizei)buffers.size(), buffers.data()), glCheckError();
    }

    auto chunk_length() const { return m_chunk_size.x * m_chunk_size.y; }
    auto chunk_size() const { return m_chunk_size; }

    auto upload(std::span<uint32_t const> tile_ids, std::span<glm::vec3 const> positions) -> void
    {
      utils::assertf(tile_ids.size() == positions.size() * chunk_length(),
                     "For %zu positions Expected %zu tile ids But got %zu tile ids",
                     positions.size(), positions.size() * chunk_length(), tile_ids.size());
      m_instance_count = (uint32_t)tile_ids.size();
      glBindBuffer(GL_ARRAY_BUFFER, m_vbo_instance_tile);
      glBufferData(GL_ARRAY_BUFFER, (GLsizei)tile_ids.size_bytes(), tile_ids.data(), GL_DYNAMIC_DRAW);
      glCheckError();
      glBindBuffer(GL_ARRAY_BUFFER, m_vbo_instance_pos);
      glBufferData(GL_ARRAY_BUFFER, (GLsizei)positions.size_bytes(), positions.data(), GL_DYNAMIC_DRAW);
      glCheckError();
    }
    auto update(std::span<uint32_t const> tile_ids, size_t chunk_index_offset = 0, size_t tile_id_index_offset = 0) -> void
    {
      glBindBuffer(GL_ARRAY_BUFFER, m_vbo_instance_tile);
      glBufferSubData(GL_ARRAY_BUFFER,
                      (GLsizei)((chunk_index_offset * chunk_length() + tile_id_index_offset) * sizeof(tile_ids[0])),
                      (GLsizei)tile_ids.size_bytes(), tile_ids.data());
      glCheckError();
    }
    auto update(std::span<glm::vec3 const> positions, size_t chunk_index_offset = 0) -> void
    {
      glBindBuffer(GL_ARRAY_BUFFER, m_vbo_instance_pos);
      glBufferSubData(GL_ARRAY_BUFFER,
                      (GLsizei)(chunk_index_offset * sizeof(positions[0])),
                      (GLsizei)positions.size_bytes(), positions.data());
      glCheckError();
    }

  private:
    uint32_t m_vao, m_vbo_instance_tile, m_vbo_instance_pos, m_instance_count;
    glm::uvec2 m_chunk_size;
    friend struct renderer;
  };
  struct renderer
  {
    renderer() = default;
    renderer(renderer const &) = delete;
    renderer &operator=(renderer const &) = delete;
    renderer &operator=(renderer &&value) noexcept { return this == &value ? *this : (std::destroy_at(this), *std::construct_at(this, std::forward<decltype(value)>(value))); }
    renderer(renderer &&value) noexcept
    {
      m_pid /*                  */ = std::exchange(value.m_pid /*                  */, {});
      m_tid /*                  */ = std::exchange(value.m_tid /*                  */, {});
      m_unifrom_locations /*    */ = std::exchange(value.m_unifrom_locations /*    */, {});
      m_atlas_tiles_size /*     */ = std::exchange(value.m_atlas_tiles_size /*     */, {});
      m_tile_pixels_size /*     */ = std::exchange(value.m_tile_pixels_size /*     */, {});
    }
    renderer(std::string_view vert_glsl, std::string_view frag_glsl,
             glm::uvec2 tile_pixels_size, glm::uvec3 atlas_tiles_size, std::span<uint8_t const> rgba_u8_subpixels = {})
    {
      auto const subpixel_length = 4u;
      auto const subpixels = rgba_u8_subpixels;
      auto const pixels_size = atlas_tiles_size * glm::uvec3{tile_pixels_size, 1};
      auto const pixels_bytes_size = subpixel_length * pixels_size.x * pixels_size.y * pixels_size.z;
      utils::assertf(not subpixels.data() or subpixels.size() == pixels_bytes_size,
                     "Provided pixels is expected to have %zub but got %zub", pixels_bytes_size, subpixels.size());
      m_tile_pixels_size = tile_pixels_size;
      m_atlas_tiles_size = atlas_tiles_size;
      glGenTextures(1, &m_tid);
      glBindTexture(GL_TEXTURE_2D_ARRAY, m_tid);
      glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
      glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
      glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, pixels_size.x, pixels_size.y, pixels_size.z, 0, GL_RGBA, GL_UNSIGNED_BYTE, subpixels.data());
      glCheckError();
      m_pid = gl::make_program(vert_glsl, frag_glsl);
      uniform_prep();
      uniform("chunk_size" /*       */, glm::uvec2{1, 1});
      uniform("atlas_tiles_size" /* */, atlas_tiles_size);
    }
    ~renderer()
    {
      if (m_pid)
        glDeleteProgram(m_pid), glCheckError();
      if (m_tid)
        glDeleteTextures(1, &m_tid), glCheckError();
    }

    auto uniform_location(std::string_view name) -> int32_t
    {
      if (auto it = m_unifrom_locations.find(name);
          it not_eq m_unifrom_locations.end())
        return it->second.second;
      auto name_str = std::make_unique<char[]>(name.size() + 1);
      std::strncpy(name_str.get(), name.data(), name.size())[name.size()] = '\0';
      auto location = glGetUniformLocation(m_pid, name_str.get());
      auto name_sv = std::string_view{name_str.get(), name.size()};
      m_unifrom_locations[name_sv] = std::pair{std::move(name_str), location};
      return location;
    }
    auto uniform_prep() -> void { glUseProgram(m_pid); }
    auto uniform_v(std::string_view name, std::span<glm::mat<4, 4, float /* */> const> values) -> void { glUniformMatrix4fv(uniform_location(name), (GLsizei)values.size(), GL_FALSE, &values[0][0][0]), glCheckError(); }
    auto uniform_v(std::string_view name, std::span<glm::vec<1, uint32_t /* */> const> values) -> void { glUniform1uiv(uniform_location(name), (GLsizei)values.size(), &values[0][0]), glCheckError(); }
    auto uniform_v(std::string_view name, std::span<glm::vec<1, int32_t /*  */> const> values) -> void { glUniform1iv(uniform_location(name), (GLsizei)values.size(), &values[0][0]), glCheckError(); }
    auto uniform_v(std::string_view name, std::span<glm::vec<1, float /*    */> const> values) -> void { glUniform1fv(uniform_location(name), (GLsizei)values.size(), &values[0][0]), glCheckError(); }
    auto uniform_v(std::string_view name, std::span<glm::vec<2, uint32_t /* */> const> values) -> void { glUniform2uiv(uniform_location(name), (GLsizei)values.size(), &values[0][0]), glCheckError(); }
    auto uniform_v(std::string_view name, std::span<glm::vec<2, int32_t /*  */> const> values) -> void { glUniform2iv(uniform_location(name), (GLsizei)values.size(), &values[0][0]), glCheckError(); }
    auto uniform_v(std::string_view name, std::span<glm::vec<2, float /*    */> const> values) -> void { glUniform2fv(uniform_location(name), (GLsizei)values.size(), &values[0][0]), glCheckError(); }
    auto uniform_v(std::string_view name, std::span<glm::vec<3, uint32_t /* */> const> values) -> void { glUniform3uiv(uniform_location(name), (GLsizei)values.size(), &values[0][0]), glCheckError(); }
    auto uniform_v(std::string_view name, std::span<glm::vec<3, int32_t /*  */> const> values) -> void { glUniform3iv(uniform_location(name), (GLsizei)values.size(), &values[0][0]), glCheckError(); }
    auto uniform_v(std::string_view name, std::span<glm::vec<3, float /*    */> const> values) -> void { glUniform3fv(uniform_location(name), (GLsizei)values.size(), &values[0][0]), glCheckError(); }
    auto uniform_v(std::string_view name, std::span<glm::vec<4, uint32_t /* */> const> values) -> void { glUniform4uiv(uniform_location(name), (GLsizei)values.size(), &values[0][0]), glCheckError(); }
    auto uniform_v(std::string_view name, std::span<glm::vec<4, int32_t /*  */> const> values) -> void { glUniform4iv(uniform_location(name), (GLsizei)values.size(), &values[0][0]), glCheckError(); }
    auto uniform_v(std::string_view name, std::span<glm::vec<4, float /*    */> const> values) -> void { glUniform4fv(uniform_location(name), (GLsizei)values.size(), &values[0][0]), glCheckError(); }
    auto inline uniform(std::string_view name, auto const &value) -> void /* clang-format off */ requires requires { uniform_v(name, std::span{&value, 1}); } { return uniform_v(name, std::span{&value, 1}); } /* clang-format on */
    auto inline uniform(std::string_view name, auto const &value) -> void /* clang-format off */ requires requires { uniform_v(name,            value    ); } { return uniform_v(name,            value    ); } /* clang-format on */

    auto upload_tile_texture(uint32_t tile_id, std::span<uint8_t const> rgba_u8_subpixels)
    {
      auto const subpixels = rgba_u8_subpixels;
      auto const atlas_tiles_size = m_atlas_tiles_size;
      auto const tile_pixels_size = glm::uvec3{m_tile_pixels_size, 1};
      utils::assertf(0 < tile_id and tile_id <= atlas_tiles_size.x * atlas_tiles_size.y, "%s", "Tile id %u out of range", tile_id);
      auto const expected_pixels_length = tile_pixels_size.x * tile_pixels_size.x * sizeof(uint8_t[4]);
      utils::assertf(subpixels.size() == expected_pixels_length, "Expected %zu (%ux%ux%u) bytes. Got %zu", expected_pixels_length, tile_pixels_size.x, tile_pixels_size.y, 4u, subpixels.size());
      auto const tile_pos = glm::uvec3{
          /**/ ((tile_id - 1u) % (atlas_tiles_size.x * atlas_tiles_size.y)) % atlas_tiles_size.x,
          /**/ ((tile_id - 1u) % (atlas_tiles_size.x * atlas_tiles_size.y)) / atlas_tiles_size.x,
          /**/ ((tile_id - 1u) / (atlas_tiles_size.x * atlas_tiles_size.y)) //
      };
      auto const pixels_pos = tile_pos * tile_pixels_size;
      glBindTexture(GL_TEXTURE_2D_ARRAY, m_tid);
      glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
                      pixels_pos.x, pixels_pos.y, pixels_pos.z,
                      tile_pixels_size.x, tile_pixels_size.y, tile_pixels_size.z,
                      GL_RGBA, GL_UNSIGNED_BYTE, subpixels.data());
      glCheckError();
    }
    auto upload_tile_textures(uint32_t tile_id_start, std::span<uint8_t const> rgba_u8_subpixels, size_t pixels_width)
    {
      auto const subpixel_length = 4u;
      auto const src_subpixels = rgba_u8_subpixels;
      auto const tile_pixels_size = m_tile_pixels_size;
      auto const src_pixels_size = glm::uvec2{pixels_width, src_subpixels.size() / (pixels_width * subpixel_length)};
      auto const src_tiles_size = src_pixels_size / tile_pixels_size;
      auto const tile_id_count = src_tiles_size.x * src_tiles_size.y;
      auto tile_pixels = std::vector<uint8_t>((size_t)tile_pixels_size.x * tile_pixels_size.y * subpixel_length);
      for (auto i = 0u; i < tile_id_count; i++)
      {
        auto const tile_id = tile_id_start + i;
        for (auto j = 0u; j < tile_pixels_size.y; j++)
          std::ranges::copy(
              src_subpixels.subspan(
                  (j * src_pixels_size.x +
                   (i % src_tiles_size.x) * tile_pixels_size.x +
                   (i / src_tiles_size.x) * tile_pixels_size.y * src_pixels_size.x) *
                      subpixel_length,
                  tile_pixels_size.x * subpixel_length),
              tile_pixels.begin() + j * tile_pixels_size.x * subpixel_length);
        upload_tile_texture(tile_id, tile_pixels);
        if constexpr (auto constexpr debug_save_bmps = 0)
        {
          char buf[0x16];
          stbi_write_bmp(utils::snprintf(buf, "build-tiles/%04u.bmp", tile_id).msg.data(),
                         tile_pixels_size.x, tile_pixels_size.y, subpixel_length, tile_pixels.data());
        }
      }
    }

    auto render(tile_mesh const &tile_mesh)
    {
      glUseProgram(m_pid);
      glBindVertexArray(tile_mesh.m_vao);
      glCheckError();
      uniform("chunk_size", tile_mesh.m_chunk_size);
      glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)tile_mesh.m_instance_count);
      glCheckError();
    }

  private:
    uint32_t m_pid, m_tid;
    std::unordered_map<std::string_view, std::pair<std::unique_ptr<char[]>, int32_t>> m_unifrom_locations;
    glm::uvec2 m_tile_pixels_size;
    glm::uvec3 m_atlas_tiles_size;
  };
}
namespace game
{
  struct application
  {
    application(application const &) = delete;
    application &operator=(application const &) = delete;

    application()
    {
      // Program Start
      struct glfw
      {
        glfw() { utils::assertf(glfwInit(), "%s %s", "glfw", "init fail"); }
        ~glfw() { glfwTerminate(); }
      } static const glfw{};
      auto window_title = "Game";
      auto window_width = 720, window_height = window_width;
      m_window = {glfwCreateWindow(window_width, window_height, window_title, 0, 0),
                  glfwDestroyWindow};
      auto window = m_window.get();
      utils::assertf(window, "%s %s", "window", "init fail");
      glfwMakeContextCurrent(window);
#if defined(USE_GLAD)
      utils::assertf(gladLoadGLES2Loader((GLADloadproc)glfwGetProcAddress), "%s %s", "glad", "init fail");
#else  // defined(USE_GLAD)
#endif // defined(USE_GLAD)
      // Constructor End
    }
    ~application()
    {
    }
    auto run() -> int
    {
      m_time_update_stamp = glfwGetTime();
      m_running = true;
      setup();
#if defined(__EMSCRIPTEN__)
      auto static loop = [this]
      {
        if (m_running and events())
        {
          update();
          render();
        }
        else
          emscripten_cancel_main_loop();
      };
      emscripten_set_main_loop([]
                               { loop(); }, 0, 1);
#else  // defined(__EMSCRIPTEN__)
      while (m_running and events())
      {
        update();
        render();
      }
#endif // defined(__EMSCRIPTEN__)
      shutdown();
      return 0;
    }

  private:
    auto setup() -> void
    {
      glfwSetWindowSizeCallback(m_window.get(), [](GLFWwindow *window, int width, int height)
                                { glViewport(0, 0, width, height); });

      auto width = 0, height = 0, channels = 4;
      auto pixels = std::shared_ptr<uint8_t>{
          stbi_load("res/rpg-asset-pack/3x/RPG tileset (full) v1.7 - 300%.png",
                    &width, &height, &channels, channels),
          stbi_image_free};
      utils::assertf(pixels, "failed to load tile atlas image");
      auto tile_pixels_size = glm::uvec2{48, 48};
      auto atlas_tiles_size = glm::uvec3{glm::uvec2{width, height} / tile_pixels_size, 1};
      auto atlas_pixels_size = atlas_tiles_size * glm::uvec3{tile_pixels_size, 1};
      auto pixels_span = std::span{pixels.get(), sizeof(uint8_t[4]) * atlas_pixels_size.x * atlas_pixels_size.y * atlas_pixels_size.z};
      m_renderer = {
          utils::read_all("res/shader/tile.vert.glsl"),
          utils::read_all("res/shader/tile.frag.glsl"),
          tile_pixels_size,
          atlas_tiles_size,
          // pixels_span,
      };
      m_renderer.upload_tile_textures(1, pixels_span, width);
      m_renderer.uniform_prep();
      m_renderer.uniform("projection", glm::ortho<float>(-16, 16, 16, -16));
      m_tile_meshes.clear();
      m_tile_meshes
          .emplace_back(glm::uvec2{5, 5}) // make a 4x4 tile chunk
          .upload(
              // tile ids
              std::array<uint32_t, 5 * 5 * 3>{
                  //
                  01, 02, 00, 02, 03,
                  33, 00, 02, 00, 35,
                  00, 33, 34, 35, 00,
                  33, 00, 66, 00, 35,
                  65, 66, 00, 66, 67, //
                  //
                  00 + 00, 00 + 00, 96 + 02, 00 + 00, 00 + 00,
                  00 + 00, 96 + 01, 00 + 00, 96 + 03, 00 + 00,
                  96 + 33, 00 + 00, 96 + 34, 00 + 00, 96 + 35,
                  00 + 00, 96 + 65, 00 + 00, 96 + 67, 00 + 00,
                  00 + 00, 00 + 00, 96 + 66, 00 + 00, 00 + 00, //
                  //
                  00, 00, 00, 00, 00,
                  00, 00, 00, 00, 00,
                  00, 00, 00, 00, 00,
                  00, 00, 00, 00, 00,
                  00, 00, 00, 00, 00, //
              },
              // chunk positions
              std::array<glm::vec3, 3>{});
    }
    auto events() -> bool
    {
      glfwPollEvents();
      return not glfwWindowShouldClose(m_window.get());
    }
    auto update() -> void
    {
      auto &old_stamp = m_time_update_stamp, new_stamp = glfwGetTime();
      if (new_stamp - old_stamp < m_dt)
        return;
      old_stamp = new_stamp;
      {
        auto static i = 0;
        std::printf("| Update:%4d | Time: %9.6lfs |\n", i++, glfwGetTime());
      }
      if (m_tile_meshes.empty())
        return;
      {
        auto mx = 0.0, my = mx;
        glfwGetCursorPos(m_window.get(), &mx, &my);
        auto ww = 0, wh = ww;
        glfwGetWindowSize(m_window.get(), &ww, &wh);
        auto p = glm::clamp(glm::vec2{mx / ww, my / wh} * 32.f - 16.f - 2.5f, {-16, -16}, {11, 11});
        auto static p0 = p, p1 = p, p2 = p;
        p0 += (p - p0) * 0.08f;
        p1 += (p - p1) * 0.04f;
        p2 += (p - p2) * 0.06f;
        m_tile_meshes.at(0).update(std::array{
            glm::vec3{p0, 2},
            glm::vec3{p1, 1},
            glm::vec3{p2, 3}, //
        });
      }
      {
        auto house_pos =
            glm::uvec2{10, 14} +
            glm::uvec2{5, 5} *
                std::array{
                    glm::uvec2{0, 0},
                    glm::uvec2{1, 0},
                    glm::uvec2{0, 1},
                    glm::uvec2{1, 1},
                }
                    .at(int(glfwGetTime()) % 4);
        auto house = std::array<uint32_t, 5 * 5>{};
        for (auto i = 0; i < 5; i++)
          for (auto j = 0; j < 5; j++)
            house.at(5 * i + j) = 1 + 32 * (house_pos.x + i) + (house_pos.y + j);
        m_tile_meshes.at(0).update(house, 2);
      }
    }
    auto render() -> void
    {
      glfwSwapInterval(1);
      glClearColor(0.1, 0.1, 0.1, 0.1);

      auto window = m_window.get();
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      m_renderer.uniform_prep();

      for (auto const &tile_mesh : m_tile_meshes)
        m_renderer.render(tile_mesh);

      glfwSwapBuffers(window);
    }
    auto shutdown() -> void
    {
    }

  private:
    std::shared_ptr<GLFWwindow> m_window;
    render::renderer m_renderer;
    std::vector<render::tile_mesh> m_tile_meshes;

    double m_time_update_stamp = 0, m_dt = 1.0 / 60.0;
    bool m_running : 1;
  };
}

int main()
{
  return game::application{}.run();
}