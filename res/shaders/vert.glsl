#define TEXTURE_SLOTS 16u
#define WEBGL          0
// Evereything before here is ignored (defines added back at pre-compile-time)
#version 300 es

in vec2  mesh_pos;
in uint  tile_id;
in ivec4 chunk_quad;

struct tileset 
{ 
  uint first, last, columns, rows;
  uint tex, padding0, padding1, padding2;
  vec4 tile_quad;
};
layout(std140) uniform TILESETS { tileset tilesets[TEXTURE_SLOTS]; };
uniform uint tilesets_count;
uniform mat4 projection;

out vec2 mpos;
out vec2 pos;
out vec2 uv;
flat out uint tex;

void main()
{
  mpos = mesh_pos;
  if (tile_id == 0u)
  {
    gl_Position = vec4(0);
    return;
  }
  uint tile = tile_id + 1u;
  tileset ts;
  for (uint i = 0u; i < tilesets_count; i++)
  {
    ts = tilesets[i];
    if (ts.first <= tile && tile <= ts.last)
      break;
  }
  tile -= ts.first;
  uint iid = uint(gl_InstanceID) % uint(chunk_quad.z * chunk_quad.w);

  vec2 tile_pos     = ts.tile_quad.xy + vec2(chunk_quad.xy) + vec2(iid % uint(chunk_quad.z), iid / uint(chunk_quad.z));
  vec2 tile_size    = ts.tile_quad.zw;
  vec2 tile_uv_pos  = vec2(tile % ts.columns, tile / ts.columns);
  vec2 tile_uv_size = 1.0f / vec2(uvec2(ts.columns, ts.rows));

  pos =  tile_pos    + mesh_pos  * tile_size   ;
  uv  = (tile_uv_pos + mesh_pos) * tile_uv_size;
  tex = ts.tex;

  gl_Position   = projection * vec4(pos, 0, 1);
}