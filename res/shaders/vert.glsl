#define TEXTURE_SLOTS 16u
#define WEBGL          0
// Everything before here is ignored (defines added back at pre-compile-time)
#version 300 es

in vec2  mesh_pos;
in uint  tile_id;
in ivec4 chunk_quad;

struct tileset
{
  uint tex, columns, rows, padding;
  vec4 quad;
};
uniform TILESETS { tileset tilesets[4]; };
uniform uint tileset_count;
uniform mat4 projection;

out vec2 mpos;
out vec2 pos;
out vec2 uv;
flat out uint tex;

void main()
{
  uint tile = tile_id;
  // Discard tile=0
  if (tile == 0u)
  {
    gl_Position = vec4(0);
    return;
  }
  tile -= 1u;
  // Find Tileset
  tileset ts;
  for (uint i = 0u; i < tileset_count; i++)
  {
    ts = tilesets[i];
    uint tile_count = ts.columns * ts.rows;
    if (tile < tile_count)
      break;
    tile -= tile_count;
  }
  // Chunk data
  ivec2 chunk_offset = chunk_quad.xy;
  uvec2 chunk_size   = uvec2(chunk_quad.zw);
  uint  iid          = uint(gl_InstanceID) % uint(chunk_size.x * chunk_size.y);
  // Quad data
  vec2 screen_pos  = ts.quad.xy + vec2(chunk_offset) + vec2(iid % chunk_size.x, iid / chunk_size.y);
  vec2 screen_size = ts.quad.zw;
  vec2 uv_pos      = vec2(tile % ts.columns, tile / ts.columns);
  vec2 uv_size     = 1.0f / vec2(ts.columns, ts.rows);
  // Draw Data
  pos =  screen_pos + mesh_pos  * screen_size;
  uv  = (uv_pos     + mesh_pos) * uv_size    ;
  tex =  ts.tex;
  gl_Position = projection * vec4(pos, 0, 1);
}