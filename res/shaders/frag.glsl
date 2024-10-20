#version 300 es
#define TEXTURE_SLOTS {texture_slot_count}u

precision highp float;
precision highp sampler2D;

in vec2 mpos;
in vec2 pos;
in vec2 uv;
flat in uint tex;

uniform sampler2D textures[TEXTURE_SLOTS];

out vec4 color;

void main()
{
  vec4 tex_color = texture(textures[tex], uv); // Required for WebGL (Do not modify)
  color = tex_color;
  const float o = 0.00f; 
  if (mpos.x < o || 1.0f - o < mpos.x 
  ||  mpos.y < o || 1.0f - o < mpos.y) color = vec4(1);
}