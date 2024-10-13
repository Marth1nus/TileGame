#version 300 es
#define TEXTURE_SLOTS {texture_slot_count}u

in vec2 mesh_uv_pos;
in vec2 instance_pos;
in vec2 instance_size;
in vec2 instance_uv_pos;
in vec2 instance_uv_size;
in uint instance_tex;
in uint tile;

uniform mat4 projection;
uniform bool use_tiles;
uniform uint columns;
struct tile_set { uint first, last, columns, rows, tex, padding[3]; };
uniform TILE_SETS { tile_set tile_sets[TEXTURE_SLOTS]; };

out vec2 mpos;
out vec2 pos;
out vec2 uv;
flat out uint tex;

void main()
{
  vec2 mesh_pos = vec2[4](
    vec2(0, 0), 
    vec2(0, 1), 
    vec2(1, 0), 
    vec2(1, 1)
  )[gl_VertexID];
  mpos = mesh_pos;
  if (use_tiles)
  {
    uint id = uint(gl_InstanceID);
    vec2 tile_pos = vec2(id % columns, id / columns);
    vec2 tile_size = vec2(1);

    id = tile;
    uint i = 0u;
    for (; i < TEXTURE_SLOTS; i++)
      if (tile_sets[i].first <= id && id <= tile_sets[i].last)
        break;
    tile_set ts = tile_sets[i];
    id -= ts.first;

    vec2 tile_uv_pos = vec2(id % ts.columns, id / ts.columns);
    vec2 tile_uv_size = 1.0f / vec2(uvec2(ts.columns, ts.rows));

    pos = (tile_pos    + mesh_pos) * tile_size   ;
    uv  = (tile_uv_pos + mesh_pos) * tile_uv_size;
    tex = ts.tex;
  }
  else
  {
    pos = instance_pos    + instance_size    * mesh_pos;
    uv  = instance_uv_pos + instance_uv_size * mesh_pos;
    tex = instance_tex;
  }
  gl_Position = projection * vec4(pos, 0, 1);
  gl_Position.y = -gl_Position.y;
}