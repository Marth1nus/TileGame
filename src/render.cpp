#include "render.hpp"
#include <stb_image.h>

namespace game::render
{
  auto glCheckError() -> void
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
      utils::print_errorf("GLES Error 0x%x %s", err, str);
    }
  }
  auto make_shader(GLenum type, std::string_view glsl, std::string_view common) noexcept -> GLuint
  {
    auto const error = [&](char const *msg) -> GLuint
    {
      auto const type_str = type == GL_FRAGMENT_SHADER ? "fragment"
                            : type == GL_VERTEX_SHADER ? "vertex"
                                                       : "invalid";
      utils::print_errorf("Shader Error (type:%s): %s", type_str, msg);
      return 0;
    };
    auto find = glsl.find("#version");
    if (find == std::string_view::npos)
      return error("#version required");
    auto version = glsl.substr(find);
    find = version.find('\n');
    if (find == std::string_view::npos)
      return error("#version requires own line");
    glsl = version.substr(find);
    version = version.substr(0, find + 1);
    auto const sources = std::array{/*    */ version.data(), /*    */ common.data(), /*    */ glsl.data()};
    auto const lengths = std::array{(GLsizei)version.size(), (GLsizei)common.size(), (GLsizei)glsl.size()};
    auto sid = glCreateShader(type);
    glShaderSource(sid, (GLsizei)sources.size(), sources.data(), lengths.data());
    glCompileShader(sid);
    glCheckError();
    if (GLsizei status, len; glGetShaderiv(sid, GL_COMPILE_STATUS, &status), not status)
    {
      glGetShaderiv(sid, GL_INFO_LOG_LENGTH, &len);
      auto log = std::unique_ptr<char[]>(new char[len + 1]);
      glGetShaderInfoLog(sid, len, &len, log.get());
      glDeleteShader(sid), sid = 0;
      glCheckError();
      return error(log.get());
    }
    glCheckError();
    return sid;
  }
  auto make_program(std::string_view vert, std::string_view frag, std::string_view common) -> make_program_result
  {
    auto vid = make_shader(GL_VERTEX_SHADER /*   */, vert, common);
    auto fid = make_shader(GL_FRAGMENT_SHADER /* */, frag, common);
    auto pid = glCreateProgram();
    glAttachShader(pid, vid);
    glAttachShader(pid, fid);
    glLinkProgram(pid);
    glCheckError();
    if (GLsizei status, len; glGetProgramiv(pid, GL_LINK_STATUS, &status), not status)
    {
      glGetProgramiv(pid, GL_INFO_LOG_LENGTH, &len);
      auto const pre = std::string_view{"Program error: "};
      auto const msg = std::unique_ptr<char[]>(new char[pre.size() + len + 1]);
      auto const log = std::copy(pre.begin(), pre.end(), msg.get());
      glGetProgramInfoLog(pid, len, &len, log);
      glDeleteProgram(pid), pid = 0;
      utils::print_errorf("%s", msg.get());
    }
    glCheckError();
    return {pid, vid, fid};
  }
  auto make_program(std::string_view vert, std::string_view frag, program_defines defines) -> make_program_result
  {
    auto static constexpr &fmt = "#define TEXTURE_SLOTS %3du\n"
                                 "#define WEBGL         %3d \n";
    char buf[std::size(fmt)];
    auto [common, alloc] = utils::snprintf(buf, fmt, defines.texture_slots, defines.webgl);
    return make_program(vert, frag, common);
  }
}
namespace game::render::tile
{
  renderer::renderer()
  {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glCheckError();
  }

  renderer::~renderer()
  {
    textures_delete();
    program_delete();
    mesh_delete();
  }

  auto renderer::mesh_delete() -> void
  {
    if (m_vao)
      glDeleteVertexArrays(1, &m_vao), m_vao = 0;
    { // buffers
      auto len = 0;
      auto bos = std::array<GLuint, s_buffer_members.size()>{};
      for (auto mem : s_buffer_members)
      {
        if (this->*mem)
          bos.at(len++) = this->*mem;
        this->*mem = 0;
      }
      if (len)
        glDeleteBuffers(len, bos.data());
    }
    m_chunk_attrib = -1;
    m_chunk_len = 0;
    glCheckError();
  }
  auto renderer::mesh_reload() -> void
  {
    mesh_delete();
    { // Vertex Array
      glGenVertexArrays(1, &m_vao);
      glBindVertexArray(m_vao);
      glCheckError();
    }
    { // Buffers
      auto bos = std::array<GLuint, s_buffer_members.size()>{};
      glGenBuffers((GLsizei)bos.size(), bos.data());
      for (auto i = 0; i < bos.size(); i++)
        this->*s_buffer_members.at(i) = bos.at(i);
      glCheckError();
    }
    auto i = 0; // attrib index tracker
    {           // in vec2 mesh_pos;
      glBindBuffer(GL_ARRAY_BUFFER, m_vbo_mesh);
      glBufferData(GL_ARRAY_BUFFER, std::span{vertices}.size_bytes(), vertices.data(), GL_STATIC_DRAW);
      glVertexAttribPointer(i, 2, GL_FLOAT, GL_FALSE, sizeof(vertices.at(0)), (void *)0);
      glVertexAttribDivisor(i, 0);
      glEnableVertexAttribArray(i++);
      glCheckError();
    }
    { // in uint tile_id;
      glBindBuffer(GL_ARRAY_BUFFER, m_vbo_tiles);
      glBufferData(GL_ARRAY_BUFFER, sizeof(int32_t), 0, GL_DYNAMIC_DRAW);
      glVertexAttribIPointer(i, 1, GL_UNSIGNED_INT, sizeof(tile), (void *)0);
      glVertexAttribDivisor(i, 1);
      glEnableVertexAttribArray(i++);
      glCheckError();
    }
    { // in ivec4 chunk_grid;
      glBindBuffer(GL_ARRAY_BUFFER, m_vbo_chunks);
      glBufferData(GL_ARRAY_BUFFER, sizeof(int32_t), 0, GL_DYNAMIC_DRAW);
      glVertexAttribIPointer(i, 4, GL_INT, sizeof(chunk), (void *)offsetof(chunk, offset));
      glVertexAttribDivisor(m_chunk_attrib = i, m_chunk_len);
      glEnableVertexAttribArray(i++);
      glCheckError();
    }
    { // uniform TILESETS { tileset tilesets[TEXTURE_SLOTS]; }
      glBindBuffer(GL_UNIFORM_BUFFER, m_ubo_tilesets);
      glBufferData(GL_UNIFORM_BUFFER, sizeof(int32_t), 0, GL_DYNAMIC_DRAW);
      glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_ubo_tilesets);
      glCheckError();
    }
  }
  auto renderer::mesh_bind() -> void
  {
    glBindVertexArray(m_vao);
  }

  auto renderer::program_delete() -> void
  {
    if (m_pid)
      glDeleteProgram(m_pid), m_pid = 0;
    m_uniform_TILESETS = -1;
    m_uniform_tileset_count = -1;
    m_uniform_projection = -1;
    m_uniform_textures = -1;
    glCheckError();
  }
  auto renderer::program_reload(std::string_view vert, std::string_view frag) -> void
  {
    auto texture_slots = 0;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &texture_slots);
    auto const [pid, vid, fid] = make_program(vert, frag, {texture_slots});
    glDeleteShader(vid), glDeleteShader(fid);
    glUseProgram(m_pid = pid);
    glCheckError();
    auto textures = std::unique_ptr<int32_t[]>{new int32_t[texture_slots]};
    for (auto i = 0; i < texture_slots; i++)
      textures[i] = i;
    glUniformBlockBinding(m_pid, m_uniform_TILESETS = (int32_t)glGetUniformBlockIndex(m_pid, "TILESETS" /*      */), 0u), glCheckError();
    glUniform1ui /*          */ (m_uniform_tileset_count /* */ = glGetUniformLocation(m_pid, "tileset_count" /* */), m_tileset_count), glCheckError();
    glUniformMatrix4fv /*    */ (m_uniform_projection /*    */ = glGetUniformLocation(m_pid, "projection" /*    */), 1u, false, &m_projection[0][0]), glCheckError();
    glUniform1iv /*          */ (m_uniform_textures /*      */ = glGetUniformLocation(m_pid, "textures" /*      */), texture_slots, textures.get()), glCheckError();
    glCheckError();
  }
  auto renderer::program_use() -> void
  {
    glUseProgram(m_pid);
  }

  auto renderer::textures_delete() -> void
  {
    if (m_textures.size())
      glDeleteTextures((GLsizei)m_textures.size(), m_textures.data());
    m_textures = {};
    m_texture_paths = {};
    glCheckError();
  }
  auto renderer::textures_reload(std::vector<std::string> &&texture_paths) -> void
  {
    textures_delete();
    m_texture_paths = std::forward<decltype(texture_paths)>(texture_paths);
    m_textures.resize(m_texture_paths.size());
    if (m_textures.size())
      glGenTextures((GLsizei)m_textures.size(), m_textures.data());
    for (auto i = 0; i < m_textures.size(); i++)
    {
      auto const &tid = m_textures.at(i);
      auto const path = m_texture_paths.at(i).c_str();
      glBindTexture(GL_TEXTURE_2D, tid);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
      glCheckError();
      auto width = 0, height = 0, chanels = 0, desired_chanels = 4;
      auto pixels = stbi_load(path, &width, &height, &chanels, desired_chanels);
      if (not pixels)
      {
        utils::print_errorf("failed to load texture '%s'", path);
        continue;
      }
      utils::assertf(chanels == desired_chanels, "failed to load with desired chanels: %s", path);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
      glCheckError();
      stbi_image_free(pixels);
      glGenerateMipmap(GL_TEXTURE_2D);
      glCheckError();
    }
  }

  auto renderer::upload(std::span<tile const> data) -> void
  {
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_tiles);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)data.size_bytes(), data.data(), GL_DYNAMIC_DRAW);
    m_tile_count = (GLsizei)data.size();
    glCheckError();
  }
  auto renderer::upload(std::span<chunk const> data) -> void
  {
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_chunks);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)data.size_bytes(), data.data(), GL_DYNAMIC_DRAW);
    m_chunk_len = data.empty() ? 0 : m_tile_count / (GLsizei)data.size();
    glCheckError();
  }
  auto renderer::upload(std::span<tileset const> data) -> void
  {
    glBindBuffer(GL_UNIFORM_BUFFER, m_ubo_tilesets);
    glBufferData(GL_UNIFORM_BUFFER, (GLsizeiptr)data.size_bytes(), data.data(), GL_DYNAMIC_DRAW);
    m_tileset_count = (GLsizei)data.size();
    glCheckError();
  }
  auto renderer::update(std::span<tile const> data, size_t offset, size_t chunk_index) -> void
  {
    offset += chunk_index * m_chunk_len;
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_tiles);
    glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(offset * sizeof(data[0])), (GLsizeiptr)data.size_bytes(), data.data());
    glCheckError();
  }
  auto renderer::update(std::span<chunk const> data, size_t offset) -> void
  {
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_chunks);
    glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(offset * sizeof(data[0])), (GLsizeiptr)data.size_bytes(), data.data());
    glCheckError();
  }
  auto renderer::update(std::span<tileset const> data, size_t offset) -> void
  {
    glBindBuffer(GL_UNIFORM_BUFFER, m_ubo_tilesets);
    glBufferSubData(GL_UNIFORM_BUFFER, (GLintptr)(offset * sizeof(data[0])), (GLsizeiptr)data.size_bytes(), data.data());
    glCheckError();
  }

  auto renderer::reload(std::string_view vert, std::string_view frag, std::vector<std::string> &&texture_paths,
                        std::span<tile const> tiles, std::span<chunk const> chunks, std::span<tileset const> tilesets) -> void
  {
    mesh_reload();
    program_reload(vert, frag);
    textures_reload(std::forward<decltype(texture_paths)>(texture_paths));
    upload(tiles);
    upload(chunks);
    upload(tilesets);
    utils::assertf(chunks.size() * m_chunk_len == tiles.size(), "Expected %zu tiles and got %zu", chunks.size() * m_chunk_len, tiles.size());
    for (auto const &chunk : chunks)
    {
      auto const chunk_len = chunk.size.x * chunk.size.y;
      utils::assertf(chunk_len == m_chunk_len, "Inconsistent Chunk length. expect:%d, got:%d, from:chunks[%zu]", m_chunk_len, chunk_len, chunks.data() - &chunk);
    }
    for (auto const &tileset : tilesets)
    {
      auto const i = tilesets.data() - &tileset;
      utils::assertf(tileset.columns > 0, "%s=0 on tilesets[%zu]", "columns", i);
      utils::assertf(tileset.rows > 0, "%s=0 on tilesets[%zu]", "rows", i);
      utils::assertf(tileset.tex < m_textures.size(), "tex out of bound on tilesets[%zu]", i);
    }
  }

  auto renderer::render_prep() -> void
  {
    glBindVertexArray(m_vao);
    glVertexAttribDivisor(m_chunk_attrib, m_chunk_len);
    glCheckError();
    glUseProgram(m_pid);
    glUniform1ui /*       */ (m_uniform_tileset_count /* */, m_tileset_count);
    glUniformMatrix4fv /* */ (m_uniform_projection /*    */, 1u, false, &m_projection[0][0]);
    glCheckError();
    for (auto i = 0; i < m_textures.size(); i++)
      glActiveTexture(GL_TEXTURE0 + i), glBindTexture(GL_TEXTURE_2D, m_textures.at(i));
    glCheckError();
  }
  auto renderer::render_draw() -> void
  {
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, (GLint)0, (GLsizei)vertices.size(), (GLsizei)m_tile_count);
    glCheckError();
  }
  auto renderer::render() -> void
  {
    render_prep();
    render_draw();
  }
}
