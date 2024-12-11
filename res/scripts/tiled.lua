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

---@class tiled.map.tileset
---@field name            string
---@field firstgid        integer
---@field filename        string
---@field exportfilename  string?
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

local tiled = {}

tiled.map = {
  maps_path_fmt = "res/maps/%s.tmx.lua",
  ---@type tiled.map?
  loaded        = nil,
}

---@param filepath string
---@return table
function tiled.map.load_config_file_safe(filepath)
  local config = assert(loadfile(filepath, nil, {}))()
  assert(type(config) == "table", "config file must return a table of values")
  return config
end

---@param name string
---@return tiled.map
function tiled.map.load(name)
  local map_filepath = tiled.map.maps_path_fmt:format(name)
  local map_folder   = map_filepath:match("^(.*[/\\])") --[[@as string]]
  local map          = tiled.map.load_config_file_safe(map_filepath) --[[@as tiled.map]]
  for tileset_i, tileset in ipairs(map.tilesets) do
    if tileset.exportfilename then
      local tileset_filepath  = map_folder .. tileset.exportfilename;
      local tileset_folder    = tileset_filepath:match("^(.*[/\\])") --[[@as string]]
      local export            = tileset
      tileset                 = tiled.map.load_config_file_safe(tileset_filepath) --[[@as tiled.map.tileset]]
      map.tilesets[tileset_i] = tileset
      for key, value in pairs(export) do
        tileset[key] = value
      end
      tileset.image = tileset_folder .. tileset.image
    end
  end
  tiled.map.loaded = map;
  return map;
end

return tiled
