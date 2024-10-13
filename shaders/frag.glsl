#version 300 es
#define TEXTURE_SLOTS {texture_slot_count}

precision mediump float;
precision mediump sampler2D;

in vec2 mpos;
in vec2 pos;
in vec2 uv;
flat in uint tex;

uniform sampler2D textures[TEXTURE_SLOTS];

out vec4 color;

void main()
{
  color = texture(textures[tex], uv / 4.0f);
  vec2 m = abs(mpos - 0.5f); float o = 4.9f;
  if (m.x > o ||  m.y > o) color = vec4(1);
}