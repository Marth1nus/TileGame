#include "script.hpp"
#include "render.hpp"
#include "application.hpp"
#include <lua.hpp>
#include <GLfW/glfw3.h>

namespace game::script::lua
{
  namespace glfw
  {
    template <typename T, size_t N>
    struct CTA /* compile-time-array */
    {
      T arr[N];
    };
    template <auto const event_name>
    auto forward_window_event(GLFWwindow *window, auto... args) -> void
    {
      auto static constexpr on_ev_name = std::string_view{event_name.arr},
                            ev_name = on_ev_name.substr("on_"sv.size());
      static_assert(on_ev_name.starts_with("on_"), "event_name must start with 'on_'");
      auto const L = utils::assert((lua_State *)glfwGetWindowUserPointer(window));
      auto const L_top = lua_gettop(L);
      lua_pushglobaltable(L);
      if (lua_getfield(L, -1, "game") not_eq LUA_TTABLE or
          lua_getfield(L, -1, "event") not_eq LUA_TTABLE)
      {
        lua_settop(L, L_top);
        return;
      }
      for (auto const name : {"on_event"sv, on_ev_name})
      {
        if (lua_getfield(L, -1, name.data()) not_eq LUA_TFUNCTION)
        {
          lua_pop(L, 1);
          continue;
        }
        auto const L_function_idx = lua_gettop(L);
        if (name not_eq on_ev_name)
          lua_pushlstring(L, ev_name.data(), ev_name.size());
        if constexpr (std::convertible_to<decltype(std::tuple{args...}), std::tuple<int, char const **>>)
        {
          auto const [pathc, paths] = std::tuple{args...};
          lua_createtable(L, pathc, 0);
          for (auto i = 0; i < pathc; i++)
            lua_pushstring(L, paths[i]), lua_seti(L, -2, i + 1);
        }
        else
        {
          auto const call = [&]<typename T>(T arg)
          {
            if constexpr (std::integral<T>)
              lua_pushinteger(L, arg);
            else if constexpr (std::floating_point<T>)
              lua_pushnumber(L, arg);
            else
              static_assert(false, "unsupported arg type");
          };
          (call(args), ...);
        }
        auto const nargs = lua_gettop(L) - L_function_idx;
        if (lua_pcall(L, nargs, 0, 0) == LUA_OK)
          continue;
        utils::print_errorf("Lua error in game.event.%s(): %s", name.data(), lua_tostring(L, -1)), lua_pop(L, 1);
      }
      lua_settop(L, L_top);
    }
    auto link_glfw_events(GLFWwindow *window, lua_State *L) -> void
    {
      auto const prev_uptr = glfwGetWindowUserPointer(window);
      utils::assert(not prev_uptr or prev_uptr == L, "Window already has a user pointer");
      glfwSetWindowUserPointer(window, L);
      glfwSetWindowPosCallback /*          */ (window, forward_window_event<CTA{"on_window_pos" /*           */}>);
      glfwSetWindowSizeCallback /*         */ (window, forward_window_event<CTA{"on_window_size" /*          */}>);
      glfwSetWindowCloseCallback /*        */ (window, forward_window_event<CTA{"on_window_close" /*         */}>);
      glfwSetWindowRefreshCallback /*      */ (window, forward_window_event<CTA{"on_window_refresh" /*       */}>);
      glfwSetWindowFocusCallback /*        */ (window, forward_window_event<CTA{"on_window_focus" /*         */}>);
      glfwSetWindowIconifyCallback /*      */ (window, forward_window_event<CTA{"on_window_iconify" /*       */}>);
      glfwSetWindowMaximizeCallback /*     */ (window, forward_window_event<CTA{"on_window_maximize" /*      */}>);
      glfwSetFramebufferSizeCallback /*    */ (window, forward_window_event<CTA{"on_framebuffer_size" /*     */}>);
      glfwSetWindowContentScaleCallback /* */ (window, forward_window_event<CTA{"on_window_content_scale" /* */}>);
      glfwSetKeyCallback /*                */ (window, forward_window_event<CTA{"on_key" /*                  */}>);
      glfwSetCharCallback /*               */ (window, forward_window_event<CTA{"on_char" /*                 */}>);
      glfwSetCharModsCallback /*           */ (window, forward_window_event<CTA{"on_char_mods" /*            */}>);
      glfwSetMouseButtonCallback /*        */ (window, forward_window_event<CTA{"on_mouse_button" /*         */}>);
      glfwSetCursorPosCallback /*          */ (window, forward_window_event<CTA{"on_cursor_pos" /*           */}>);
      glfwSetCursorEnterCallback /*        */ (window, forward_window_event<CTA{"on_cursor_enter" /*         */}>);
      glfwSetScrollCallback /*             */ (window, forward_window_event<CTA{"on_scroll" /*               */}>);
      glfwSetDropCallback /*               */ (window, forward_window_event<CTA{"on_drop" /*                 */}>);
    }
  }
  namespace helper
  {
    auto errorf(lua_State *L, int trace_start_idx, char const *fmt, ...) -> char const *
    {
      auto const top = lua_gettop(L);
      auto const mem_segments = (top - trace_start_idx) / 2;
      auto const [name, name_alloc] = [&]
      {
        auto capacity = 0x10u;
        for (auto i = trace_start_idx; i < top - 1; i += 2)
          capacity += std::size(".") + std::string_view{lua_tostring(L, i)}.size();
        auto alloc = std::unique_ptr<char[]>(new char[capacity]);
        auto const end = alloc.get() + capacity;
        auto it = alloc.get();
        for (auto i = trace_start_idx; i < top - 1; i += 2)
          it += std::snprintf(it, end - it, ".%s", lua_tostring(L, i));
        auto const name = alloc.get() + 1;
        return std::pair{std::string_view{name, size_t(it - name)}, std::move(alloc)};
      }();
      char msg_buf[0x40];
      va_list args;
      va_start(args, fmt);
      auto const [msg, msg_alloc] = utils::vsnprintf(msg_buf, fmt, args);
      va_end(args);
      auto const err = lua_pushfstring(L, "(%s: %s = %s) %s", name.data(), luaL_typename(L, top), lua_tostring(L, top), msg.data());
      return err;
    }
    template <typename KT, typename VT>
    auto field(lua_State *L, int trace_start_idx, KT key, VT, [[maybe_unused]] int expected_type = LUA_TNIL)
    {
      if constexpr (std::convertible_to<KT, std::string_view>)
      {
        auto const key_sv = std::string_view{key};
        lua_pushlstring(L, key_sv.data(), key_sv.size());
      }
      else if constexpr (std::integral<KT>)
      {
        lua_pushinteger(L, key);
      }
      else
        static_assert(false, "Invalid key type");
      auto const type = (lua_pushvalue(L, -1), lua_gettable(L, -3));
      if constexpr (std::convertible_to<VT, std::string_view>)
      {
        auto len = (size_t)0;
        auto str = lua_tolstring(L, -1, &len);
        if (not str)
          throw errorf(L, trace_start_idx, "was not of type %s", lua_typename(L, LUA_TSTRING));
        lua_pop(L, 2);
        return std::string_view{str, len};
      }
      else if constexpr (std::integral<VT>)
      {
        auto isnum = 0;
        auto num = lua_tointegerx(L, -1, &isnum);
        if (not isnum)
          throw errorf(L, trace_start_idx, "was not of type %s", "integer");
        lua_pop(L, 2);
        return num;
      }
      else if constexpr (std::floating_point<VT>)
      {
        auto isnum = 0;
        auto num = lua_tonumberx(L, -1, &isnum);
        if (not isnum)
          throw errorf(L, trace_start_idx, "was not of type %s", lua_typename(L, LUA_TNUMBER));
        lua_pop(L, 2);
        return num;
      }
      else
      {
        if (type not_eq expected_type)
          throw errorf(L, trace_start_idx, "was not of type %s", lua_typename(L, expected_type));
      }
    }
  }
  namespace event
  {
    auto push_lib(lua_State *L) -> int
    {
      lua_createtable(L, 0, 0);
      return 1;
    }
  }
  namespace input
  {
    inline static constexpr std::pair<char const *, int16_t> keys[]{{"UNKNOWN", GLFW_KEY_UNKNOWN}, {"SPACE", GLFW_KEY_SPACE}, {"APOSTROPHE", GLFW_KEY_APOSTROPHE}, {"COMMA", GLFW_KEY_COMMA}, {"MINUS", GLFW_KEY_MINUS}, {"PERIOD", GLFW_KEY_PERIOD}, {"SLASH", GLFW_KEY_SLASH}, {"0", GLFW_KEY_0}, {"1", GLFW_KEY_1}, {"2", GLFW_KEY_2}, {"3", GLFW_KEY_3}, {"4", GLFW_KEY_4}, {"5", GLFW_KEY_5}, {"6", GLFW_KEY_6}, {"7", GLFW_KEY_7}, {"8", GLFW_KEY_8}, {"9", GLFW_KEY_9}, {"SEMICOLON", GLFW_KEY_SEMICOLON}, {"EQUAL", GLFW_KEY_EQUAL}, {"A", GLFW_KEY_A}, {"B", GLFW_KEY_B}, {"C", GLFW_KEY_C}, {"D", GLFW_KEY_D}, {"E", GLFW_KEY_E}, {"F", GLFW_KEY_F}, {"G", GLFW_KEY_G}, {"H", GLFW_KEY_H}, {"I", GLFW_KEY_I}, {"J", GLFW_KEY_J}, {"K", GLFW_KEY_K}, {"L", GLFW_KEY_L}, {"M", GLFW_KEY_M}, {"N", GLFW_KEY_N}, {"O", GLFW_KEY_O}, {"P", GLFW_KEY_P}, {"Q", GLFW_KEY_Q}, {"R", GLFW_KEY_R}, {"S", GLFW_KEY_S}, {"T", GLFW_KEY_T}, {"U", GLFW_KEY_U}, {"V", GLFW_KEY_V}, {"W", GLFW_KEY_W}, {"X", GLFW_KEY_X}, {"Y", GLFW_KEY_Y}, {"Z", GLFW_KEY_Z}, {"LEFT_BRACKET", GLFW_KEY_LEFT_BRACKET}, {"BACKSLASH", GLFW_KEY_BACKSLASH}, {"RIGHT_BRACKET", GLFW_KEY_RIGHT_BRACKET}, {"GRAVE_ACCENT", GLFW_KEY_GRAVE_ACCENT}, {"WORLD_1", GLFW_KEY_WORLD_1}, {"WORLD_2", GLFW_KEY_WORLD_2}, {"ESCAPE", GLFW_KEY_ESCAPE}, {"ENTER", GLFW_KEY_ENTER}, {"TAB", GLFW_KEY_TAB}, {"BACKSPACE", GLFW_KEY_BACKSPACE}, {"INSERT", GLFW_KEY_INSERT}, {"DELETE", GLFW_KEY_DELETE}, {"RIGHT", GLFW_KEY_RIGHT}, {"LEFT", GLFW_KEY_LEFT}, {"DOWN", GLFW_KEY_DOWN}, {"UP", GLFW_KEY_UP}, {"PAGE_UP", GLFW_KEY_PAGE_UP}, {"PAGE_DOWN", GLFW_KEY_PAGE_DOWN}, {"HOME", GLFW_KEY_HOME}, {"END", GLFW_KEY_END}, {"CAPS_LOCK", GLFW_KEY_CAPS_LOCK}, {"SCROLL_LOCK", GLFW_KEY_SCROLL_LOCK}, {"NUM_LOCK", GLFW_KEY_NUM_LOCK}, {"PRINT_SCREEN", GLFW_KEY_PRINT_SCREEN}, {"PAUSE", GLFW_KEY_PAUSE}, {"F1", GLFW_KEY_F1}, {"F2", GLFW_KEY_F2}, {"F3", GLFW_KEY_F3}, {"F4", GLFW_KEY_F4}, {"F5", GLFW_KEY_F5}, {"F6", GLFW_KEY_F6}, {"F7", GLFW_KEY_F7}, {"F8", GLFW_KEY_F8}, {"F9", GLFW_KEY_F9}, {"F10", GLFW_KEY_F10}, {"F11", GLFW_KEY_F11}, {"F12", GLFW_KEY_F12}, {"F13", GLFW_KEY_F13}, {"F14", GLFW_KEY_F14}, {"F15", GLFW_KEY_F15}, {"F16", GLFW_KEY_F16}, {"F17", GLFW_KEY_F17}, {"F18", GLFW_KEY_F18}, {"F19", GLFW_KEY_F19}, {"F20", GLFW_KEY_F20}, {"F21", GLFW_KEY_F21}, {"F22", GLFW_KEY_F22}, {"F23", GLFW_KEY_F23}, {"F24", GLFW_KEY_F24}, {"F25", GLFW_KEY_F25}, {"KP_0", GLFW_KEY_KP_0}, {"KP_1", GLFW_KEY_KP_1}, {"KP_2", GLFW_KEY_KP_2}, {"KP_3", GLFW_KEY_KP_3}, {"KP_4", GLFW_KEY_KP_4}, {"KP_5", GLFW_KEY_KP_5}, {"KP_6", GLFW_KEY_KP_6}, {"KP_7", GLFW_KEY_KP_7}, {"KP_8", GLFW_KEY_KP_8}, {"KP_9", GLFW_KEY_KP_9}, {"KP_DECIMAL", GLFW_KEY_KP_DECIMAL}, {"KP_DIVIDE", GLFW_KEY_KP_DIVIDE}, {"KP_MULTIPLY", GLFW_KEY_KP_MULTIPLY}, {"KP_SUBTRACT", GLFW_KEY_KP_SUBTRACT}, {"KP_ADD", GLFW_KEY_KP_ADD}, {"KP_ENTER", GLFW_KEY_KP_ENTER}, {"KP_EQUAL", GLFW_KEY_KP_EQUAL}, {"LEFT_SHIFT", GLFW_KEY_LEFT_SHIFT}, {"LEFT_CONTROL", GLFW_KEY_LEFT_CONTROL}, {"LEFT_ALT", GLFW_KEY_LEFT_ALT}, {"LEFT_SUPER", GLFW_KEY_LEFT_SUPER}, {"RIGHT_SHIFT", GLFW_KEY_RIGHT_SHIFT}, {"RIGHT_CONTROL", GLFW_KEY_RIGHT_CONTROL}, {"RIGHT_ALT", GLFW_KEY_RIGHT_ALT}, {"RIGHT_SUPER", GLFW_KEY_RIGHT_SUPER}, {"MENU", GLFW_KEY_MENU}, {"LAST", GLFW_KEY_LAST}};
    inline static constexpr std::pair<char const *, int8_t> mouse_buttons[]{{"1", GLFW_MOUSE_BUTTON_1}, {"2", GLFW_MOUSE_BUTTON_2}, {"3", GLFW_MOUSE_BUTTON_3}, {"4", GLFW_MOUSE_BUTTON_4}, {"5", GLFW_MOUSE_BUTTON_5}, {"6", GLFW_MOUSE_BUTTON_6}, {"7", GLFW_MOUSE_BUTTON_7}, {"8", GLFW_MOUSE_BUTTON_8}, {"LAST", GLFW_MOUSE_BUTTON_LAST}, {"LEFT", GLFW_MOUSE_BUTTON_LEFT}, {"RIGHT", GLFW_MOUSE_BUTTON_RIGHT}, {"MIDDLE", GLFW_MOUSE_BUTTON_MIDDLE}};
    inline static constexpr std::pair<char const *, int8_t> actions[]{{"RELEASE", GLFW_RELEASE}, {"PRESS", GLFW_PRESS}, {"REPEAT", GLFW_REPEAT}};
    auto push_keys(lua_State *L) -> int // fun(): {[string]:integer}
    {
      lua_createtable(L, 0, (int)std::size(keys));
      for (auto [key, val] : keys)
        lua_pushinteger(L, val), lua_setfield(L, -2, key);
      return 1;
    }
    auto push_mouse_buttons(lua_State *L) -> int // fun(): {[string]:integer}
    {
      lua_createtable(L, 0, (int)std::size(mouse_buttons));
      for (auto [key, val] : mouse_buttons)
        lua_pushinteger(L, val), lua_setfield(L, -2, key);
      return 1;
    }
    auto push_actions(lua_State *L) -> int // fn(): {[string]:integer}
    {
      lua_createtable(L, 0, (int)std::size(actions));
      for (auto [key, val] : actions)
        lua_pushinteger(L, val), lua_setfield(L, -2, key);
      return 1;
    }
    auto push_lib(lua_State *L) -> int // fun(): game.input
    {
      luaL_Reg static constexpr funcs[]{
          {"key", 0},
          {"mb", 0},
          {"mouse_button", 0},
          {"action", 0},
          {0, 0}};
      luaL_newlib(L, funcs);
      push_keys(L), lua_setfield(L, -2, "key");
      push_mouse_buttons(L), lua_setfield(L, -2, "mb");
      lua_getfield(L, -1, "mb"), lua_setfield(L, -2, "mouse_button");
      push_actions(L), lua_setfield(L, -2, "action");
      return 1;
    }
  }
  namespace game
  {
    auto set_camera_ortho(lua_State *L) -> int // fun(left: number, right: number, bottom: number, top: number)
    {
      auto const argc = lua_gettop(L);
      if (auto constexpr expected_argc = 4; expected_argc != argc)
        luaL_error(L, "Expected %d args, got %d args", expected_argc, argc);
      auto const left /*   */ = luaL_checknumber(L, 1);
      auto const right /*  */ = luaL_checknumber(L, 2);
      auto const bottom /* */ = luaL_checknumber(L, 3);
      auto const top /*    */ = luaL_checknumber(L, 4);
      if (not global_application or not global_application->get_renderer())
        return luaL_error(L, "Global Application Missing");
      auto const projection = glm::ortho<float>(left, right, bottom, top);
      global_application->get_renderer()->set_projection(projection);
    }
    auto load_map(lua_State *L) -> int // fun(map: tiled.map)
    {
      auto const argc = lua_gettop(L);
      if (auto constexpr expected_argc = 1; expected_argc != argc)
        luaL_error(L, "Expected %d args, got %d args", expected_argc, argc);
      luaL_checktype(L, 1, LUA_TTABLE);
      if (not global_application or not global_application->get_renderer())
        return luaL_error(L, "Global Application Missing");
      try
      {
        using render::tile::tile, render::tile::chunk, render::tile::tileset;
        auto const field = [&](auto key, auto value, int expected_type = 0)
        { return helper::field(L, argc + 1, key, value, expected_type); };
        auto &renderer = *global_application->get_renderer();
        renderer.mesh_reload();
        lua_pushstring(L, "map"), lua_pushvalue(L, 1);
        // auto const map_version /*       */ = field("version" /*       */, ""); // "1.10"
        // auto const map_luaversion /*    */ = field("luaversion" /*    */, ""); // "5.1"
        // auto const map_tiledversion /*  */ = field("tiledversion" /*  */, ""); // "1.11.0"
        // auto const map_class /*         */ = field("class" /*         */, ""); // string
        // auto const map_orientation /*   */ = field("orientation" /*   */, ""); // "orthogonal"
        // auto const map_renderorder /*   */ = field("renderorder" /*   */, ""); // "right-down"
        // auto const map_width /*         */ = field("width" /*         */, 0); // integer
        // auto const map_height /*        */ = field("height" /*        */, 0); // integer
        auto const map_tilewidth /*     */ = field("tilewidth" /*     */, 0); // integer
        auto const map_tileheight /*    */ = field("tileheight" /*    */, 0); // integer
        // auto const map_nextlayerid /*   */ = field("nextlayerid" /*   */, 0); // integer
        // auto const map_nextobjectid /*  */ = field("nextobjectid" /*  */, 0); // integer
        // auto const map_properties /*    */ = field("properties" /*    */, ""); // table
        { // layers
          std::vector<tile> tiles;
          std::vector<chunk> chunks;
          field("layers", nullptr, LUA_TTABLE);
          { // Reserve
            auto chunk_len = (size_t)0;
            auto tiles_reserve = (size_t)0;
            auto chunks_reserve = (size_t)0;
            for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) // for layer in map.layers
            {
              auto const layer_type = field("type", ""); // "tilelayer"
              if (layer_type not_eq "tilelayer")
                continue;
              field("chunks", nullptr, LUA_TTABLE);
              chunks_reserve += (size_t)luaL_len(L, -1);
              for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) // for chunk in layer.chunks
              {
                auto const chunk_width /*  */ = field("width" /*  */, 0); // integer
                auto const chunk_height /* */ = field("height" /* */, 0); // integer
                field("data", nullptr, LUA_TTABLE);
                auto const chunk_grid_len = (size_t)chunk_width * chunk_height;
                auto const chunk_data_len = (size_t)luaL_len(L, -1);
                { // Check lengths
                  if (chunk_len == 0)
                    chunk_len = chunk_data_len;
                  if (chunk_len not_eq chunk_data_len)
                    throw helper::errorf(L, argc + 1, "%s does not match. Expected:%zu Got:%zu", "len(data)", chunk_len, chunk_data_len);
                  if (chunk_len not_eq chunk_grid_len)
                    throw helper::errorf(L, argc + 1, "%s does not match Expected:%zu Got:%zu", "width*height", chunk_len, chunk_grid_len);
                }
                tiles_reserve += chunk_data_len;
                lua_pop(L, 2);
              }
              lua_pop(L, 2);
            }
            tiles.reserve(tiles_reserve);
            chunks.reserve(chunks_reserve);
          }
          for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) // for layer in map.layers
          {
            auto const layer_type /*       */ = field("type" /*       */, ""); // "tilelayer"
            if (layer_type not_eq "tilelayer")
              continue;
            // auto const layer_x /*          */ = field("x" /*          */, 0);  // integer
            // auto const layer_y /*          */ = field("y" /*          */, 0);  // integer
            // auto const layer_width /*      */ = field("width" /*      */, 0);  // integer
            // auto const layer_height /*     */ = field("height" /*     */, 0);  // integer
            // auto const layer_id /*         */ = field("id" /*         */, 0);  // integer
            // auto const layer_name /*       */ = field("name" /*       */, ""); // string
            // auto const layer_class /*      */ = field("class" /*      */, ""); // string
            // auto const layer_opacity /*    */ = field("opacity" /*    */, 0);  // 1
            // auto const layer_offsetx /*    */ = field("offsetx" /*    */, 0);  // 0
            // auto const layer_offsety /*    */ = field("offsety" /*    */, 0);  // 0
            // auto const layer_parallaxx /*  */ = field("parallaxx" /*  */, 0);  // 1
            // auto const layer_parallaxy /*  */ = field("parallaxy" /*  */, 0);  // 1
            // auto const layer_encoding /*   */ = field("encoding" /*   */, ""); // "lua"
            field("chunks", nullptr, LUA_TTABLE);
            for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) // for chunk in layer.chunks
            {
              auto const chunk_x /*      */ = field("x" /*      */, 0); // integer
              auto const chunk_y /*      */ = field("y" /*      */, 0); // integer
              auto const chunk_width /*  */ = field("width" /*  */, 0); // integer
              auto const chunk_height /* */ = field("height" /* */, 0); // integer
              field("data", nullptr, LUA_TTABLE);
              auto const chunk_data_len = luaL_len(L, -1);
              for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) // for tile in chunk.data
              {
                auto isnum = 0;
                tiles.push_back((tile)lua_tointegerx(L, -1, &isnum));
                if (not isnum)
                  throw helper::errorf(L, argc + 1, "%s", "was not an integer");
              }
              chunks.push_back(chunk{
                  .offset{(glm::i32)chunk_x, (glm::i32)chunk_y},
                  .size{(glm::u32)chunk_width, (glm::u32)chunk_height},
              });
              lua_pop(L, 2);
            }
            lua_pop(L, 2);
          }
          lua_pop(L, 2);
          renderer.upload(tiles);
          renderer.upload(chunks);
        }
        { // tilesets
          std::vector<tileset> tilesets;
          std::vector<std::string> textures;
          field("tilesets", nullptr, LUA_TTABLE);
          auto const tilesets_len = (size_t)luaL_len(L, -1);
          tilesets.reserve(tilesets_len);
          textures.reserve(tilesets_len);
          for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) // for tileset in map.tilesets
          {
            // auto const tileset_name /*            */ = field("name" /*            */, ""); // string
            // auto const tileset_firstgid /*        */ = field("firstgid" /*        */, 0u); // integer
            // auto const tileset_filename /*        */ = field("filename" /*        */, ""); // string
            // auto const tileset_exportfilename /*  */ = field("exportfilename" /*  */, ""); // string?
            // auto const tileset_version /*         */ = field("version" /*         */, ""); // "1.10"
            // auto const tileset_luaversion /*      */ = field("luaversion" /*      */, ""); // "5.1"
            // auto const tileset_tiledversion /*    */ = field("tiledversion" /*    */, ""); // "1.11.0"
            // auto const tileset_class /*           */ = field("class" /*           */, ""); // string
            auto const tileset_tilewidth /*       */ = field("tilewidth" /*       */, 0u); // integer
            auto const tileset_tileheight /*      */ = field("tileheight" /*      */, 0u); // integer
            // auto const tileset_spacing /*         */ = field("spacing" /*         */, 0);  // 0
            // auto const tileset_margin /*          */ = field("margin" /*          */, 0);  // 0
            auto const tileset_columns /*         */ = field("columns" /*         */, 0);  // integer
            auto const tileset_image /*           */ = field("image" /*           */, ""); // string
            // auto const tileset_imagewidth /*      */ = field("imagewidth" /*      */, 0);  // integer
            // auto const tileset_imageheight /*     */ = field("imageheight" /*     */, 0);  // integer
            // auto const tileset_objectalignment /* */ = field("objectalignment" /* */, ""); // "unspecified"
            // auto const tileset_tilerendersize /*  */ = field("tilerendersize" /*  */, ""); // "tile"
            // auto const tileset_fillmode /*        */ = field("fillmode" /*        */, ""); // "stretch"
            // field("tileoffset" /*      */, nullptr, LUA_TTABLE);                           // {x:integer, y:integer}
            // auto const tileset_tileoffset_x /*    */ = field("x" /*               */, 0u); // integer
            // auto const tileset_tileoffset_y /*    */ = field("y" /*               */, 0u); // integer
            // lua_pop(L, 2);
            // field("grid" /*            */, "");                                            // {orientation:"orthogonal", width:integer, height:integer}
            // auto const tileset_grid_orientation /**/ = field("orientation" /*     */, ""); // "orthogonal"
            // auto const tileset_grid_width /*      */ = field("width" /*           */, 0u); // integer
            // auto const tileset_grid_height /*     */ = field("height" /*          */, 0u); // integer
            // lua_pop(L, 2);
            auto const tileset_tilecount /*       */ = field("tilecount" /*       */, 0u); // integer
            auto const size = glm::vec2{tileset_tilewidth, tileset_tileheight} / glm::vec2{map_tilewidth, map_tileheight};
            auto const offset = glm::vec2{0.0f, 1.0f - size.y};
            auto const tex = [&]
            {
              auto tex = 0u;
              for (; tex < textures.size(); tex++)
                if (textures.at(tex) == tileset_image)
                  return tex;
              if (tex == textures.size())
                textures.emplace_back(tileset_image);
              return tex;
            }();
            tilesets.push_back(tileset{
                .tex = tex,
                .columns = (uint32_t)tileset_columns,
                .rows = (uint32_t)(tileset_tilecount / tileset_columns),
                .offset = offset,
                .size = size,
            });
          }
          lua_pop(L, 2);
          renderer.upload(tilesets);
          renderer.textures_reload(std::move(textures));
        }
        return lua_settop(L, 1), lua_pushvalue(L, 1), 1;
      }
      catch (std::exception const &e)
      {
        lua_pushstring(L, e.what());
      }
      catch (...)
      {
      }
      return lua_error(L);
    }
    auto push_lib(lua_State *L) -> int // fun() : game
    {
      luaL_Reg static constexpr funcs[]{
          {"set_camera_ortho", set_camera_ortho},
          {"load_map", load_map},
          {"event", 0},
          {"input", 0},
          {0, 0}};
      luaL_newlib(L, funcs);
      event::push_lib(L), lua_setfield(L, -2, "event");
      input::push_lib(L), lua_setfield(L, -2, "input");
      return 1;
    }
  }
  auto openlibs(lua_State *L) -> int // fun()
  {
    game::push_lib(L), lua_setglobal(L, "game");
    return 0;
  }
  auto push_global(lua_State *L, std::span<char const *const> members) -> int
  {
    lua_pushglobaltable(L);
    auto i = 0, type = lua_type(L, -1);
    for (; i < members.size(); i++)
    {
      type = lua_getfield(L, -1, members[i]);
      lua_remove(L, -2);
      if (not(type == LUA_TTABLE or i == members.size() - 1))
        return lua_pop(L, 1), lua_pushnil(L), LUA_TNIL;
    }
    return type;
  }
  auto call_global(lua_State *L, std::span<char const *const> members, int nargs, int nresults) -> bool
  {
    if (push_global(L, members) not_eq LUA_TFUNCTION)
      return lua_pop(L, nargs + 1), false;
    lua_insert(L, -nargs - 1);
    if (lua_pcall(L, nargs, nresults, 0) == LUA_OK)
      return true;
    { // Error
      auto buf_min_size = (size_t)0;
      for (auto mem : members)
        buf_min_size += "."sv.size() + std::string_view{mem}.size();
      buf_min_size += std::size("()");
      auto const alloc = std::unique_ptr<char[]>(new char[buf_min_size]);
      auto const beg = alloc.get(), end = beg + buf_min_size;
      auto it = beg;
      for (std::string_view mem : members)
        for (*it++ = '.'; auto c : mem)
          *it++ = c;
      for (auto c : "()")
        *it++ = c;
      auto const fn_name = beg + 1;
      utils::print_errorf("Lua error in %s: %s", fn_name, lua_tostring(L, -1)), lua_pop(L, 1);
    }
    return false;
  }
  auto link_window_events(GLFWwindow *window, lua_State *L) -> void
  {
    return glfw::link_glfw_events(window, L);
  }
}
