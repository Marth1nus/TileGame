#ifndef RENDER_HPP
#define RENDER_HPP

#include "common.hpp"
#include <glad/gles2.h>

namespace game::render
{
  struct make_program_result
  {
    GLuint pid, vid, fid;
  };
  struct program_defines
  {
    int texture_slots, webgl : 1 = 0;
  };
  auto glCheckError() -> void;
  auto make_shader(GLenum type, std::string_view glsl, std::string_view common) noexcept -> GLuint;
  auto make_program(std::string_view vert, std::string_view frag, std::string_view common) -> make_program_result;
  auto make_program(std::string_view vert, std::string_view frag, program_defines defines) -> make_program_result;

}
namespace game::render::tile
{
  using tile = glm::u32;
  struct chunk
  {
    glm::ivec2 offset;
    glm::uvec2 size;
  };
  struct tileset
  {
    glm::u32 tex, columns, rows, padding;
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
    renderer();
    ~renderer();

    auto mesh_delete() -> void;
    auto mesh_reload() -> void;
    auto mesh_bind() -> void;

    auto program_delete() -> void;
    auto program_reload(std::string_view vert, std::string_view frag) -> void;
    auto program_use() -> void;

    auto textures_delete() -> void;
    auto textures_reload(std::vector<std::string> &&texture_paths) -> void;

    auto upload(std::span<tile const> data) -> void;
    auto upload(std::span<chunk const> data) -> void;
    auto upload(std::span<tileset const> data) -> void;
    auto update(std::span<tile const> data, size_t offset = 0, size_t chunk_index = 0) -> void;
    auto update(std::span<chunk const> data, size_t offset = 0) -> void;
    auto update(std::span<tileset const> data, size_t offset = 0) -> void;

    auto reload(std::string_view vert, std::string_view frag, std::vector<std::string> &&texture_paths = {}) { return reload(vert, frag, std::forward<decltype(texture_paths)>(texture_paths), {}, {}, {}); }
    auto reload(std::string_view vert, std::string_view frag, std::vector<std::string> &&texture_paths,
                std::span<tile const> tiles, std::span<chunk const> chunks, std::span<tileset const> tilesets) -> void;

    auto render_prep() -> void;
    auto render_draw() -> void;
    auto render() -> void;

  private:
    GLuint m_pid{}, m_vao{},
        m_vbo_mesh{}, m_vbo_tiles{}, m_vbo_chunks{}, m_ubo_tilesets{};
    GLint m_chunk_attrib = -1;
    GLint m_uniform_TILESETS = -1,
          m_uniform_tileset_count = -1,
          m_uniform_projection = -1,
          m_uniform_textures = -1;
    std::vector<GLuint> m_textures{};
    std::vector<std::string> m_texture_paths{};

  public:
    auto set_projection() -> glm::mat4 & { return m_projection; }
    auto get_projection() const -> glm::mat4 const & { return m_projection; }
    auto set_projection(glm::mat4 const &v) -> glm::mat4 & { return m_projection = v; }

  private: // draw variables (updated or used in renderer::render())
    GLint m_tile_count = 0, m_chunk_len = 0, m_tileset_count = 0;
    glm::mat4 m_projection = glm::ortho<float>(-1, 1, 1, -1);

  private:
    auto inline static constexpr s_buffer_members = std::array{
        &renderer::m_vbo_mesh,
        &renderer::m_vbo_tiles,
        &renderer::m_vbo_chunks,
        &renderer::m_ubo_tilesets,
    };
  };
}

#endif // RENDER_HPP