---@meta

---@class game.event
game.event = {}

---@type nil | fun(name: string, ...)
game.event.on_event = nil

---@type fun()
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
