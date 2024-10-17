---@diagnostic disable: lowercase-global

game.viewport(0, 0, 720, 720)
game.tick_rate(1.0 / 1)
i = i or 1
print("hello", i)
i = i + 1

function game.event.on_key(key, _, press)
  print(string.char(key), press, game.input.key.W == key)
end

function game.event.on_event(name, ...)
end
