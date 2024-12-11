#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "common.hpp"
#include "render.hpp"
#include "script.hpp"

namespace game
{
  struct application
  {
    application();
    ~application();
    auto run() -> void;
    auto loop() -> bool;
    auto update(double dt) -> void;
    auto draw() -> void;

  public:
    auto inline get_renderer(this auto &self) noexcept -> auto { return self.m_renderer ? &*self.m_renderer : nullptr; }
    auto inline get_lua_state(this auto &self) noexcept -> auto { return self.m_L; }

  private:
    std::shared_ptr<GLFWwindow> m_window = nullptr;
    std::optional<render::tile::renderer> m_renderer;
    std::shared_ptr<lua_State> m_L = nullptr;
    size_t m_update_tik = 0;
    double m_update_start = 0.0,
           m_update_dt = 1.0 / 30.0,
           m_update_max_catchup = 1.0 / 60.0;
  };
  auto inline global_application = (application *)0;
}

#endif // APPLICATION_HPP