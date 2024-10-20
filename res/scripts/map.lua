local map = {}

---@class game.map
---@field tilesets  game.map.tileset[]
---@field layers    game.map.layer[]

---@class game.map.tileset
---@field name           string
---@field columns        integer
---@field image          string
---@field firstgid       integer
---@field tilecount      integer
---@field count          integer?
---@field exportfilename string?

---@alias game.map.layer game.map.layer.tile | game.map.layer.object
---@class game.map.layer.tile
---@field type      "tilelayer"
---@field x         integer
---@field y         integer
---@field width     integer
---@field height    integer
---@field chunks    game.map.layer.tile.chunk[]

---@class game.map.layer.tile.chunk
---@field x         integer
---@field y         integer
---@field width     integer
---@field height    integer
---@field data      integer[]

---@class game.map.layer.object
---@field type      "objectgroup"
---@field objects   game.map.layer.object[]

---@class game.map.layer.object.object
---@field id     integer
---@field name   string
---@field type   string
---@field shape  "point"
---@field x      number
---@field y      number
---@field width  number
---@field height number


map.folder = "res/maps/"
map.tilesets_folder = "res/maps/tilesets/"

---@type game.map?
map.loaded = nil

---@param name string
---@param skip_ext boolean?
---@return any
function map.load_config_file_safe(name, skip_ext)
  return assert(loadfile(map.folder .. name .. (skip_ext and "" or ".lua"), nil, {}))()
end

---@param name string
function map.load(name)
  map.loaded = map.load_config_file_safe(name)
  local tilesets = map.loaded.tilesets
  for _, tileset in ipairs(tilesets) do
    local tilesets_folder = map.folder
    if tileset.exportfilename then
      tilesets_folder = map.tilesets_folder
      local extended = map.load_config_file_safe(tileset.exportfilename, true)
      for key, value in pairs(extended) do
        tileset[key] = value
      end
    end
    tileset.count = tileset.tilecount
    tileset.columns = tileset.columns
    tileset.image = tilesets_folder .. tileset.image
  end
  game.set_tilesets(tilesets)
end

---@param tilesets game.map.tileset[]
function map.tilesets(tilesets)
end

function map.draw()
  for layer_i, layer in ipairs(map.loaded.layers) do
    if layer.type == "tilelayer" then
      for chunk_i, chunk in ipairs(layer.chunks) do
        game.set_tiles(chunk.data);
        game.draw_tiles(chunk.width, chunk.x, chunk.y);
      end
    end
  end
end

return map
