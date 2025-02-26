#include "render.hpp"

#if defined(USE_GLAD)
#include <glad/glad.h>
#else // defined(USE_GLAD)
#include <GLES3/gl3.h>
#endif // defined(USE_GLAD)

#define glCheckError(...) ::game::render::gl::gl_check_error(__VA_ARGS__)

namespace game::render::gl
{
  auto static gl_error_name(GLenum err) -> char const *
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
  auto static gl_check_error(std::source_location location = std::source_location::current()) -> void
  {
    GLenum err;
    while ((err = glGetError()) not_eq GL_NO_ERROR)
      utils::errorf("GLES Error 0x%03x %-18s on line %u from function `%s`", err, gl_error_name(err), location.line(), location.function_name());
  }
  auto static make_shader(int32_t shader_type, std::string_view glsl) -> uint32_t
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
  auto static make_program(std::string_view vert_glsl, std::string_view frag_glsl) -> uint32_t
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
namespace game::render // tile_mesh
{
  tile_mesh::tile_mesh(tile_mesh &&value) noexcept
  {
    m_vao /*               */ = std::exchange(value.m_vao /*               */, {});
    m_vbo_instance_tile /* */ = std::exchange(value.m_vbo_instance_tile /* */, {});
    m_vbo_instance_pos /*  */ = std::exchange(value.m_vbo_instance_pos /*  */, {});
    m_instance_count /*    */ = std::exchange(value.m_instance_count /*    */, {});
    m_chunk_size /*        */ = std::exchange(value.m_chunk_size /*        */, {});
  }
  tile_mesh::tile_mesh(glm::uvec2 chunk_grid)
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
  tile_mesh::~tile_mesh()
  {
    if (m_vao)
      glDeleteVertexArrays(1, &m_vao), glCheckError();
    if (auto buffers = std::array{m_vbo_instance_tile, m_vbo_instance_pos};
        std::ranges::any_of(buffers, [](auto const &v) -> bool
                            { return v; }))
      glDeleteBuffers((GLsizei)buffers.size(), buffers.data()), glCheckError();
  }

  auto tile_mesh::upload(std::span<uint32_t const> tile_ids, std::span<glm::vec3 const> positions) -> void
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
  auto tile_mesh::update(std::span<uint32_t const> tile_ids, size_t chunk_index_offset, size_t tile_id_index_offset) -> void
  {
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_instance_tile);
    glBufferSubData(GL_ARRAY_BUFFER,
                    (GLsizei)((chunk_index_offset * chunk_length() + tile_id_index_offset) * sizeof(tile_ids[0])),
                    (GLsizei)tile_ids.size_bytes(), tile_ids.data());
    glCheckError();
  }
  auto tile_mesh::update(std::span<glm::vec3 const> positions, size_t chunk_index_offset) -> void
  {
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_instance_pos);
    glBufferSubData(GL_ARRAY_BUFFER,
                    (GLsizei)(chunk_index_offset * sizeof(positions[0])),
                    (GLsizei)positions.size_bytes(), positions.data());
    glCheckError();
  }
}
namespace game::render // renderer
{

  renderer::renderer(renderer &&value) noexcept
  {
    m_pid /*                  */ = std::exchange(value.m_pid /*                  */, {});
    m_tid /*                  */ = std::exchange(value.m_tid /*                  */, {});
    m_unifrom_locations /*    */ = std::exchange(value.m_unifrom_locations /*    */, {});
    m_atlas_tiles_size /*     */ = std::exchange(value.m_atlas_tiles_size /*     */, {});
    m_tile_pixels_size /*     */ = std::exchange(value.m_tile_pixels_size /*     */, {});
  }
  renderer::renderer(std::string_view vert_glsl, std::string_view frag_glsl,
                     glm::uvec2 tile_pixels_size, glm::uvec3 atlas_tiles_size, std::span<uint8_t const> rgba_u8_subpixels)
  {
    auto const subpixel_length = 4u;
    auto const subpixels = rgba_u8_subpixels;
    auto const pixels_size = atlas_tiles_size * glm::uvec3{tile_pixels_size, 1};
    auto const bytes_size = subpixel_length * pixels_size.x * pixels_size.y * pixels_size.z;
    utils::assertf(not subpixels.data() or subpixels.size() == bytes_size,
                   "Provided pixels is expected to have %zub but got %zub", bytes_size, subpixels.size());
    m_tile_pixels_size = tile_pixels_size;
    m_atlas_tiles_size = atlas_tiles_size;
    glGenTextures(1, &m_tid);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_tid);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, pixels_size.x, pixels_size.y, pixels_size.z);
    glCheckError();
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
                    0, 0, 0,
                    pixels_size.x, pixels_size.y, pixels_size.z,
                    GL_RGBA, GL_UNSIGNED_BYTE,
                    subpixels.data() ? subpixels.data() : std::make_unique<uint8_t[]>(bytes_size).get());
    glCheckError();
    m_pid = gl::make_program(vert_glsl, frag_glsl);
    uniform_prep();
    uniform("chunk_size" /*       */, glm::uvec2{1, 1});
    uniform("atlas_tiles_size" /* */, atlas_tiles_size);
  }
  renderer::~renderer()
  {
    if (m_pid)
      glDeleteProgram(m_pid), glCheckError();
    if (m_tid)
      glDeleteTextures(1, &m_tid), glCheckError();
  }

  auto renderer::uniform_location(std::string_view name) -> int32_t
  {
    if (auto it = m_unifrom_locations.find(name);
        it not_eq m_unifrom_locations.end())
      return it->second.second;
    auto name_str = std::make_unique<char[]>(name.size() + 1);
    std::strncpy(name_str.get(), name.data(), name.size())[name.size()] = '\0';
    auto location = glGetUniformLocation(m_pid, name_str.get());
    auto name_sv = std::string_view{name_str.get(), name.size()};
    m_unifrom_locations[name_sv] = std::pair{std::move(name_str), location};
    return glCheckError(), location;
  }
  auto renderer::uniform_prep() -> void { glUseProgram(m_pid), glCheckError(); }
  auto renderer::uniform_v(std::string_view name, std::span<glm::mat<4, 4, float /* */> const> values) -> void { glUniformMatrix4fv(uniform_location(name), (GLsizei)values.size(), GL_FALSE, &values[0][0][0]), glCheckError(); }
  auto renderer::uniform_v(std::string_view name, std::span<glm::vec<1, uint32_t /* */> const> values) -> void { glUniform1uiv(uniform_location(name), (GLsizei)values.size(), &values[0][0]), glCheckError(); }
  auto renderer::uniform_v(std::string_view name, std::span<glm::vec<1, int32_t /*  */> const> values) -> void { glUniform1iv(uniform_location(name), (GLsizei)values.size(), &values[0][0]), glCheckError(); }
  auto renderer::uniform_v(std::string_view name, std::span<glm::vec<1, float /*    */> const> values) -> void { glUniform1fv(uniform_location(name), (GLsizei)values.size(), &values[0][0]), glCheckError(); }
  auto renderer::uniform_v(std::string_view name, std::span<glm::vec<2, uint32_t /* */> const> values) -> void { glUniform2uiv(uniform_location(name), (GLsizei)values.size(), &values[0][0]), glCheckError(); }
  auto renderer::uniform_v(std::string_view name, std::span<glm::vec<2, int32_t /*  */> const> values) -> void { glUniform2iv(uniform_location(name), (GLsizei)values.size(), &values[0][0]), glCheckError(); }
  auto renderer::uniform_v(std::string_view name, std::span<glm::vec<2, float /*    */> const> values) -> void { glUniform2fv(uniform_location(name), (GLsizei)values.size(), &values[0][0]), glCheckError(); }
  auto renderer::uniform_v(std::string_view name, std::span<glm::vec<3, uint32_t /* */> const> values) -> void { glUniform3uiv(uniform_location(name), (GLsizei)values.size(), &values[0][0]), glCheckError(); }
  auto renderer::uniform_v(std::string_view name, std::span<glm::vec<3, int32_t /*  */> const> values) -> void { glUniform3iv(uniform_location(name), (GLsizei)values.size(), &values[0][0]), glCheckError(); }
  auto renderer::uniform_v(std::string_view name, std::span<glm::vec<3, float /*    */> const> values) -> void { glUniform3fv(uniform_location(name), (GLsizei)values.size(), &values[0][0]), glCheckError(); }
  auto renderer::uniform_v(std::string_view name, std::span<glm::vec<4, uint32_t /* */> const> values) -> void { glUniform4uiv(uniform_location(name), (GLsizei)values.size(), &values[0][0]), glCheckError(); }
  auto renderer::uniform_v(std::string_view name, std::span<glm::vec<4, int32_t /*  */> const> values) -> void { glUniform4iv(uniform_location(name), (GLsizei)values.size(), &values[0][0]), glCheckError(); }
  auto renderer::uniform_v(std::string_view name, std::span<glm::vec<4, float /*    */> const> values) -> void { glUniform4fv(uniform_location(name), (GLsizei)values.size(), &values[0][0]), glCheckError(); }

  auto renderer::upload_tile_texture(uint32_t tile_id, std::span<uint8_t const> rgba_u8_subpixels) -> void
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
  auto renderer::upload_tile_textures(uint32_t tile_id_start, std::span<uint8_t const> rgba_u8_subpixels, size_t pixels_width) -> void
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

  auto renderer::render(tile_mesh const &tile_mesh) -> void
  {
    glCheckError();
    glUseProgram(m_pid);
    glBindVertexArray(tile_mesh.m_vao);
    uniform("chunk_size", tile_mesh.m_chunk_size);
    glCheckError();
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)tile_mesh.m_instance_count), glCheckError();
    glCheckError();
  }
  auto renderer::check_error(std::source_location location) -> void { glCheckError(location); }
}
