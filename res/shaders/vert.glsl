#version 300 es
#define TEXTURE_SLOTS ###u

in vec2  mesh_pos;
in vec2  instance_pos;
in vec2  instance_size;
in vec2  instance_uv_pos;
in vec2  instance_uv_size;
in uint  instance_tex;
in uint  tiles_tile;
in ivec4 tiles_chunk;

uniform mat4 projection;

uniform bool tiles_use;
uniform uint tilesets_count;
struct tileset 
{ 
  uint first, last, columns, rows,
      tex, padding0, padding1, padding2;
  vec4 tile_quad; // xy:tile_pos zw:tile_size 
};
layout(std140) uniform TILESETS { tileset tilesets[TEXTURE_SLOTS]; };

out vec2 mpos;
out vec2 pos;
out vec2 uv;
flat out uint tex;

void main()
{
  mpos = mesh_pos;
  if (tiles_use)
  {
    if (tiles_tile == 0u)
    {
      gl_Position = vec4(0);
      return;
    }
    ivec4 chunk   = tiles_chunk;
    uint  columns = uint(chunk.z);
    uint  iid     = uint(gl_InstanceID) % uint(chunk.z * chunk.w);
    uint  tile    = tiles_tile;
    uint  i       = 0u;

    for (; i < tilesets_count; i++)
      if (tilesets[i].first <= tile && tile <= tilesets[i].last)
        break;
    tileset ts = tilesets[i];
    tile -= ts.first;

    vec2 tile_pos     = vec2(iid % columns, iid / columns) + vec2(chunk.xy) + ts.tile_quad.xy;
    vec2 tile_size    = ts.tile_quad.zw;
    vec2 tile_uv_pos  = vec2(tile % ts.columns, tile / ts.columns);
    vec2 tile_uv_size = 1.0f / vec2(uvec2(ts.columns, ts.rows));

    if (ts.tex == 1u) // TODO: Impliment tileset offsets in map loading and remove this hardcoded fix
    {
      tile_pos += vec2(0,-1);
      tile_size = vec2(1,2);
    }

    pos =  tile_pos    + mesh_pos  * tile_size   ;
    uv  = (tile_uv_pos + mesh_pos) * tile_uv_size;
    tex = ts.tex;
  }
  else
  {
    pos = instance_pos    + instance_size    * mesh_pos;
    uv  = instance_uv_pos + instance_uv_size * mesh_pos;
    tex = instance_tex;
  }
  gl_Position   = projection * vec4(pos, 0, 1);
  gl_Position.y = -gl_Position.y;
}