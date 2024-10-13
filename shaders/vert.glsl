#version 300 es
#define TEXTURE_SLOTS {texture_slot_count}

layout(location = 0) in vec2 mesh_pos;
layout(location = 1) in vec2 mesh_uv_pos;
layout(location = 2) in vec2 instance_pos;
layout(location = 3) in vec2 instance_size;
layout(location = 4) in vec2 instance_uv_pos;
layout(location = 5) in vec2 instance_uv_size;
layout(location = 6) in uint instance_tex;
layout(location = 7) in uint tile_id;

uniform mat4 projection;
uniform bool use_tiles;

out vec2 pos;
out vec2 uv;
flat out uint tex;
out vec2 mpos;

void tile_main();

void main()
{
  mpos = mesh_pos;
  if (use_tiles) { tile_main(); return; }
  pos = instance_pos + instance_size * mesh_pos;
  uv = instance_uv_pos + instance_uv_size * mesh_uv_pos;
  tex = instance_tex;
  gl_Position = projection * vec4(pos, 0, 1);
  gl_Position.y *= -1.0f;
}

// =====================================================================================================================
// == Tiles ============================================================================================================
// =====================================================================================================================

struct tile_set { uint first, last, columns, tex; };
uniform TILE_SETS { tile_set tile_sets[TEXTURE_SLOTS]; };
uniform uint tile_chunk_columns;

void tile_main()
{
  uint tid = tile_id, tile_set_i = uint(0);
  for (uint i = uint(0); i < uint(TEXTURE_SLOTS); i++)
    if (tile_sets[i].first <= tid && tid <= tile_sets[i].last)
      tile_set_i = i;
  tile_set ts = tile_sets[tile_set_i];
  tid -= ts.first;
  vec2 uv_size = vec2(1.0f) / vec2(uvec2(ts.columns, (ts.last - ts.first + uint(1)) / ts.columns));
  uint iid = uint(gl_InstanceID);
  pos = vec2(uvec2(iid % tile_chunk_columns, iid / tile_chunk_columns)) + mesh_pos;
  uv  = vec2(uvec2(tid % ts.columns        , tid / ts.columns        )) + mesh_pos * uv_size;
  tex = ts.tex;
  gl_Position = projection * vec4(pos, 0, 1);
  gl_Position.y *= -1.0f;
}