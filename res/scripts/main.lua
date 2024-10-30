game.map = require("tiled.map")

game.state = game.state or {
  view_width = 16,
  aspect     = 1,
  ---@type vec2
  camera_pos = { 0, 0 },
}

utils = {
  ---@param value any
  ---@param indent? string
  ---@return string
  tostring = function(value, indent)
    if type(value) ~= "table" then
      return tostring(value)
    end
    local res = "\n"
    indent = indent or ""
    local indent_p = indent .. "\t"
    for k, v in pairs(value) do
      res = string.format("%s%s%-16s = %s,\n", res, indent_p, tostring(k), utils.tostring(v, indent_p))
    end
    return string.format("{%s%s}", res, indent)
  end,
  ---@param value any
  print = function(value)
    print(utils.tostring(value))
  end,
}

on_key_press = {
  [game.input.key.DELETE] = function() print("\027[2J\027[H") end,
  [game.input.key.INSERT] = function() game.prep_tilemap(game.map.load("main")) end,
}

key_down = key_down or {}
function game.event.on_key(key, _, action)
  if not key_down_reverse then
    local reverse = {}
    for key, value in pairs(game.input.key) do
      reverse[value] = key;
    end
    key_down_reverse = reverse
  end
  local char = key_down_reverse[key]
  key_down[key] = action ~= game.input.action.RELEASE or nil
  key_down[char] = key_down[key]
  print("Key", char, key)
  if action == game.input.action.PRESS then
    local f = on_key_press[key]
    if f then f() end
  end
end

mb_down = mb_down or {}
function game.event.on_mouse_button(button, action, _)
  mb_down[button] = action ~= game.input.action.RELEASE or nil
end

function game.event.on_scroll(dx, dy)
  if mb_down[game.input.mb['5']] then
    game.state.view_width = game.state.view_width - math.ceil(dy)
    game.state.update_view()
    return
  end
  local cx, cy = table.unpack(game.state.camera_pos)
  cx = cx - dx
  cy = cy - dy
  game.state.camera_pos = { cx, cy };
  game.state.update_view()
end

function game.event.on_window_size(w, h)
  game.viewport(0, 0, w, h)
  game.state.aspect = w / h
  game.state.update_view()
end

function game.state.update_view()
  local a, s = game.state.aspect, game.state.view_width
  local w, h = s * a, s / a
  local x, y = table.unpack(game.state.camera_pos)
  game.camera(x - w, x + w, y - h, y + h);
end

function game.setup()
  print("setup")
  game.event.on_window_size(720, 720)
  game.prep_tilemap(game.map.load("main"))
  game.tick_rate(1.0 / 30.0);
end

function game.update(dt)
  local s = 10 / 30;
  local cx, cy = table.unpack(game.state.camera_pos)
  cy = cy + (key_down.W and -s or 0) + (key_down.S and s or 0)
  cx = cx + (key_down.A and -s or 0) + (key_down.D and s or 0)
  game.state.camera_pos = { cx, cy };
  game.state.update_view()
end

function game.draw()
  game.draw_tiles()
end

function game.shutdown()
  print("shutdown")
end
