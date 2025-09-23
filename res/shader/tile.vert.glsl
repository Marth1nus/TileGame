#version 300 es
precision highp float;

in uint instance_tile;
in vec3 instance_pos;

uniform uvec3 atlas_tiles_size;
uniform mat4 projection;

// out vec3 fragment_pos;
out vec3 fragment_uv;

vec2 vertex_poss[4] = vec2[4](//
/**/ vec2(-0.5f, -0.5f), //
/**/ vec2(-0.5f, +0.5f), //
/**/ vec2(+0.5f, -0.5f), //
/**/ vec2(+0.5f, +0.5f)  //
);

void main()
{
  uint tile_id = instance_tile;
  if (tile_id == 0u)
  {
    gl_Position = vec4(0);
    return;
  }
  uvec3 vertex_pos = uvec3(vertex_poss[gl_VertexID & 3], 0);
  vec3 fragment_pos = instance_pos + vec3(vertex_pos);
  fragment_uv = vec3(uvec3(
  /**/ ((tile_id - 1u) % (atlas_tiles_size.x * atlas_tiles_size.y)) % atlas_tiles_size.x, 
  /**/ ((tile_id - 1u) % (atlas_tiles_size.x * atlas_tiles_size.y)) / atlas_tiles_size.x, 
  /**/ ((tile_id - 1u) / (atlas_tiles_size.x * atlas_tiles_size.y)) //
  ) + vertex_pos) / vec3(atlas_tiles_size);
  gl_Position = projection * vec4(fragment_pos, 1);
}
