#include "application.hpp"

#include <lua.hpp>
#include <GLFW/glfw3.h>

namespace game
{
  application::application()
  {
    utils::assert(not global_application, "Multiple Applications?");
    global_application = this;
    { // Window
      struct glfw
      {
        glfw() { utils::assertf(glfwInit(), "%s init fail", "glfw"); }
        ~glfw() { glfwTerminate(); }
      } static const glfw;
      auto const width = 720, height = width;
      auto const title = "TileGame";
      auto const window = utils::assertf(glfwCreateWindow(width, height, title, 0, 0), "%s init fail", "window");
      m_window = {window, glfwDestroyWindow};
      glfwMakeContextCurrent(window);
      utils::assertf(gladLoadGLES2(glfwGetProcAddress), "%s init fail", "glad");
      glViewport(0, 0, width, height);
    }
    { // Renderer
      auto &r = m_renderer.emplace();
      using namespace render::tile;
      r.reload(utils::file_read_all("res/shaders/vert.glsl"),
               utils::file_read_all("res/shaders/frag.glsl"));
      auto constexpr w = 8;
      r.set_projection() = glm::ortho<float>(-w, w, w, -w);
    }
    { // Lua
      auto const L = (m_L = {luaL_newstate(), lua_close}).get();
      utils::assertf(L, "%s init fail", "lua");
      luaL_openlibs(L);
      script::lua::openlibs(L);
      script::lua::link_window_events(m_window.get(), L);
      if (auto const code = "package.path = package.path .. ';res/scripts/?.lua'";
          luaL_dostring(L, code) not_eq LUA_OK)
        utils::print_errorf("Lua error in %s: %s", "--", lua_tostring(L, -1)), lua_pop(L, 1);
      if (auto const path = "res/scripts/main.lua";
          luaL_dofile(L, path) not_eq LUA_OK)
        utils::print_errorf("Lua error in %s: %s", path, lua_tostring(L, -1)), lua_pop(L, 1);
      (void)script::lua::call_global(L, std::array{"game", "setup"});
      lua_settop(L, 0);
    }
  }
  application::~application()
  {
    if (auto const L = m_L.get())
      (void)script::lua::call_global(L, std::array{"game", "shutdown"});
    m_L = {};
    m_renderer = {};
    m_window = {};
    global_application = 0;
  }
  auto application::run() -> void
  {
    m_update_start = glfwGetTime();
    m_update_tik = 0;
    while (loop())
      ;
  }
  auto application::loop() -> bool
  {
    auto static constexpr now = glfwGetTime;
    auto const evt_start = now();
    { // Events
      glfwPollEvents();
    }
    auto const evt_stop = now(), upd_start = evt_stop;
    { // Update
      auto const update_start = glfwGetTime();
      auto const real_time = update_start - m_update_start;
      auto const simulation_time = m_update_tik * m_update_dt;
      for (auto lag = real_time - simulation_time; lag > m_update_dt; lag -= m_update_dt)
      {
        auto const now = glfwGetTime();
        if (m_update_max_catchup < (now - update_start))
        {
          m_update_start = now - simulation_time;
          break;
        }
        update(m_update_dt), m_update_tik++;
      }
    }
    auto const upd_stop = now(), drw_start = upd_stop;
    { // Draw
      draw();
    }
    auto const drw_stop = now();
    if constexpr (auto constexpr display_time_widget = 1)
    {
      auto static constinit count = 120.0,
                            evt_ave = 0.0,
                            upd_ave = 0.0,
                            drw_ave = 0.0;
      evt_ave = (evt_ave * (count - 1.0) + (evt_stop - evt_start)) / count;
      upd_ave = (upd_ave * (count - 1.0) + (upd_stop - upd_start)) / count;
      drw_ave = (drw_ave * (count - 1.0) + (drw_stop - drw_start)) / count;
      std::printf("\033[s"
                  "\033[1;64H                 "
                  "\033[30;47m"
                  "\033[2;64H  event:%6.2lfms "
                  "\033[3;64H update:%6.2lfms "
                  "\033[4;64H   draw:%6.2lfms "
                  "\033[0m"
                  "\033[u",
                  evt_ave * 1000.0,
                  upd_ave * 1000.0,
                  drw_ave * 1000.0);
    }
    auto const window = m_window.get();
    glfwSwapBuffers(window);
    return not glfwWindowShouldClose(window);
  }
  auto application::update(double dt) -> void
  {
    if (auto const L = m_L.get())
      lua_settop(L, 0), lua_pushnumber(L, dt),
          (void)script::lua::call_global(L, std::array{"game", "update"}, 1);
  }
  auto application::draw() -> void
  {
    m_renderer->render();
    if (auto const L = m_L.get())
      (void)script::lua::call_global(L, std::array{"game", "draw"});
  }
}