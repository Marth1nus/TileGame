---@meta

---@class game
game = {}

---@alias vec2 [number, number]

---@type nil | fun()
game.setup = nil

---@type nil | fun(dt: number)
game.update = nil

---@type nil | fun()
game.draw = nil

---@type nil | fun()
game.shutdown = nil

---@type fun(x: integer, y: integer, width: integer, height: integer)
function game.viewport() end

---@type fun(left: number, right: number, bottom: number, top: number)
function game.camera() end

---@type fun(dt?: number): number
function game.tick_rate() end

---@class game.tileset
---@field count    integer
---@field columns  integer
---@field image    string

---@type fun(tilesets: game.tileset[])
function game.set_tilesets() end

---@type fun(tiles: integer[])
function game.set_tiles() end

---@type fun(columns: integer, x: integer, y: integer)
function game.draw_tiles() end
