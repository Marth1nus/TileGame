#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "common.hpp"
#include "render.hpp"

struct GLFWwindow;
struct lua_State;
#include <box2d/box2d.h>

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
    std::shared_ptr<GLFWwindow> m_window;

    render::renderer m_renderer;
    std::vector<render::tile_mesh> m_tile_meshes;

    b2WorldId m_world_id = b2_nullWorldId;
    std::vector<b2BodyId> m_bodies;

    std::shared_ptr<lua_State> m_L;

    double m_time_update_stamp = 0, m_dt = 1.0 / 30.0;
    bool m_running : 1;
  };
}

#endif // APPLICATION_HPP