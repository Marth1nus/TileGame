#ifndef RENDER_HPP
#define RENDER_HPP

#include "common.hpp"

namespace game::render
{
  enum class attrib
  {
    instance_tile,
    instance_pos,
  };
  struct tile_mesh
  {
    tile_mesh() noexcept = default;
    tile_mesh(tile_mesh const &) noexcept = delete;
    tile_mesh &operator=(tile_mesh const &) noexcept = delete;
    tile_mesh &operator=(tile_mesh &&value) noexcept { return DEFAULT_MOVE_CONSTRUCTOR(value); }
    tile_mesh(tile_mesh &&value) noexcept;
    tile_mesh(glm::uvec2 chunk_grid);
    ~tile_mesh();

    auto chunk_length() const { return m_chunk_size.x * m_chunk_size.y; }
    auto chunk_size() const { return m_chunk_size; }

    auto upload(std::span<uint32_t const> tile_ids, std::span<glm::vec3 const> positions) -> void;
    auto update(std::span<uint32_t const> tile_ids, size_t chunk_index_offset = 0, size_t tile_id_index_offset = 0) -> void;
    auto update(std::span<glm::vec3 const> positions, size_t chunk_index_offset = 0) -> void;

  private:
    uint32_t m_vao, m_vbo_instance_tile, m_vbo_instance_pos, m_instance_count;
    glm::uvec2 m_chunk_size;
    friend struct renderer;
  };
  struct renderer
  {
    renderer() noexcept = default;
    renderer(renderer const &) noexcept = delete;
    renderer &operator=(renderer const &) noexcept = delete;
    renderer &operator=(renderer &&value) noexcept { return DEFAULT_MOVE_CONSTRUCTOR(value); }
    renderer(renderer &&value) noexcept;
    renderer(std::string_view vert_glsl, std::string_view frag_glsl,
             glm::uvec2 tile_pixels_size, glm::uvec3 atlas_tiles_size, std::span<uint8_t const> rgba_u8_subpixels = {});
    ~renderer();

    auto uniform_location(std::string_view name) -> int32_t;
    auto uniform_prep() -> void;
    auto uniform_v(std::string_view name, std::span<glm::mat<4, 4, float /* */> const> values) -> void;
    auto uniform_v(std::string_view name, std::span<glm::vec<1, uint32_t /* */> const> values) -> void;
    auto uniform_v(std::string_view name, std::span<glm::vec<1, int32_t /*  */> const> values) -> void;
    auto uniform_v(std::string_view name, std::span<glm::vec<1, float /*    */> const> values) -> void;
    auto uniform_v(std::string_view name, std::span<glm::vec<2, uint32_t /* */> const> values) -> void;
    auto uniform_v(std::string_view name, std::span<glm::vec<2, int32_t /*  */> const> values) -> void;
    auto uniform_v(std::string_view name, std::span<glm::vec<2, float /*    */> const> values) -> void;
    auto uniform_v(std::string_view name, std::span<glm::vec<3, uint32_t /* */> const> values) -> void;
    auto uniform_v(std::string_view name, std::span<glm::vec<3, int32_t /*  */> const> values) -> void;
    auto uniform_v(std::string_view name, std::span<glm::vec<3, float /*    */> const> values) -> void;
    auto uniform_v(std::string_view name, std::span<glm::vec<4, uint32_t /* */> const> values) -> void;
    auto uniform_v(std::string_view name, std::span<glm::vec<4, int32_t /*  */> const> values) -> void;
    auto uniform_v(std::string_view name, std::span<glm::vec<4, float /*    */> const> values) -> void;
    auto inline uniform(std::string_view name, auto const &value) -> void /* clang-format off */ requires requires { uniform_v(name, std::span{&value, 1}); } { return uniform_v(name, std::span{&value, 1}); } /* clang-format on */
    auto inline uniform(std::string_view name, auto const &value) -> void /* clang-format off */ requires requires { uniform_v(name,            value    ); } { return uniform_v(name,            value    ); } /* clang-format on */

    auto upload_tile_texture(uint32_t tile_id, std::span<uint8_t const> rgba_u8_subpixels) -> void;
    auto upload_tile_textures(uint32_t tile_id_start, std::span<uint8_t const> rgba_u8_subpixels, size_t pixels_width) -> void;

    auto render(tile_mesh const &tile_mesh) -> void;

  private:
    uint32_t m_pid, m_tid;
    std::unordered_map<std::string_view, std::pair<std::unique_ptr<char[]>, int32_t>> m_unifrom_locations;
    glm::uvec2 m_tile_pixels_size;
    glm::uvec3 m_atlas_tiles_size;
  };
}

#endif // RENDER_HPP