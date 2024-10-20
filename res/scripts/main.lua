package.loaded.map = nil
game.map = require("map")
game.state = game.state or {
  view_width = 16,
  aspect     = 1,
  camera_pos = {0,0}
}

function game.event.on_window_size(w, h)
  game.viewport(0, 0, w, h)
  game.state.aspect = w / h
  game.state.update_view()
end

function game.state.update_view()
  local a, s = game.state.aspect, game.state.view_width
  local w, h = s * a, s / a
  local x, y = table.unpack(game.state.camera_pos or { 0, 0 })
  game.camera(x - w, x + w, y - h, y + h);
end

function game.setup()
  print("setup")
  game.event.on_window_size(720, 720)
  game.loaded_map = game.map.load("main")
end

function game.update(dt)
  game.loaded_map = game.map.load("main")
end

function game.draw()
  game.map.draw()
end

function game.shutdown()
  print("shutdown")
end
