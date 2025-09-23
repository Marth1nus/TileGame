#ifndef RENDER_HPP
#define RENDER_HPP

#include "common.hpp"

namespace game::render
{
  template <typename T, typename D>
  using unique = std::unique_ptr<T, D>;
  template <typename T>
  using shared = std::shared_ptr<T>; // Note: render::shared pointers do not need atomic ref counting. Consider non std::shared_ptr solution.
}
namespace game::render::handle
{
  template <typename T>
  auto inline static constexpr is_handle = false;
#define DEFINE_HANDLE_TYPE(name) \
  struct name;                   \
  template <>                    \
  auto inline /*  */ constexpr is_handle<name> = true;
  DEFINE_HANDLE_TYPE(shader);
  DEFINE_HANDLE_TYPE(program);
  DEFINE_HANDLE_TYPE(buffer);
  DEFINE_HANDLE_TYPE(texture);
  DEFINE_HANDLE_TYPE(framebuffer);
  DEFINE_HANDLE_TYPE(vertexarray);
#undef DEFINE_HANDLE_TYPE
  template <typename T>
  concept handle = is_handle<T>;

  template <handle T>
  auto inline convert(uint32_t h) noexcept -> T * { return reinterpret_cast<T *>(static_cast<std::uintptr_t>(h)); }
  template <handle T>
  auto inline convert(T *h) noexcept -> uint32_t { return static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(h)); }

  template <handle T>
  auto create() noexcept -> T *;
  template <handle T>
  auto destroy(T *) noexcept -> void;
  using deleter = decltype([](handle auto *p) -> void
                           { return destroy(p); });
  template <handle T>
  auto inline create_unique() noexcept { return unique<T, deleter>{create<T>()}; }
  template <handle T>
  auto inline create_shared() noexcept { return shared<T>{create_unique<T>()}; }

  enum struct shader_type : uint8_t
  {
    none,
    vertex,
    fragment
  };
  template <>
  auto create<shader>() noexcept -> shader * = delete; // C++26 : = delete("create<shader>() is deleted. Use create<shader>(shader_type) instead");
  template <>
  auto inline create_unique<shader>() noexcept = delete; // C++26 : = delete("create_unique<shader>() is deleted. Use create_unique<shader>(shader_type) instead");
  template <>
  auto inline create_shared<shader>() noexcept = delete; // C++26 : = delete("create_shared<shader>() is deleted. Use create_shared<shader>(shader_type) instead");
  template <std::same_as<shader> T>
  auto create(shader_type shader_type) noexcept -> shader *;
  template <std::same_as<shader> T>
  auto inline create_unique(shader_type t) noexcept { return unique<T, deleter>{create<T>(t)}; }
  template <std::same_as<shader> T>
  auto inline create_shared(shader_type t) noexcept { return shared<T>{create_unique<T>(t)}; }
}
namespace game::render::object
{
  struct shader
  {
  public:
    shader() noexcept = default;
    shader(shader &&) noexcept = default;
    shader(shader const &) noexcept = default;
    shader &operator=(shader &&value) noexcept = default;
    shader &operator=(shader const &value) noexcept = default;
    ~shader() noexcept = default;

    using type = handle::shader_type;
    shader(shared<handle::shader> handle) noexcept : m_handle{std::move(handle)} {}
    shader(type t, std::string_view glsl);

    auto inline get_handle() const noexcept { return m_handle; }

  private:
    shared<handle::shader> m_handle;
  };
  struct program
  {
  public:
    program() noexcept = default;
    program(program &&) noexcept = default;
    program(program const &) noexcept = default;
    program &operator=(program &&value) noexcept = default;
    program &operator=(program const &value) noexcept = default;
    ~program() noexcept = default;

    program(shared<handle::program> handle) noexcept : m_handle{std::move(handle)} {}
    program(std::string_view vert_glsl, std::string_view frag_glsl);

    auto uniform_location(std::string_view name) const noexcept -> int32_t;
    auto uniform_location_cache_clear() -> auto { return m_uniform_locations.clear(); }
    auto uniform_location_cache_clear(std::string_view name) -> auto { return m_uniform_locations.erase(name); }

    auto uniform_prep() -> void;
    auto static uniform(int32_t location, std::span</*                 */ glm::u32 const> values) -> void;
    auto static uniform(int32_t location, std::span</*                 */ glm::i32 const> values) -> void;
    auto static uniform(int32_t location, std::span</*                 */ glm::f32 const> values) -> void;
    auto static uniform(int32_t location, std::span<glm::vec<1, /*    */ glm::u32> const> values) -> void;
    auto static uniform(int32_t location, std::span<glm::vec<1, /*    */ glm::i32> const> values) -> void;
    auto static uniform(int32_t location, std::span<glm::vec<1, /*    */ glm::f32> const> values) -> void;
    auto static uniform(int32_t location, std::span<glm::vec<2, /*    */ glm::u32> const> values) -> void;
    auto static uniform(int32_t location, std::span<glm::vec<2, /*    */ glm::i32> const> values) -> void;
    auto static uniform(int32_t location, std::span<glm::vec<2, /*    */ glm::f32> const> values) -> void;
    auto static uniform(int32_t location, std::span<glm::vec<3, /*    */ glm::u32> const> values) -> void;
    auto static uniform(int32_t location, std::span<glm::vec<3, /*    */ glm::i32> const> values) -> void;
    auto static uniform(int32_t location, std::span<glm::vec<3, /*    */ glm::f32> const> values) -> void;
    auto static uniform(int32_t location, std::span<glm::vec<4, /*    */ glm::u32> const> values) -> void;
    auto static uniform(int32_t location, std::span<glm::vec<4, /*    */ glm::i32> const> values) -> void;
    auto static uniform(int32_t location, std::span<glm::vec<4, /*    */ glm::f32> const> values) -> void;
    auto static uniform(int32_t location, std::span<glm::mat<4, 4, /* */ glm::f32> const> values) -> void;

    template <typename T>
      requires((std::ranges::contiguous_range<T> and utils::arithmetic_vec_or_scalar<std::ranges::range_value_t<T>>) or utils::arithmetic_vec_or_scalar<T>)
    auto inline uniform(std::string_view name, T const &value) -> decltype(auto)
    {
      /**/ if constexpr (utils::arithmetic_vec_or_scalar<T>)
        return uniform(/*           */ (name), std::span{&value, 1});
      else if constexpr (not std::ranges::contiguous_range<T>)
        static_assert(false);
      else if constexpr (utils::arithmetic_vec_or_scalar<std::ranges::range_value_t<T>>)
        return uniform(uniform_location(name), std::span{value});
      else
        static_assert(false);
    }

  private:
    shared<handle::program> m_handle{};
    std::unordered_map<std::string_view, std::pair<std::unique_ptr<char[]>, int32_t>> mutable m_uniform_locations{};
  };
  struct buffer
  {
  public:
    enum struct target : uint8_t
    {
      none,
      array,
      element,
      uniform,
    };
    enum struct usage : uint8_t
    {
      none,
      static_copy,
      static_draw,
      static_read,
      dynamic_copy,
      dynamic_draw,
      dynamic_read,
    };
    using size_t = int32_t;

  public:
    buffer() noexcept = default;
    buffer(buffer &&) noexcept = default;
    buffer(buffer const &) noexcept = default;
    buffer &operator=(buffer &&value) noexcept = default;
    buffer &operator=(buffer const &value) noexcept = default;
    ~buffer() noexcept = default;

    buffer(shared<handle::buffer> handle, target target, usage usage = {}, size_t size = {}) : m_handle{std::move(handle)}, m_target{target}, m_usage{usage}, m_size{size} {}
    buffer(target t, size_t size = {}, usage u = {}, shared<handle::buffer> handle = {});

    auto upload_bytes(std::span<std::byte const> bytes, usage u) -> void;
    auto update_bytes(std::span<std::byte const> bytes, size_t offset = 0) -> void;

    auto inline upload(std::ranges::contiguous_range auto const &values, usage u) -> auto { return upload_bytes(std::as_bytes(std::span{values}), u); }
    auto inline update(std::ranges::contiguous_range auto const &values, size_t offset = 0) -> auto { return update_bytes(std::as_bytes(std::span{values}), offset * (size_t)sizeof(std::span{values}[0])); }

    auto inline get_handle() const noexcept { return m_handle; }
    auto inline get_target() const noexcept { return m_target; }
    auto inline get_usage() const noexcept { return m_usage; }
    auto inline get_size() const noexcept { return m_size; }

  private:
    shared<handle::buffer> m_handle;
    target m_target;
    usage m_usage;
    size_t m_size;
  };
  struct texture
  {
  public:
    enum struct target : uint8_t
    {
      none,
      // 2D
      texture2 = 0x20,
      // 3D
      texture2_array = 0x30,
      texture3,
    };
    enum struct format : uint8_t
    {
      none,
      /* clang-format off */    r8 = 0x10,    r8_unorm =    r8,    r8_snorm,    r16f,    r32f,    r8ui,    r16ui,    r32ui,    r8i,    r16i,    r32i,    /* clang-format on */
      /* clang-format off */   rg8 = 0x20,   rg8_unorm =   rg8,   rg8_snorm,   rg16f,   rg32f,   rg8ui,   rg16ui,   rg32ui,   rg8i,   rg16i,   rg32i,   /* clang-format on */
      /* clang-format off */  rgb8 = 0x30,  rgb8_unorm =  rgb8,  rgb8_snorm,  rgb16f,  rgb32f,  rgb8ui,  rgb16ui,  rgb32ui,  rgb8i,  rgb16i,  rgb32i,  /* clang-format on */
      /* clang-format off */ rgba8 = 0x40, rgba8_unorm = rgba8, rgba8_snorm, rgba16f, rgba32f, rgba8ui, rgba16ui, rgba32ui, rgba8i, rgba16i, rgba32i, /* clang-format on */
    };
    struct source
    {
      texture::format format{};
      glm::uvec3 size{};
      std::span<std::byte const> subpixels{};
    };

  public:
    texture() noexcept = default;
    texture(texture &&) noexcept = default;
    texture(texture const &) noexcept = default;
    texture &operator=(texture &&value) noexcept = default;
    texture &operator=(texture const &value) noexcept = default;
    ~texture() noexcept = default;

    texture(shared<handle::texture> handle, target target, format format, glm::uvec3 size) : m_handle{std::move(handle)}, m_target{target}, m_format{format}, m_size{size} {}
    texture(target target, source src, shared<handle::texture> handle = {});

    auto upload(source src, glm::uvec3 position = {0, 0, 0}) -> void;

    auto inline get_handle() const noexcept { return m_handle; }
    auto inline get_target() const noexcept { return m_target; }
    auto inline get_format() const noexcept { return m_format; }
    auto inline get_size() const noexcept { return m_size; }

  private:
    shared<handle::texture> m_handle;
    target m_target;
    format m_format;
    glm::uvec3 m_size;
  };
  struct framebuffer
  {
  public:
    framebuffer() noexcept = default;
    framebuffer(framebuffer &&) noexcept = default;
    framebuffer(framebuffer const &) noexcept = default;
    framebuffer &operator=(framebuffer &&value) noexcept = default;
    framebuffer &operator=(framebuffer const &value) noexcept = default;
    ~framebuffer() noexcept = default;

    framebuffer(shared<handle::framebuffer> handle) noexcept : m_handle{std::move(handle)} {}

    auto inline get_handle() const noexcept { return m_handle; }

  private:
    shared<handle::framebuffer> m_handle;
  };
  struct vertexarray
  {
  public:
    struct attribute
    {
    public:
      enum struct type : uint8_t
      {
        none,
        /*# [0111..0344]
          |      | type | size | count |
          | ---: | ---: | ---: | ----: |
          |    1 |    u |    8 |     1 |
          |    2 |    i |   16 |     2 |
          |    3 |    f |   32 |     3 |
          |    4 |      |   64 |     4 |
          | mask | 0300 | 0070 |  0007 |
        */
        range_vec_first = 0111,
        /* clang-format off */  u8 = 0111,  u8vec1 =  u8,  u8vec2,  u8vec3,  u8vec4, /* clang-format on */
        /* clang-format off */  i8 = 0211,  i8vec1 =  i8,  i8vec2,  i8vec3,  i8vec4, /* clang-format on */
        // clang-format off */  f8 = 0311,  f8vec1 =  f8,  f8vec2,  f8vec3,  f8vec4, /* clang-format on */
        /* clang-format off */ u16 = 0121, u16vec1 = u16, u16vec2, u16vec3, u16vec4, /* clang-format on */
        /* clang-format off */ i16 = 0221, i16vec1 = i16, i16vec2, i16vec3, i16vec4, /* clang-format on */
        /* clang-format off */ f16 = 0321, f16vec1 = f16, f16vec2, f16vec3, f16vec4, /* clang-format on */
        /* clang-format off */ u32 = 0131, u32vec1 = u32, u32vec2, u32vec3, u32vec4, /* clang-format on */
        /* clang-format off */ i32 = 0231, i32vec1 = i32, i32vec2, i32vec3, i32vec4, /* clang-format on */
        /* clang-format off */ f32 = 0331, f32vec1 = f32, f32vec2, f32vec3, f32vec4, /* clang-format on */
        range_vec_last = 0344,
      };

    public:
      shared<handle::buffer> buffer{};
      type value_type{};
      glm::u32 offset{}, stride{}, divisor{};

    public:
      template <utils::arithmetic_vec_or_scalar T>
      auto inline static constexpr type_of = []() -> attribute::type
      {
        using V = std::conditional_t<utils::arithmetic<T>, glm::vec<1, T>, T>;
        static_assert(utils::arithmetic_vec<V>, "Well then... How did we get here?! Invalid attribute type. This should be very impossible.");
        auto constexpr type = std::unsigned_integral<typename V::value_type> ? 1u
                              : std::signed_integral<typename V::value_type> ? 2u
                              : std::floating_point<typename V::value_type>  ? 3u
                                                                             : 0u,
                       size = 1u + std::countr_zero(sizeof(typename V::value_type)),
                       count = (uint32_t)V::length();
        if constexpr (not((1u <= type /*  */ and type /*  */ <= 3u) and
                          (1u <= size /*  */ and size /*  */ <= 3u) and
                          (1u <= count /* */ and count /* */ <= 4u) and
                          not(type == 3u and size == 1u)))
          return attribute::type::none;
        else
          return attribute::type{((type /*  */ bitand 0300u) >> std::popcount(0077u)) bitor
                                 ((size /*  */ bitand 0070u) >> std::popcount(0007u)) bitor
                                 ((count /* */ bitand 0007u) >> std::popcount(0000u))};
      }();
      template <utils::arithmetic_vec_or_scalar T>
      auto inline constexpr with_type(this attribute self, T *) noexcept -> decltype(self)
      {
        self.value_type = type_of<T>;
        return self;
      }
      template <typename MT, utils::arithmetic_vec_or_scalar T>
      auto inline constexpr with_type(this attribute self, T MT::*member) noexcept -> decltype(self)
      {
        self.value_type = type_of<T>;
        self.offset = static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(&(reinterpret_cast<MT *>(0)->*member)));
        self.stride = static_cast<uint32_t>(sizeof(MT));
        return self;
      }
    };

  public:
    vertexarray() noexcept = default;
    vertexarray(vertexarray &&) noexcept = default;
    vertexarray(vertexarray const &) noexcept = default;
    vertexarray &operator=(vertexarray &&value) noexcept = default;
    vertexarray &operator=(vertexarray const &value) noexcept = default;
    ~vertexarray() noexcept = default;

    vertexarray(shared<handle::vertexarray> handle) noexcept : m_handle{std::move(handle)} {}
    vertexarray(std::span<attribute const> attribs, shared<handle::vertexarray> handle) noexcept;
    vertexarray(std::convertible_to<std::span<attribute const>> auto &&attribs) noexcept : vertexarray{std::span<attribute const>{attribs}, {}} {}

    auto bind() noexcept;
    auto set_attrib(uint32_t i, attribute attrib) noexcept -> void;
    auto set_attribs(std::span<attribute const> attribs) noexcept -> void;
    auto set_attrib_enabled(uint32_t i, bool enabled) noexcept -> void;

    auto inline get_handle() const noexcept { return m_handle; }
    auto inline get_attribs() const noexcept -> std::span<attribute const> { return m_attribs ? std::span{*m_attribs} : std::span<attribute>{}; }

  private:
    shared<handle::vertexarray> m_handle{};
    std::shared_ptr<std::vector<attribute>> m_attribs{};
  };
}
namespace game::render // using object::
{
  using object::shader,
      object::program,
      object::buffer,
      object::texture,
      object::framebuffer,
      object::vertexarray;
}

#endif // RENDER_HPP