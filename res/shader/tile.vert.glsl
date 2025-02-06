#version 300 es
precision highp float;

in uint instance_tile;
in vec3 instance_pos;

uniform uvec2 chunk_size;
uniform uvec3 atlas_tiles_size;
uniform mat4 projection;

out vec3 fragment_pos;
out vec3 fragment_uv;

uvec2 vertex_poss[4] = uvec2[4](//
/**/ uvec2(0, 0), //
/**/ uvec2(1, 0), //
/**/ uvec2(0, 1), //
/**/ uvec2(1, 1)  //
);

void main()
{
  if (instance_tile == 0u)
  {
    gl_Position = vec4(0);
    return;
  }
  uvec3 vertex_pos = uvec3(vertex_poss[gl_VertexID & 3], 0);
  fragment_pos = instance_pos + vec3(
  /**/ (uint(gl_InstanceID) % (chunk_size.x * chunk_size.y)) % chunk_size.x, 
  /**/ (uint(gl_InstanceID) % (chunk_size.x * chunk_size.y)) / chunk_size.x, 0 //
  ) + vec3(vertex_pos);
  fragment_uv = vec3(uvec3(
  /**/ ((instance_tile - 1u) % (atlas_tiles_size.x * atlas_tiles_size.y)) % atlas_tiles_size.x, 
  /**/ ((instance_tile - 1u) % (atlas_tiles_size.x * atlas_tiles_size.y)) / atlas_tiles_size.x, 
  /**/ ((instance_tile - 1u) / (atlas_tiles_size.x * atlas_tiles_size.y)) //
  ) + vertex_pos) / vec3(atlas_tiles_size);
  gl_Position = projection * vec4(fragment_pos, 1);
}
