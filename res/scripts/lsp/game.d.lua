---@meta
---@alias  vec2 [number , number ]
---@alias ivec2 [integer, integer]
---@class game
---@field setup    nil | fun()
---@field update   nil | fun(dt: number)
---@field shutdown nil | fun()
---@field set_camera_ortho fun(left: number, right: number, bottom: number, top: number)
---@field load_map         fun(map: tiled.map)
game = {}
