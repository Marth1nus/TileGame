#version 300 es
#define TEXTURE_SLOTS {texture_slot_count}u

precision mediump float;
precision mediump sampler2D;

in vec2 pos;
in vec2 uv;
flat in uint tex;

uniform sampler2D textures[TEXTURE_SLOTS];

out vec4 color;

void main()
{
  color = texture(textures[tex], uv);
}