#ifndef SCRIPT_HPP
#define SCRIPT_HPP

#include "common.hpp"
struct lua_State;
struct GLFWwindow;

namespace game::script::lua
{
  namespace event
  {
    auto push_lib(lua_State *L) -> int; // fun(): game.event
  }
  namespace input
  {
    auto push_keys(lua_State *L) -> int;          // fun(): {[string]:integer}
    auto push_mouse_buttons(lua_State *L) -> int; // fun(): {[string]:integer}
    auto push_actions(lua_State *L) -> int;       // fun(): {[string]:integer}
    auto push_lib(lua_State *L) -> int;           // fun(): game.input
  }
  namespace game
  {
    auto set_camera_ortho(lua_State *L) -> int; // fun(left: number, right: number, bottom: number, top: number)
    auto load_map /*  */ (lua_State *L) -> int; // fun(map: tiled.map)
    auto push_lib /*  */ (lua_State *L) -> int; // fun() : game
  }
  auto openlibs(lua_State *L) -> int; // fun()
}
namespace game::script::lua
{
  auto push_global(lua_State *L, std::span<char const *const> members) -> int;
  auto call_global(lua_State *L, std::span<char const *const> members, int nargs = 0, int nresults = 0) -> bool;
  auto link_window_events(GLFWwindow *window, lua_State *L) -> void;
}

#endif // SCRIPT_HPP