#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "common.hpp"
#include "render.hpp"
#include "entity.hpp"

struct GLFWwindow;
struct lua_State;

namespace game
{
  struct application
  {
    application(application const &) noexcept = delete;
    application &operator=(application const &) noexcept = delete;

    application();
    ~application();
    auto run() -> int;

  private:
    auto setup() -> void;
    auto events() -> bool;
    auto update() -> void;
    auto render() -> void;
    auto shutdown() -> void;

  private:
    std::shared_ptr<GLFWwindow> m_window{};
    std::shared_ptr<lua_State> m_L{};

    render::program m_program{};
    render::font m_font{};

    b2WorldId m_world_id = b2_nullWorldId;
    entt::registry m_registry{};

    double m_time_update_stamp = 0, m_dt = 1.0 / 60.0;
    bool m_running : 1;
  };
}

#endif // APPLICATION_HPP