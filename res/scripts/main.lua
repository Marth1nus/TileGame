print("reload main.lua")
tiled = require("tiled")

function table_reverse(t)
  local r = {}
  for k, v in pairs(t) do r[v] = k end
  return r
end

---@type {[game.input.key]: string}
key_name = key_name or table_reverse(game.input.key)
---@type {[game.input.key | string]: fun()}
on_key = on_key or {}
---@type {[game.input.key | string]: true|nil}
key_down = key_down or {}

---@type {[game.input.mb]: string}
mb_name = mb_name or table_reverse(game.input.mb)
---@type {[game.input.mb | string]: fun()}
on_mb = on_mb or {}
---@type {[game.input.mb | string]: true|nil}
mb_down = mb_down or {}

state = state or {
  view_width = 4,
  ---@type vec2
  view_offset = { 0, 0 },
}

function update_view()
  local x, y = table.unpack(state.view_offset)
  local w, h = state.view_width, state.view_width
  game.set_camera_ortho(x - w, x + w, y + h, y - h)
end

function reload()
  dofile("res/scripts/main.lua")
  game.load_map(tiled.map.load("main"))
end

function game.setup()
  print("setup")
  reload()
end

function game.update(dt)
  update_view()
end

function game.shutdown()
  print("shutdown")
end

function game.event.on_scroll(xoffset, yoffset)
  if mb_down[4] then
    state.view_width = state.view_width - yoffset
  else
    local x, y = table.unpack(state.view_offset)
    x, y = x - xoffset, y - yoffset
    state.view_offset = { x, y }
  end
end

function game.event.on_key(key, scancode, action, mods)
  local chr = key_name[key]
  local press = action ~= game.input.action.RELEASE and true or nil
  if press then
    if on_key[key] then on_key[key]() end
    if on_key[chr] then on_key[chr]() end
  end
  key_down[key], key_down[chr] = press, press
end

function game.event.on_mouse_button(button, action, mods)
  local but = button
  local press = action ~= game.input.action.RELEASE and true or nil
  if press then
    if on_mb[but] then on_mb[but]() end
  end
  mb_down[but] = press
end

function on_key.F5()
  reload()
end
