---@class tiled.map
---@field version      "1.10"
---@field luaversion   "5.1"
---@field tiledversion "1.11.0"
---@field class        string
---@field orientation  "orthogonal"
---@field renderorder  "right-down"
---@field width        integer
---@field height       integer
---@field tilewidth    integer
---@field tileheight   integer
---@field nextlayerid  integer
---@field nextobjectid integer
---@field properties   table
---@field tilesets     tiled.map.tileset[]
---@field layers       tiled.map.layer[]

---@alias tiled.map.tileset tiled.map.tileset.exported | tiled.map.tileset.embedded
---@class tiled.map.tileset.base
---@field name           string
---@field firstgid       integer
---@field filename       string
---@class tiled.map.tileset.exported : tiled.map.tileset.base
---@field exportfilename string
---@class tiled.map.tileset.embedded : tiled.map.tileset.base
---@field version         "1.10"
---@field luaversion      "5.1"
---@field tiledversion    "1.11.0"
---@field class           string
---@field tilewidth       integer
---@field tileheight      integer
---@field spacing         0
---@field margin          0
---@field columns         integer
---@field image           string
---@field imagewidth      integer
---@field imageheight     integer
---@field objectalignment "unspecified"
---@field tilerendersize  "tile"
---@field fillmode        "stretch"
---@field tileoffset      {x:integer, y:integer}
---@field grid            {orientation:"orthogonal", width:integer, height:integer}
---@field properties      table
---@field wangsets        table
---@field tilecount       integer
---@field tiles           table

---@alias tiled.map.layer tiled.map.layer.tile | tiled.map.layer.object

---@class tiled.map.layer.tile
---@field type       "tilelayer"
---@field x          integer
---@field y          integer
---@field width      integer
---@field height     integer
---@field id         integer
---@field name       string
---@field class      string
---@field visible    true
---@field opacity    1
---@field offsetx    0
---@field offsety    0
---@field parallaxx  1
---@field parallaxy  1
---@field properties {}
---@field encoding   "lua"
---@field chunks     tiled.map.tile.chunk[]

---@class tiled.map.tile.chunk
---@field x      integer
---@field y      integer
---@field width  integer
---@field height integer
---@field data   integer[]

---@class tiled.map.layer.object
---@field type       "objectgroup",
---@field draworder  "topdown",
---@field id         integer
---@field name       string
---@field class      string
---@field visible    true
---@field opacity    1
---@field offsetx    0
---@field offsety    0
---@field parallaxx  1
---@field parallaxy  1
---@field properties table
---@field objects    tiled.map.object[]

---@class tiled.map.object
---@field id         integer
---@field name       string
---@field type       string
---@field shape      "point"
---@field x          number
---@field y          number
---@field width      0
---@field height     0
---@field rotation   0
---@field visible    true
---@field properties table


game.map = {
  folder          = "res/maps/",
  tilesets_folder = "res/maps/tilesets/",
  ---@type tiled.map?
  loaded          = nil,
}

---@param filepath string
---@return any
function game.map.load_config_file_safe(filepath)
  return assert(loadfile(filepath, nil, {}))()
end

---@param name string
---@return tiled.map
function game.map.load(name)
  ---@type tiled.map
  local map = game.map.load_config_file_safe(game.map.folder .. name .. ".lua")
  for tileset_i, tileset in ipairs(map.tilesets) do
    if tileset.exportfilename then
      local filepath = game.map.folder .. tileset.exportfilename
      local folder = filepath:match("^(.*[/\\])")
      ---@type tiled.map.tileset.embedded
      local extended = game.map.load_config_file_safe(filepath)
      for key, value in pairs(tileset) do
        extended[key] = value;
      end
      extended.image = folder .. extended.image
      map.tilesets[tileset_i] = extended;
    end
  end
  game.map.loaded = map;
  return map;
end

return game.map
