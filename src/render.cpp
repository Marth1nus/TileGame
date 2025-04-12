#include "render.hpp"

#if defined(USE_GLAD)
#include <glad/glad.h>
#else // defined(USE_GLAD)
#include <GLES3/gl3.h>
#endif // defined(USE_GLAD)

#define glCheckError(...) ::game::render::gl::gl_check_error(__VA_ARGS__)

namespace game::render
{
  using handle::convert;
}
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
  namespace tables
  {
    template <typename T>
    auto inline static constexpr enum_GLenum = std::false_type{};
    template <>
    auto inline /*  */ constexpr enum_GLenum<object::shader::type> = std::array{std::pair{object::shader::type::vertex, GL_VERTEX_SHADER}, std::pair{object::shader::type::fragment, GL_FRAGMENT_SHADER}};
    template <>
    auto inline /*  */ constexpr enum_GLenum<object::buffer::target> = std::array{std::pair{object::buffer::target::array, GL_ARRAY_BUFFER}, std::pair{object::buffer::target::element, GL_ELEMENT_ARRAY_BUFFER}, std::pair{object::buffer::target::uniform, GL_UNIFORM_BUFFER}};
    template <>
    auto inline /*  */ constexpr enum_GLenum<object::buffer::usage> = std::array{std::pair{object::buffer::usage::static_copy, GL_STATIC_COPY}, std::pair{object::buffer::usage::static_draw, GL_STATIC_DRAW}, std::pair{object::buffer::usage::static_read, GL_STATIC_READ}, std::pair{object::buffer::usage::dynamic_copy, GL_DYNAMIC_COPY}, std::pair{object::buffer::usage::dynamic_draw, GL_DYNAMIC_DRAW}, std::pair{object::buffer::usage::dynamic_read, GL_DYNAMIC_READ}};
    template <>
    auto inline /*  */ constexpr enum_GLenum<object::texture::target> = std::array{std::pair{object::texture::target::texture2, GL_TEXTURE_2D}, std::pair{object::texture::target::texture2_array, GL_TEXTURE_2D_ARRAY}, std::pair{object::texture::target::texture3, GL_TEXTURE_3D}};
    template <>
    auto inline /*  */ constexpr enum_GLenum<object::texture::format> = std::array{std::pair{object::texture::format::r8, GL_R8}, std::pair{object::texture::format::r8_snorm, GL_R8_SNORM}, std::pair{object::texture::format::r16f, GL_R16F}, std::pair{object::texture::format::r32f, GL_R32F}, std::pair{object::texture::format::r8ui, GL_R8UI}, std::pair{object::texture::format::r16ui, GL_R16UI}, std::pair{object::texture::format::r32ui, GL_R32UI}, std::pair{object::texture::format::r8i, GL_R8I}, std::pair{object::texture::format::r16i, GL_R16I}, std::pair{object::texture::format::r32i, GL_R32I}, std::pair{object::texture::format::rg8, GL_RG8}, std::pair{object::texture::format::rg8_snorm, GL_RG8_SNORM}, std::pair{object::texture::format::rg16f, GL_RG16F}, std::pair{object::texture::format::rg32f, GL_RG32F}, std::pair{object::texture::format::rg8ui, GL_RG8UI}, std::pair{object::texture::format::rg16ui, GL_RG16UI}, std::pair{object::texture::format::rg32ui, GL_RG32UI}, std::pair{object::texture::format::rg8i, GL_RG8I}, std::pair{object::texture::format::rg16i, GL_RG16I}, std::pair{object::texture::format::rg32i, GL_RG32I}, std::pair{object::texture::format::rgb8, GL_RGB8}, std::pair{object::texture::format::rgb8_snorm, GL_RGB8_SNORM}, std::pair{object::texture::format::rgb16f, GL_RGB16F}, std::pair{object::texture::format::rgb32f, GL_RGB32F}, std::pair{object::texture::format::rgb8ui, GL_RGB8UI}, std::pair{object::texture::format::rgb16ui, GL_RGB16UI}, std::pair{object::texture::format::rgb32ui, GL_RGB32UI}, std::pair{object::texture::format::rgb8i, GL_RGB8I}, std::pair{object::texture::format::rgb16i, GL_RGB16I}, std::pair{object::texture::format::rgb32i, GL_RGB32I}, std::pair{object::texture::format::rgba8, GL_RGBA8}, std::pair{object::texture::format::rgba8_snorm, GL_RGBA8_SNORM}, std::pair{object::texture::format::rgba16f, GL_RGBA16F}, std::pair{object::texture::format::rgba32f, GL_RGBA32F}, std::pair{object::texture::format::rgba8ui, GL_RGBA8UI}, std::pair{object::texture::format::rgba16ui, GL_RGBA16UI}, std::pair{object::texture::format::rgba32ui, GL_RGBA32UI}, std::pair{object::texture::format::rgba8i, GL_RGBA8I}, std::pair{object::texture::format::rgba16i, GL_RGBA16I}, std::pair{object::texture::format::rgba32i, GL_RGBA32I}};
    template <typename T>
    concept has_GLenum = not std::same_as<decltype(enum_GLenum<T>), std::false_type>;
    template <has_GLenum T>
    auto constexpr to_GLenum(T val) noexcept -> GLenum
    {
      for (auto const [en, gl] : enum_GLenum<T>)
        if (val == en)
          return gl;
      return GL_NONE;
    }
    template <has_GLenum T>
    auto constexpr to_enum(GLenum val) noexcept -> T
    {
      for (auto const [en, gl] : enum_GLenum<T>)
        if (val == gl)
          return en;
      return {};
    }
  }
  using tables::to_GLenum, tables::to_enum;
  /// @return [format_component_GLenum, format_component_count, format_type_GLenum, format_type_size]
  auto constexpr texture_format_details(texture::format f) noexcept -> auto
  {
    auto const i = (int)f,
               components = (i >> std::countr_zero(0xf0u)) bitand 0xf,
               type_index = (i >> std::countr_zero(0x0fu)) bitand 0xf;
    auto static constexpr sizes = std::array<uint8_t, 10>{sizeof(glm::u8), sizeof(glm::i8), sizeof(glm::i16), sizeof(glm::f32), sizeof(glm::u8), sizeof(glm::u16), sizeof(glm::u32), sizeof(glm::i8), sizeof(glm::i16), sizeof(glm::i32)};
    auto static constexpr types = std::array<uint16_t, 10>{GL_UNSIGNED_BYTE, GL_BYTE, GL_HALF_FLOAT, GL_FLOAT, GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, GL_UNSIGNED_INT, GL_BYTE, GL_SHORT, GL_INT};
    auto static constexpr comp_enum = std::array<uint16_t, 4>{GL_RED, GL_RG, GL_RGB, GL_RGBA};
    return std::tuple{(GLenum)comp_enum.at(components), (size_t)components, (GLenum)types.at(type_index), (size_t)sizes.at(type_index)};
  }
  /// @return [component_count, component_type_GLenum]
  auto constexpr vertex_attrib_details(vertexarray::attribute::type t) noexcept -> std::pair<GLsizei, GLenum>
  {
    using attrib_type = vertexarray::attribute::type;
    using enum attrib_type;
    if (range_vec_first <= t and t <= range_vec_last)
    {
      auto const count = (GLsizei)t bitand 0007;
      if (count < 1 or 4 < count)
        return {0, GL_NONE};
      auto const base = attrib_type((int)t bitand 0370 bitor 0001);
      for (auto const [en, gl] : {std::pair{u8, GL_UNSIGNED_BYTE}, std::pair{i8, GL_BYTE}, std::pair{u16, GL_UNSIGNED_SHORT}, std::pair{i16, GL_SHORT}, std::pair{f16, GL_HALF_FLOAT}, std::pair{u32, GL_UNSIGNED_INT}, std::pair{i32, GL_INT}, std::pair{f32, GL_FLOAT}})
        if (en == base)
          return {count, gl};
      return {0, GL_NONE};
    }
    return {0, GL_NONE};
  }
}
namespace game::render::handle
{
#define TEMPLATE_SPECIALIZE template <>
  TEMPLATE_SPECIALIZE auto create<shader>(shader_type shader_type) noexcept -> shader *
  {
    auto h = glCreateShader(gl::to_GLenum(shader_type));
    return glCheckError(), convert<shader>(h);
  }
  TEMPLATE_SPECIALIZE auto create<program>() noexcept -> program *
  {
    auto h = glCreateProgram();
    return glCheckError(), convert<program>(h);
  }
  TEMPLATE_SPECIALIZE auto create<buffer>() noexcept -> buffer *
  {
    auto h = 0u;
    return glGenBuffers(1, &h), glCheckError(), convert<buffer>(h);
  }
  TEMPLATE_SPECIALIZE auto create<texture>() noexcept -> texture *
  {
    auto h = 0u;
    return glGenTextures(1, &h), glCheckError(), convert<texture>(h);
  }
  TEMPLATE_SPECIALIZE auto create<framebuffer>() noexcept -> framebuffer *
  {
    auto h = 0u;
    return glGenFramebuffers(1, &h), glCheckError(), convert<framebuffer>(h);
  }
  TEMPLATE_SPECIALIZE auto create<vertexarray>() noexcept -> vertexarray *
  {
    auto h = 0u;
    return glGenVertexArrays(1, &h), glCheckError(), convert<vertexarray>(h);
  }
  TEMPLATE_SPECIALIZE auto destroy(shader *p) noexcept -> void
  {
    glDeleteShader(convert(p)), glCheckError();
  }
  TEMPLATE_SPECIALIZE auto destroy(program *p) noexcept -> void
  {
    glDeleteProgram(convert(p)), glCheckError();
  }
  TEMPLATE_SPECIALIZE auto destroy(buffer *p) noexcept -> void
  {
    auto h = convert(p);
    glDeleteBuffers(1, &h), glCheckError();
  }
  TEMPLATE_SPECIALIZE auto destroy(texture *p) noexcept -> void
  {
    auto h = convert(p);
    glDeleteTextures(1, &h), glCheckError();
  }
  TEMPLATE_SPECIALIZE auto destroy(framebuffer *p) noexcept -> void
  {
    auto h = convert(p);
    glDeleteFramebuffers(1, &h), glCheckError();
  }
  TEMPLATE_SPECIALIZE auto destroy(vertexarray *p) noexcept -> void
  {
    auto h = convert(p);
    glDeleteVertexArrays(1, &h), glCheckError();
  }
#undef TEMPLATE_SPECIALIZE
}
namespace game::render::object // shader
{
  shader::shader(type t, std::string_view glsl)
      : m_handle{unique<handle::shader, handle::deleter>{convert<handle::shader>(
            gl::make_shader(gl::to_GLenum(t), glsl))}} {}
}
namespace game::render::object // program
{
  program::program(std::string_view vert_glsl, std::string_view frag_glsl)
      : m_handle{unique<handle::program, handle::deleter>{convert<handle::program>(
            gl::make_program(vert_glsl, frag_glsl))}},
        m_uniform_locations{} {}
  auto program::uniform_prep() -> void { glUseProgram(convert(m_handle.get())), glCheckError(); }
  auto program::uniform(int32_t location, std::span</*                 */ glm::u32 const> values) -> void { glUniform1uiv /*      */ (location, (GLsizei)values.size(), &values[0]), glCheckError(); }
  auto program::uniform(int32_t location, std::span</*                 */ glm::i32 const> values) -> void { glUniform1iv /*       */ (location, (GLsizei)values.size(), &values[0]), glCheckError(); }
  auto program::uniform(int32_t location, std::span</*                 */ glm::f32 const> values) -> void { glUniform1fv /*       */ (location, (GLsizei)values.size(), &values[0]), glCheckError(); }
  auto program::uniform(int32_t location, std::span<glm::vec<1, /*    */ glm::u32> const> values) -> void { glUniform1uiv /*      */ (location, (GLsizei)values.size(), &values[0][0]), glCheckError(); }
  auto program::uniform(int32_t location, std::span<glm::vec<1, /*    */ glm::i32> const> values) -> void { glUniform1iv /*       */ (location, (GLsizei)values.size(), &values[0][0]), glCheckError(); }
  auto program::uniform(int32_t location, std::span<glm::vec<1, /*    */ glm::f32> const> values) -> void { glUniform1fv /*       */ (location, (GLsizei)values.size(), &values[0][0]), glCheckError(); }
  auto program::uniform(int32_t location, std::span<glm::vec<2, /*    */ glm::u32> const> values) -> void { glUniform2uiv /*      */ (location, (GLsizei)values.size(), &values[0][0]), glCheckError(); }
  auto program::uniform(int32_t location, std::span<glm::vec<2, /*    */ glm::i32> const> values) -> void { glUniform2iv /*       */ (location, (GLsizei)values.size(), &values[0][0]), glCheckError(); }
  auto program::uniform(int32_t location, std::span<glm::vec<2, /*    */ glm::f32> const> values) -> void { glUniform2fv /*       */ (location, (GLsizei)values.size(), &values[0][0]), glCheckError(); }
  auto program::uniform(int32_t location, std::span<glm::vec<3, /*    */ glm::u32> const> values) -> void { glUniform3uiv /*      */ (location, (GLsizei)values.size(), &values[0][0]), glCheckError(); }
  auto program::uniform(int32_t location, std::span<glm::vec<3, /*    */ glm::i32> const> values) -> void { glUniform3iv /*       */ (location, (GLsizei)values.size(), &values[0][0]), glCheckError(); }
  auto program::uniform(int32_t location, std::span<glm::vec<3, /*    */ glm::f32> const> values) -> void { glUniform3fv /*       */ (location, (GLsizei)values.size(), &values[0][0]), glCheckError(); }
  auto program::uniform(int32_t location, std::span<glm::vec<4, /*    */ glm::u32> const> values) -> void { glUniform4uiv /*      */ (location, (GLsizei)values.size(), &values[0][0]), glCheckError(); }
  auto program::uniform(int32_t location, std::span<glm::vec<4, /*    */ glm::i32> const> values) -> void { glUniform4iv /*       */ (location, (GLsizei)values.size(), &values[0][0]), glCheckError(); }
  auto program::uniform(int32_t location, std::span<glm::vec<4, /*    */ glm::f32> const> values) -> void { glUniform4fv /*       */ (location, (GLsizei)values.size(), &values[0][0]), glCheckError(); }
  auto program::uniform(int32_t location, std::span<glm::mat<4, 4, /* */ glm::f32> const> values) -> void { glUniformMatrix4fv /* */ (location, (GLsizei)values.size(), GL_FALSE, &values[0][0][0]), glCheckError(); }
  auto program::uniform_location(std::string_view name) const noexcept -> int32_t
  {
    auto it = m_uniform_locations.find(name);
    if (it not_eq m_uniform_locations.end())
      return it->second.second;
    auto name_str = std::make_unique<char[]>(name.size() + 1);
    std::strncpy(name_str.get(), name.data(), name.size())[name.size()] = '\0';
    auto location = glGetUniformLocation(convert(m_handle.get()), name_str.get());
    glCheckError();
    auto name_sv = std::string_view{name_str.get(), name.size()};
    m_uniform_locations.insert({name_sv, {std::move(name_str), location}});
    return location;
  }
}
namespace game::render::object // buffer
{
  buffer::buffer(target t, size_t size, usage u, shared<handle::buffer> handle)
      : m_handle{handle ? std::move(handle) : handle::create_shared<handle::buffer>()},
        m_target{t},
        m_usage{u},
        m_size{size}
  {
    utils::assertf(m_handle, "%s", "failed to create buffer handle");
    utils::assertf(t != target::none, "%s", "must provide a target for a buffer object");
  }
  auto buffer::upload_bytes(std::span<std::byte const> bytes, usage u) -> void
  {
    m_size = (size_t)bytes.size_bytes();
    m_usage = u != usage::none ? u : m_usage;
    glBindBuffer(gl::to_GLenum(m_target), convert(m_handle.get()));
    glBufferData(gl::to_GLenum(m_target), m_size, bytes.data(), gl::to_GLenum(m_usage));
    glCheckError();
  }
  auto buffer::update_bytes(std::span<std::byte const> bytes, size_t offset) -> void
  {
    glBindBuffer(gl::to_GLenum(m_target), convert(m_handle.get()));
    glBufferSubData(gl::to_GLenum(m_target), offset, (size_t)bytes.size_bytes(), bytes.data());
    glCheckError();
  }
}
namespace game::render::object // texture
{
  texture::texture(target t, source source, shared<handle::texture> handle)
      : m_handle{handle ? std::move(handle) : handle::create_shared<handle::texture>()},
        m_target{t},
        m_format{source.format},
        m_size{source.size}
  {
    utils::assertf(m_target != target::none, "texture %s was none", "target");
    utils::assertf(m_format != format::none, "texture %s was none", "format");
    utils::assertf(m_size.x and m_size.y and m_size.z, "texture size {%u,%u,%u} can not contain 0s", m_size.x, m_size.y, m_size.z);
    auto const target = gl::to_GLenum(m_target),
               format = gl::to_GLenum(m_format);
    auto const target_component_count = ((int)m_target bitand 0xf0) >> 4;
    auto const [format_component_GLenum, format_component_count, format_type_GLenum, format_type_size] = gl::texture_format_details(source.format);
    utils::assertf(target_component_count == format_component_count, "texture target expects %i components but format expects %i", target_component_count, format_component_count);
    glBindTexture(target, convert(m_handle.get()));
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST), glCheckError();
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST), glCheckError();
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT), glCheckError();
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT), glCheckError();
    glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_MIRRORED_REPEAT), glCheckError();
    /**/ if (target_component_count == 2)
      glTexStorage2D(target, 1, format, m_size.x, m_size.y), glCheckError();
    else if (target_component_count == 3)
      glTexStorage3D(target, 1, format, m_size.x, m_size.y, m_size.z), glCheckError();
    else
      utils::assertf(false, "Can not %s for texture target: %i", "reserve storage", (int)m_target);
    upload(source);
  }
  auto texture::upload(source source, glm::uvec3 position) -> void
  {
    auto const target = gl::to_GLenum(m_target);
    auto const target_comp = ((int)m_target >> 4) bitand 0xf;
    auto const [format_component_GLenum, format_component_count, format_type_GLenum, format_type_size] = gl::texture_format_details(source.format);
    auto const expected_subpixels_size = format_type_size * format_component_count * COMPONENT_WISE (*)(glm::max(source.size, {1, 1, 1}));
    /**/ if (source.subpixels.data())
      utils::assertf(source.subpixels.size() <= expected_subpixels_size, "Expected at least %zub of data for texture. Got %zub", expected_subpixels_size, source.subpixels.size());
    glBindTexture(target, convert(m_handle.get()));
    /**/ if (target_comp == 2)
      glTexSubImage2D(target, 0,
                      position.x, position.y,
                      source.size.x, source.size.y,
                      format_component_GLenum, format_type_GLenum, source.subpixels.data()),
          glCheckError();
    else if (target_comp == 3)
      glTexSubImage3D(target, 0,
                      position.x, position.y, position.z,
                      source.size.x, source.size.y, source.size.z,
                      format_component_GLenum, format_type_GLenum, source.subpixels.data()),
          glCheckError();
    else
      utils::assertf(false, "Can not %s for texture target: %i", "upload data", (int)m_target);
  }
}
namespace game::render::object // framebuffer
{

}
namespace game::render::object // vertexarray
{
  vertexarray::vertexarray(std::span<attribute const> attribs, shared<handle::vertexarray> handle) noexcept
      : m_handle{handle ? handle : handle::create_shared<handle::vertexarray>()},
        m_attribs{} { set_attribs(attribs); }
  auto vertexarray::bind() noexcept
  {
    glCheckError();
    glBindVertexArray(convert(m_handle.get())), glCheckError();
  }
  auto vertexarray::set_attrib(uint32_t i, attribute attrib) noexcept -> void
  {
    if (not m_attribs)
      m_attribs = std::make_shared<decltype(m_attribs)::element_type>();
    if (m_attribs->size() <= i)
      m_attribs->resize(i + 1);
    m_attribs->at(i) = attrib;
    auto const [size, type] = gl::vertex_attrib_details(attrib.value_type);
    glCheckError();
    glBindVertexArray(convert(m_handle.get())), glCheckError();
    glBindBuffer(GL_ARRAY_BUFFER, convert(attrib.buffer.get())), glCheckError();
    if (type == GL_HALF_FLOAT or type == GL_FLOAT)
      glVertexAttribPointer(i, size, type, false, attrib.stride, (void *)(std::uintptr_t)attrib.offset), glCheckError();
    else
      glVertexAttribIPointer(i, size, type, attrib.stride, (void *)(std::uintptr_t)attrib.offset), glCheckError();
    glVertexAttribDivisor(i, attrib.divisor), glCheckError();
    glBindVertexArray(0), glCheckError();
  }
  auto vertexarray::set_attribs(std::span<attribute const> attribs) noexcept -> void
  {
    m_attribs = std::make_shared<decltype(m_attribs)::element_type>(attribs.size());
    for (auto i = 0u; i < attribs.size(); i++)
      set_attrib(i, attribs[i]);
  }
  auto vertexarray::set_attrib_enabled(uint32_t i, bool enabled) noexcept -> void
  {
    glCheckError();
    glBindVertexArray(convert(m_handle.get())), glCheckError();
    if (enabled)
      glEnableVertexAttribArray(i);
    else
      glDisableVertexAttribArray(i);
    glBindVertexArray(0), glCheckError();
  }
}
namespace game::render::text // font
{
  font::font(glm::u32 max_codepoint, glm::uvec2 glyph_size)
      : m_texture{},
        m_glyph_size{glyph_size}
  {
    auto max_texture_width = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_width);
    utils::assertf(max_texture_width > 0, "%s", "Failed to retrieve maximum texture width from OpenGL");
    auto const max_page_pixels_size = glm::uvec2{max_texture_width, max_texture_width};
    auto const page_glyphs_size = max_page_pixels_size / glyph_size;
    auto const page_count = max_codepoint / (1u + COMPONENT_WISE (*)(page_glyphs_size));
    auto const size = glm::max(glm::uvec3{1, 1, 1}, glm::uvec3{page_glyphs_size, page_count});
    m_texture = {
        texture::target::texture2_array,
        texture::source{
            .format = texture::format::rgba8,
            .size = glm::uvec3{glyph_size, 1} * size,
        },
    };
  }
  auto game::render::text::font::get_atlas_glyphs_position(glm::u32 codepoint) const noexcept -> glm::uvec3
  {
    auto const atlas_glyphs_size = get_atlas_glyphs_size();
    return glm::uvec3{
        /**/ ((codepoint - 1u) % (atlas_glyphs_size.x * atlas_glyphs_size.y)) % atlas_glyphs_size.x,
        /**/ ((codepoint - 1u) % (atlas_glyphs_size.x * atlas_glyphs_size.y)) / atlas_glyphs_size.x,
        /**/ ((codepoint - 1u) / (atlas_glyphs_size.x * atlas_glyphs_size.y)) //
    };
  }
  auto font::upload_glyph(uint32_t codepoint, texture::source source) -> void
  {
    auto const size = get_glyph_pixels_size(),
               position = size * get_atlas_glyphs_position(codepoint);
    utils::assertf(source.size == size, "Glyph texture source size was wrong. Expected {%u,%u,%u}. Got {%u,%u,%u}", size.x, size.y, size.z, source.size.x, source.size.y, source.size.z);
    m_texture.upload(source, position);
  }
  auto font::upload_glyphs(uint32_t codepoint_begin, texture::source source) -> decltype(codepoint_begin)
  {
    auto const glyph_pixels_size = get_glyph_pixels_size();
    auto const source_glyphs_size = source.size / glyph_pixels_size;
    auto const [format_component_GLenum, format_component_count, format_type_GLenum, format_type_size] = gl::texture_format_details(source.format);
    auto const pixel_bytes_size = format_component_count * format_type_size;
    auto codepoint = codepoint_begin;
    auto glyph_subpixels = std::vector<std::byte>{};
    glyph_subpixels.reserve((size_t)COMPONENT_WISE (*)(glyph_pixels_size) * pixel_bytes_size);
    for (auto it = glm::uvec3{-1, 0, 0},
              it_size = source_glyphs_size;
         (/* for x */ ++it.x < it_size.x or (it.x = 0)) or
         (/* for y */ ++it.y < it_size.y or (it.y = 0)) or
         (/* for z */ ++it.z < it_size.z or (it.z = 0)) or
         (/* end */ 0);)
    {
      auto const source_glyphs_position = it;
      glyph_subpixels.clear();
      for (auto it = glm::uvec3{-1, 0, 0},
                it_size = glyph_pixels_size;
           (/* for x */ ++it.x < it_size.x or (it.x = 0)) or
           (/* for y */ ++it.y < it_size.y or (it.y = 0)) or
           (/* for z */ ++it.z < it_size.z or (it.z = 0)) or
           (/* end */ 0);)
      {
        auto const source_pixels_position = it + source_glyphs_position * glyph_pixels_size;
        auto const source_pixels_offset = COMPONENT_WISE (*)(source_pixels_position *glm::uvec3{1, source.size.x, source.size.x * source.size.y});
        utils::assertf(source_pixels_offset * pixel_bytes_size + pixel_bytes_size <= source.subpixels.size_bytes(), "%s", "source.subpixels overrun");
        auto const source_pixel_subpixels = source.subpixels.subspan(source_pixels_offset * pixel_bytes_size, pixel_bytes_size);
        for (auto source_pixel_subpixel : source_pixel_subpixels)
          glyph_subpixels.push_back(source_pixel_subpixel);
      }
      upload_glyph(
          codepoint++,
          texture::source{
              .format = source.format,
              .size = glyph_pixels_size,
              .subpixels = glyph_subpixels,
          });
    }
    return codepoint;
  }
}
namespace game::render::text // string
{
  auto string::set_string(std::u8string_view utf8) noexcept -> void
  {
    m_vector.clear();
    m_vector.reserve(utf8.size() / 2);
    auto cursor_position = glm::vec2{0.0f, 0.0f};
    while (not utf8.empty())
    {
      auto [codepoint, advance] = utils::to_utf32(utf8);
      codepoint = codepoint == -1u ? U'?' : codepoint;
      utf8 = utf8.substr(std::max<size_t>(1u, advance));
      m_vector.push_back({
          .codepoint = codepoint,
          .position = cursor_position,
          .size = {1, 1},
      });
      cursor_position = codepoint == '\n' ? glm::vec2{0.0f, cursor_position.y + 1.0f}
                                          : cursor_position + glm::vec2{m_vector.back().size.x, 0.0f};
    }
  }
  auto string::get_string() const noexcept -> std::u8string
  {
    auto utf8 = std::u8string{};
    utf8.reserve(m_vector.size() * 2u);
    for (auto const &character : m_vector)
      for (auto u8 : utils::to_utf8(character.codepoint))
        if (u8)
          utf8.push_back(u8);
        else
          break;
    return utf8;
  }
  auto string::update_rendering() noexcept -> void
  {
    if (not m_buffer.get_handle())
      m_buffer = {buffer::target::array, sizeof(character), buffer::usage::static_draw};
    if (not m_vertexarray.get_handle())
      m_vertexarray = std::array{
          vertexarray::attribute{.buffer = m_buffer.get_handle(), .divisor = 1}.with_type(&character::codepoint /* */),
          vertexarray::attribute{.buffer = m_buffer.get_handle(), .divisor = 1}.with_type(&character::position /*  */),
          vertexarray::attribute{.buffer = m_buffer.get_handle(), .divisor = 1}.with_type(&character::size /*      */),
      };
    m_buffer.upload(m_vector, buffer::usage::static_draw);
  }
}
