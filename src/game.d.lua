---@meta

--======================================================================================================================
--======================================================================================================================

---@class game
game = {}

---@alias vec2 [number, number]

--================================

---@class tile_set
---@field first    integer
---@field last     integer
---@field columns  integer
---@field rows     integer
---@field tex      integer

function game.tile_sets_clear() end

---@param tile_set tile_set
function game.tile_sets_add(tile_set) end

---@param tile_sets tile_set[]
function game.tile_sets_set(tile_sets) end

--================================

---@param tiles integer[]
function game.tiles_set(tiles) end

--================================

---@class instance
---@field pos      vec2
---@field size     vec2
---@field uv_pos   vec2
---@field uv_size  vec2
---@field tex      integer

function game.instances_clear() end

---@param instance instance
function game.instances_add(instance) end

---@param instances instance[]
function game.instances_set(instances) end

--================================

---@param dt number?
---@returns number
function game.tick_rate(dt) end

function game.reopen_lib() end

---@param filepaths string[]
function game.textures_set(filepaths) end

---@param left   number
---@param right  number
---@param bottom number
---@param top    number
function game.camera(left, right, bottom, top) end

---@param x      integer
---@param y      integer
---@param width  integer
---@param height integer
function game.viewport(x, y, width, height) end

---@returns number
function game.time() end

--======================================================================================================================
--======================================================================================================================

---@class game.event
game.event = {}

---@type nil | fun(name: string, ...)
game.event.on_event = nil

function game.event.window_init() end

---@type nil | fun(xpos: integer, ypos: integer)
game.event.on_window_pos = nil

---@type nil | fun(width: integer, height: integer)
game.event.on_window_size = nil

---@type nil | fun()
game.event.on_window_close = nil

---@type nil | fun()
game.event.on_window_refresh = nil

---@type nil | fun(focused: boolean)
game.event.on_window_focus = nil

---@type nil | fun(iconified: boolean)
game.event.on_window_iconify = nil

---@type nil | fun(maximized: boolean)
game.event.on_window_maximize = nil

---@type nil | fun(width: integer, height: integer)
game.event.on_framebuffer_size = nil

---@type nil | fun(xscale: number, yscale: number)
game.event.on_window_content_scale = nil

---@type nil | fun(key: game.input.key, scancode: integer, action: game.input.action, mods: integer)
game.event.on_key = nil

---@type nil | fun(codepoint: integer)
game.event.on_char = nil

---@type nil | fun(codepoint: integer, mods: integer)
game.event.on_char_mods = nil

---@type nil | fun(button: game.input.mouse_button, action: game.input.action, mods: integer)
game.event.on_mouse_button = nil

---@type nil | fun(xpos: number, ypos: number)
game.event.on_cursor_pos = nil

---@type nil | fun(entered: boolean)
game.event.on_cursor_enter = nil

---@type nil | fun(xoffset: number, yoffset: number)
game.event.on_scroll = nil

---@type nil | fun(paths: string[])
game.event.on_drop = nil

--======================================================================================================================
--======================================================================================================================

---@class game.input
game.input = {}

---@enum game.input.action
game.input.action = {
  RELEASE = 0,
  PRESS = 1,
  REPEAT = 2,
}

---@enum game.input.key
game.input.key = {
  UNKNOWN       = -1,
  SPACE         = 32,
  APOSTROPHE    = 39,
  COMMA         = 44,
  MINUS         = 45,
  PERIOD        = 46,
  SLASH         = 47,
  ['0']         = 48,
  ['1']         = 49,
  ['2']         = 50,
  ['3']         = 51,
  ['4']         = 52,
  ['5']         = 53,
  ['6']         = 54,
  ['7']         = 55,
  ['8']         = 56,
  ['9']         = 57,
  SEMICOLON     = 59,
  EQUAL         = 61,
  A             = 65,
  B             = 66,
  C             = 67,
  D             = 68,
  E             = 69,
  F             = 70,
  G             = 71,
  H             = 72,
  I             = 73,
  J             = 74,
  K             = 75,
  L             = 76,
  M             = 77,
  N             = 78,
  O             = 79,
  P             = 80,
  Q             = 81,
  R             = 82,
  S             = 83,
  T             = 84,
  U             = 85,
  V             = 86,
  W             = 87,
  X             = 88,
  Y             = 89,
  Z             = 90,
  LEFT_BRACKET  = 91,
  BACKSLASH     = 92,
  RIGHT_BRACKET = 93,
  GRAVE_ACCENT  = 96,
  WORLD_1       = 161,
  WORLD_2       = 162,
  ESCAPE        = 256,
  ENTER         = 257,
  TAB           = 258,
  BACKSPACE     = 259,
  INSERT        = 260,
  DELETE        = 261,
  RIGHT         = 262,
  LEFT          = 263,
  DOWN          = 264,
  UP            = 265,
  PAGE_UP       = 266,
  PAGE_DOWN     = 267,
  HOME          = 268,
  END           = 269,
  CAPS_LOCK     = 280,
  SCROLL_LOCK   = 281,
  NUM_LOCK      = 282,
  PRINT_SCREEN  = 283,
  PAUSE         = 284,
  F1            = 290,
  F2            = 291,
  F3            = 292,
  F4            = 293,
  F5            = 294,
  F6            = 295,
  F7            = 296,
  F8            = 297,
  F9            = 298,
  F10           = 299,
  F11           = 300,
  F12           = 301,
  F13           = 302,
  F14           = 303,
  F15           = 304,
  F16           = 305,
  F17           = 306,
  F18           = 307,
  F19           = 308,
  F20           = 309,
  F21           = 310,
  F22           = 311,
  F23           = 312,
  F24           = 313,
  F25           = 314,
  KP_0          = 320,
  KP_1          = 321,
  KP_2          = 322,
  KP_3          = 323,
  KP_4          = 324,
  KP_5          = 325,
  KP_6          = 326,
  KP_7          = 327,
  KP_8          = 328,
  KP_9          = 329,
  KP_DECIMAL    = 330,
  KP_DIVIDE     = 331,
  KP_MULTIPLY   = 332,
  KP_SUBTRACT   = 333,
  KP_ADD        = 334,
  KP_ENTER      = 335,
  KP_EQUAL      = 336,
  LEFT_SHIFT    = 340,
  LEFT_CONTROL  = 341,
  LEFT_ALT      = 342,
  LEFT_SUPER    = 343,
  RIGHT_SHIFT   = 344,
  RIGHT_CONTROL = 345,
  RIGHT_ALT     = 346,
  RIGHT_SUPER   = 347,
  MENU          = 348,
  LAST          = 348,
}

---@enum game.input.mouse_button
game.input.mouse_button = {
  ['1']  = 0,
  ['2']  = 1,
  ['3']  = 2,
  ['4']  = 3,
  ['5']  = 4,
  ['6']  = 5,
  ['7']  = 6,
  ['8']  = 7,
  LAST   = 7,
  LEFT   = 0,
  RIGHT  = 1,
  MIDDLE = 2,
}
--======================================================================================================================
--======================================================================================================================
